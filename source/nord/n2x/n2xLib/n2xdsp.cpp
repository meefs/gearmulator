#include "n2xdsp.h"

#include "n2xhardware.h"

#include "dsp56kDebugger/debugger.h"
#include "dsp56kEmu/dspthread.h"
#include "hardwareLib/dspBootCode.h"
#include "mc68k/hdi08.h"

namespace n2x
{
	static constexpr dsp56k::TWord g_xyMemSize			= 0x010000;
	static constexpr dsp56k::TWord g_externalMemAddr	= 0x008000;
	static constexpr dsp56k::TWord g_pMemSize			= 0x004000;
	static constexpr dsp56k::TWord g_bootCodeBase		= 0x003f00;

	namespace
	{
		dsp56k::DefaultMemoryValidator g_memValidator;
	}
	DSP::DSP(Hardware& _hw, mc68k::Hdi08& _hdiUc, const uint32_t _index)
	: m_hardware(_hw)
	, m_hdiUC(_hdiUc)
	, m_index(_index)
	, m_name(_index ? "DSP B" : "DSP A")
	, m_memory(g_memValidator, g_pMemSize, g_xyMemSize, g_externalMemAddr)
	, m_dsp(m_memory, &m_periphX, &m_periphNop)
	, m_haltDSP(m_dsp)
	{
		if(!_hw.isValid())
			return;

		{
			auto& clock = m_periphX.getEsaiClock();
			auto& esai = m_periphX.getEsai();

			clock.setExternalClockFrequency(3'333'333); // schematic claims 1 MHz but we measured 10/3 Mhz

			constexpr auto samplerate = g_samplerate;
			constexpr auto clockMultiplier = 2;

			clock.setSamplerate(samplerate * clockMultiplier);

			clock.setClockSource(dsp56k::EsaiClock::ClockSource::Cycles);

			if(m_index == 0)
			{
				// DSP A = chip U2 = left on the schematic
				// Sends its audio to DSP B at twice the sample rate, it sends four words per frame
				clock.setEsaiDivider(&esai, 0);
			}
			else
			{
				// DSP B = chip U3 = right on the schematic
				// receives audio from DSP A at twice the sample rate
				// sends its audio to the DACs at regular sample rate
				clock.setEsaiDivider(&esai, 1, 0);
				clock.setEsaiCounter(&esai, -1, 0);
			}
		}

		auto config = m_dsp.getJit().getConfig();

		config.aguSupportBitreverse = true;
		config.linkJitBlocks = true;
		config.dynamicPeripheralAddressing = false;
#ifdef _DEBUG
		config.debugDynamicPeripheralAddressing = true;
#endif
		config.maxInstructionsPerBlock = 0;
		config.support16BitSCMode = true;
		config.dynamicFastInterrupts = true;

		m_dsp.getJit().setConfig(config);

		// fill P memory with something that reminds us if we jump to garbage
		for(dsp56k::TWord i=0; i<m_memory.sizeP(); ++i)
		{
			m_memory.set(dsp56k::MemArea_P, i, 0x000200);	// debug instruction
			m_dsp.getJit().notifyProgramMemWrite(i);
		}

		// rewrite bootloader to work at address g_bootCodeBase instead of $ff0000
		for(uint32_t i=0; i<std::size(hwLib::g_dspBootCode56362); ++i)
		{
			uint32_t code = hwLib::g_dspBootCode56362[i];
			if((hwLib::g_dspBootCode56362[i] & 0xffff00) == 0xff0000)
			{
				code = g_bootCodeBase | (hwLib::g_dspBootCode56362[i] & 0xff);
			}

			m_memory.set(dsp56k::MemArea_P, i + g_bootCodeBase, code);
			m_dsp.getJit().notifyProgramMemWrite(i + g_bootCodeBase);
		}

		// set OMR pins so that bootcode wants program data via HDI08 RX
		m_dsp.setPC(g_bootCodeBase);
		m_dsp.regs().omr.var |= OMR_MA | OMR_MB | OMR_MC | OMR_MD;
		
		hdi08().setRXRateLimit(0);

		m_periphX.getEsai().writeEmptyAudioIn(2);

		m_hdiUC.setRxEmptyCallback([&](const bool _needMoreData)
		{
			onUCRxEmpty(_needMoreData);
		});
		m_hdiUC.setWriteTxCallback([&](const uint32_t _word)
		{
			hdiTransferUCtoDSP(_word);
		});
		m_hdiUC.setWriteIrqCallback([&](const uint8_t _irq)
		{
			hdiSendIrqToDSP(_irq);
		});
		m_hdiUC.setReadIsrCallback([&](const uint8_t _isr)
		{
			return hdiUcReadIsr(_isr);
		});
		m_hdiUC.setInitHdi08Callback([&]
		{
			// clear init flag again immediately, code is waiting for it to happen
			m_hdiUC.icr(m_hdiUC.icr() & 0x7f);
			m_hdiUC.isr(m_hdiUC.isr() | mc68k::Hdi08::IsrBits::Txde | mc68k::Hdi08::IsrBits::Trdy);
		});

#if DSP56300_DEBUGGER
		if(!m_index)
			m_thread.reset(new dsp56k::DSPThread(dsp(), m_name.c_str(), std::make_shared<dsp56kDebugger::Debugger>(m_dsp)));
		else
#endif
		m_thread.reset(new dsp56k::DSPThread(dsp(), m_name.c_str()));

		m_thread->setLogToStdout(false);

		m_irqInterruptDone = dsp().registerInterruptFunc([this]
		{
			m_triggerInterruptDone.notify();
		});
	}

	void DSP::onUCRxEmpty(const bool _needMoreData)
	{
		if(_needMoreData)
		{
			hwLib::ScopedResumeDSP rB(m_hardware.getDSPB().getHaltDSP());

			while(dsp().hasPendingInterrupts())
				std::this_thread::yield();
		}
		hdiTransferDSPtoUC();
	}

	void DSP::hdiTransferUCtoDSP(const uint32_t _word)
	{
//		LOG('[' << m_name << "] toDSP writeRX=" << HEX(_word) << ", ucPC=" << HEX(m_hardware.getUC().getPrevPC()));
		hdi08().writeRX(&_word, 1);
	}

	void DSP::hdiSendIrqToDSP(const uint8_t _irq)
	{
		{
			dsp().injectExternalInterrupt(_irq);
			dsp().injectExternalInterrupt(m_irqInterruptDone);

			hwLib::ScopedResumeDSP rB(m_hardware.getDSPB().getHaltDSP());
			m_triggerInterruptDone.wait();
		}

		hdiTransferDSPtoUC();
	}

	uint8_t DSP::hdiUcReadIsr(uint8_t _isr)
	{
		hdiTransferDSPtoUC();

		// transfer DSP host flags HF2&3 to uc
		const auto hf23 = hdi08().readControlRegister() & 0x18;
		_isr &= ~0x18;
		_isr |= hf23;
		// always ready to receive more data
		_isr |= mc68k::Hdi08::IsrBits::Trdy;
		return _isr;
	}

	bool DSP::hdiTransferDSPtoUC()
	{
		if (m_hdiUC.canReceiveData() && hdi08().hasTX())
		{
			const auto v = hdi08().readTX();
//			LOG('[' << m_name << "] HDI dsp2UC=" << HEX(v));
			m_hdiUC.writeRx(v);
			return true;
		}
		return false;
	}
}
