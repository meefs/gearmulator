#include "n2xfrontpanel.h"

#include <cassert>
#include <cstring>	// memcpy

#include "dsp56kEmu/logging.h"

namespace n2x
{
	template class FrontPanelCS<g_frontPanelAddressCS4>;
	template class FrontPanelCS<g_frontPanelAddressCS6>;

	template <uint32_t Base> FrontPanelCS<Base>::FrontPanelCS(FrontPanel& _fp): m_panel(_fp)
	{
	}

	FrontPanelCS4::FrontPanelCS4(FrontPanel& _fp) : FrontPanelCS(_fp)
	{
	}

	uint8_t FrontPanelCS4::read8(mc68k::PeriphAddress _addr)
	{
		const auto knobType = m_panel.cs6().getKnobType();

		switch (knobType)
		{
		case KnobType::PitchBend:
		case KnobType::ModWheel:		return 0;	// pretend we're a rack unit
		case KnobType::MasterVol:		return 0xff;
		case KnobType::AmpGain:			return 0xff;

/*		case KnobType::Osc1Fm:			return 0;
		case KnobType::Porta:			return 0;
		case KnobType::Lfo2Rate:		return 0x20;
		case KnobType::Lfo1Rate:		return 0x20;
		case KnobType::ModEnvAmt:		return 0;
		case KnobType::ModEnvD:			return 0;
		case KnobType::ModEnvA:			return 0;
		case KnobType::AmpEnvD:			return 0;
		case KnobType::FilterFreq:		return 0xff;
		case KnobType::FilterEnvA:		return 0;
		case KnobType::AmpEnvA:			return 0;
		case KnobType::OscMix:			return 0x80;
		case KnobType::Osc2Fine:		return 0x80;
		case KnobType::Lfo1Amount:		return 0;
		case KnobType::OscPW:			return 0x40;
		case KnobType::AmpGain:			return 0xff;
		case KnobType::FilterEnvR:		return 0x30;
		case KnobType::AmpEnvR:			return 0x30;
		case KnobType::FilterEnvAmt:	return 0;
		case KnobType::FilterEnvS:		return 0xff;
		case KnobType::AmpEnvS:			return 0xff;
		case KnobType::FilterReso:		return 0x10;
		case KnobType::FilterEnvD:		return 0;
		case KnobType::ExpPedal:		return 0;
		case KnobType::Lfo2Amount:		return 0;
		case KnobType::Osc2Semi:		return 0x80;
*/		default:
			return 0x80;
		}
	}

	FrontPanelCS6::FrontPanelCS6(FrontPanel& _fp) : FrontPanelCS(_fp)
	{
	}

	void FrontPanelCS6::write8(const mc68k::PeriphAddress _addr, const uint8_t _val)
	{
//		LOG("Write CS6 " << HEXN(_addr - base(),2) << " = " << HEXN(_val,2));

		switch (static_cast<uint32_t>(_addr))
		{
			case g_frontPanelAddressCS6 + 0x8:
				m_ledLatch8 = _val;
				break;
			case g_frontPanelAddressCS6 + 0xa:
				m_ledLatch10 = _val;
				if(m_ledLatch10 & (1<<7))
				{
					m_lcds[2] = m_ledLatch8;
					printLCD();
				}
				else
				{
//					LOG("Read pot " << HEXN(_val, 2));
					m_selectedKnob = static_cast<KnobType>(_val);
				}
				break;
			case g_frontPanelAddressCS6 + 0xc:
				{
					bool gotLCDs = false;

					m_ledLatch12 = _val;

					if(m_ledLatch12 & (1<<6))
					{
						m_lcds[0] = m_ledLatch8;
						gotLCDs = true;
					}
					if(m_ledLatch12 & (1<<7))
					{
						m_lcds[1] = m_ledLatch8;
						gotLCDs = true;
					}

					if(gotLCDs)
						printLCD();
				}
				break;
		}
		FrontPanelCS::write8(_addr, _val);
	}

	static uint32_t g_counter = 0;
	constexpr uint32_t g_len = 4096;
	constexpr uint32_t g_threshold = 512;
	static bool wasPressed = false;
	static bool wasReleased = false;

	uint8_t FrontPanelCS6::read8(mc68k::PeriphAddress _addr)
	{
		++g_counter;
		g_counter &= (g_len - 1);
		auto press = g_counter >= (g_len - g_threshold);

		if(press)
			wasPressed = true;
		else if(!press && wasPressed)
			wasReleased = true;
		if(wasReleased)
			press = false;

		const auto a = static_cast<uint32_t>(_addr);
		switch (a)
		{
		case g_frontPanelAddressCS6:
			return 0xff;
		case g_frontPanelAddressCS6 + 2:
			if(press)
			{
				constexpr auto bt = static_cast<uint8_t>(ButtonType::OscSync) & 0xff;
				return 0xff ^ bt;
			}
			return 0xff;
		case g_frontPanelAddressCS6 + 4:
			return 0xff;
		case g_frontPanelAddressCS6 + 6:
			{
				if(press)
				{
					constexpr auto bt = static_cast<uint8_t>(ButtonType::ModwheelDest) & 0xff;
					return 0xff ^ bt;
				}
				return 0xff;
			}
		}
		return FrontPanelCS::read8(_addr);
	}

	static uint32_t g_count = 0;

	void FrontPanelCS6::printLCD()
	{
		++g_count;
		if(g_count & 0x7f)
			return;
		/*
		 --    --    --
		|  |  |  |  |  |
		 --    --    --
		|  |  |  |  |  |
		 --    --    --
		*/

		using Line = std::array<char, 16>;
		std::array<Line,5> buf;

		for (auto& b : buf)
		{
			b.fill(' ');
		}

		int off = 0;

		auto drawH = [&](int _x, const int _y, const bool _set)
		{
			_x += off;
			buf[_y][_x  ] = _set ? '-' : ' ';
			buf[_y][_x+1] = _set ? '-' : ' ';
		};

		auto drawV = [&](int _x, const int _y, const bool _set)
		{
			_x += off;
			buf[_y][_x] = _set ? '|' : ' ';
		};

		for(auto i=0; i<3; ++i)
		{
			auto bt = [&](const uint32_t _bit)
			{
				return !(m_lcds[i] & (1<<_bit));
			};

			drawH(1, 0, bt(7));
			drawH(1, 2, bt(1));
			drawH(1, 4, bt(4));

			drawV(0, 1, bt(2));
			drawV(3, 1, bt(6));
			drawV(0, 3, bt(3));
			drawV(3, 3, bt(5));

			off += 6;
		}

		buf[4][4] = (m_lcds[0] & (1<<0)) ? ' ' : '*';

		char message[(sizeof(Line) + 1) * buf.size() + 1];
		size_t i=0;
		for (const auto& line : buf)
		{
			memcpy(&message[i], line.data(), line.size());
			i += line.size();
			message[i++] = '\n';
		}
		assert(i == std::size(message)-1);
		message[i] = 0;

		LOG("LCD:\n" << message);
	}

	FrontPanel::FrontPanel(): m_cs4(*this), m_cs6(*this)
	{
	}
}
