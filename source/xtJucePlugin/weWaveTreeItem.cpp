#include "weWaveTreeItem.h"

#include "weWaveCategoryTreeItem.h"
#include "weWaveDesc.h"
#include "xtEditor.h"
#include "xtWaveEditor.h"

#include "juceUiLib/messageBox.h"

#include "synthLib/midiToSysex.h"

namespace xtJucePlugin
{
	WaveTreeItem::WaveTreeItem(WaveEditor& _editor, const WaveCategory _category, const xt::WaveId _waveIndex)
		: m_editor(_editor)
		, m_category(_category)
		, m_waveIndex(_waveIndex)
	{
		m_onWaveChanged.set(m_editor.getData().onWaveChanged, [this](const xt::WaveId& _waveIndex)
		{
			onWaveChanged(_waveIndex);
		});

		setText(getWaveName(_waveIndex));
	}

	void WaveTreeItem::paintWave(const xt::WaveData& _data, juce::Graphics& _g, const int _x, const int _y, const int _width, const int _height, const juce::Colour& _colour)
	{
		_g.setColour(_colour);

		const float scaleX = static_cast<float>(_width)  / static_cast<float>(_data.size());
		const float scaleY = static_cast<float>(_height) / static_cast<float>(256);

		for(uint32_t x=1; x<_data.size(); ++x)
		{
			const auto x0 = static_cast<float>(x - 1) * scaleX + static_cast<float>(_x);
			const auto x1 = static_cast<float>(x    ) * scaleX + static_cast<float>(_x);

			const auto y0 = static_cast<float>(255 - (_data[x - 1] + 128)) * scaleY + static_cast<float>(_y);
			const auto y1 = static_cast<float>(255 - (_data[x    ] + 128)) * scaleY + static_cast<float>(_y);

			_g.drawLine(x0, y0, x1, y1);
		}
	}

	std::string WaveTreeItem::getWaveName(const xt::WaveId _waveIndex)
	{
		if(!xt::wave::isValidWaveIndex(_waveIndex.rawId()))
			return {};
		const auto category = getCategory(_waveIndex);
		return WaveCategoryTreeItem::getCategoryName(category) + ' ' + std::to_string(_waveIndex.rawId());
	}

	WaveCategory WaveTreeItem::getCategory(const xt::WaveId _waveIndex)
	{
		if(_waveIndex.rawId() < xt::wave::g_romWaveCount)
			return WaveCategory::Rom;
		if(_waveIndex.rawId() >= xt::wave::g_firstRamWaveIndex && _waveIndex.rawId() < xt::wave::g_firstRamWaveIndex + xt::wave::g_ramWaveCount)
			return WaveCategory::User;
		return WaveCategory::Invalid;
	}

	void WaveTreeItem::itemSelectionChanged(bool isNowSelected)
	{
		TreeItem::itemSelectionChanged(isNowSelected);

		if(isNowSelected)
			m_editor.setSelectedWave(m_waveIndex);
	}

	juce::var WaveTreeItem::getDragSourceDescription()
	{
		auto* desc = new WaveDesc(m_editor);

		desc->waveIds = {m_waveIndex};
		desc->source = WaveDescSource::WaveList;

		desc->fillData(m_editor.getData());

		return desc;
	}

	bool WaveTreeItem::isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails& dragSourceDetails)
	{
		if(xt::wave::isReadOnly(m_waveIndex))
			return false;
		const auto* waveDesc = WaveDesc::fromDragSource(dragSourceDetails);
		if(!waveDesc)
			return false;
		if(waveDesc->waveIds.size() != 1)
			return false;
		return true;
	}

	void WaveTreeItem::itemDropped(const juce::DragAndDropTarget::SourceDetails& dragSourceDetails, int insertIndex)
	{
		TreeItem::itemDropped(dragSourceDetails, insertIndex);
		if(xt::wave::isReadOnly(m_waveIndex))
			return;
		const auto* waveDesc = WaveDesc::fromDragSource(dragSourceDetails);
		if(!waveDesc)
			return;
		auto& data = m_editor.getData();

		if(data.copyWave(m_waveIndex, waveDesc->waveIds.front()))
		{
			setSelected(true, true, juce::dontSendNotification);
			data.sendWaveToDevice(m_waveIndex);
		}
	}

	bool WaveTreeItem::isInterestedInFileDrag(const juce::StringArray& files)
	{
		if(xt::wave::isReadOnly(m_waveIndex))
			return false;

		if (files.size() != 1)
			return false;

		const auto& f = files[0];

		if(f.endsWithIgnoreCase(".mid") || f.endsWithIgnoreCase(".syx") || f.endsWithIgnoreCase(".wav"))
			return true;

		return TreeItem::isInterestedInFileDrag(files);
	}

	void WaveTreeItem::filesDropped(const juce::StringArray& files, int insertIndex)
	{
		if(xt::wave::isReadOnly(m_waveIndex))
			return;

		if (files.isEmpty())
			return;

		if (files[0].endsWithIgnoreCase(".wav"))
		{
			if (auto wave = m_editor.importWaveFile(files[0].toStdString()))
			{
				m_editor.getData().setWave(m_waveIndex, *wave);
				m_editor.getData().sendWaveToDevice(m_waveIndex);
			}
			return;
		}

		const auto errorTitle = m_editor.getEditor().getProcessor().getProperties().name + " - Error";

		std::map<xt::WaveId, xt::WaveData> waves;
		std::map<xt::TableId, xt::TableData> tables;
		m_editor.filesDropped(waves, tables, files);

		if (waves.empty())
		{
			if (tables.size() == 1)
				genericUI::MessageBox::showOk(juce::AlertWindow::WarningIcon, errorTitle, "This file doesn't contain a Wave but a Control Table, please drop on a User Table slot.");

			return;
		}

		m_editor.getData().setWave(m_waveIndex, waves.begin()->second);
		m_editor.getData().sendWaveToDevice(m_waveIndex);
	}

	std::vector<std::vector<uint8_t>> WaveTreeItem::getSysexFromFiles(const juce::StringArray& _files)
	{
		std::vector<std::vector<uint8_t>> sysex;

		for(const auto& file : _files)
		{
			std::vector<std::vector<uint8_t>> s;
			synthLib::MidiToSysex::extractSysexFromFile(s, file.toStdString());
			sysex.insert(sysex.end(), s.begin(), s.end());
		}

		return sysex;
	}

	void WaveTreeItem::itemClicked(const juce::MouseEvent& _mouseEvent)
	{
		if(!_mouseEvent.mods.isPopupMenu())
		{
			TreeItem::itemClicked(_mouseEvent);
			return;
		}

		juce::PopupMenu menu;

		const auto selectedTableId = m_editor.getSelectedTable();

		if(selectedTableId.isValid())
		{
			const auto subMenuCT = m_editor.createCopyToSelectedTableMenu(m_waveIndex);
			menu.addSubMenu("Copy to current Control Table...", subMenuCT);
		}

		const auto subMenuUW = WaveEditor::createRamWavesPopupMenu([this](const xt::WaveId _dest)
		{
			if (m_editor.getData().copyWave(_dest, m_waveIndex))
			{
				m_editor.getData().sendWaveToDevice(_dest);
				m_editor.setSelectedWave(_dest);
			}
		});
		menu.addSubMenu("Copy to User Wave...", subMenuUW);
		menu.addSeparator();

		if (auto wave = m_editor.getData().getWave(m_waveIndex))
		{
			auto w = *wave;

			juce::PopupMenu exportMenu;

			if (xt::wave::isReadOnly(m_waveIndex))
			{
				// we cannot export a .mid or .syx that is a rom location as the hardware cannot import it
				auto subMenuSyx = WaveEditor::createRamWavesPopupMenu([this, w](const xt::WaveId _id)
				{
					m_editor.exportAsSyx(_id, w);
				});
				exportMenu.addSubMenu(".syx", subMenuSyx);
				auto subMenuMid = WaveEditor::createRamWavesPopupMenu([this, w](const xt::WaveId _id)
				{
					m_editor.exportAsMid(_id, w);
				});
				exportMenu.addSubMenu(".mid", subMenuMid);
			}
			else
			{
				exportMenu.addItem(".syx", [this, w]
				{
					m_editor.exportAsSyx(m_waveIndex, w);
				});
				exportMenu.addItem(".mid", [this, w]
				{
					m_editor.exportAsMid(m_waveIndex, w);
				});
			}
			exportMenu.addItem(".wav", [this, w]
			{
				m_editor.exportAsWav(w);
			});
			menu.addSubMenu("Export as...", exportMenu);
		}

		if (!xt::wave::isReadOnly(m_waveIndex))
		{
			menu.addSeparator();
			menu.addItem("Import .syx/.mid...", [this]
			{
				m_editor.selectImportFile([this](const juce::String& _filename)
				{
					juce::StringArray files;
					files.add(_filename);
					filesDropped(files, 0);
				});
			});
		}

		menu.showMenuAsync({});
	}

	void WaveTreeItem::onWaveChanged(const xt::WaveId _index) const
	{
		if(_index != m_waveIndex)
			return;
		onWaveChanged();
	}

	void WaveTreeItem::onWaveChanged() const
	{
		repaintItem();
	}

	void WaveTreeItem::paintItem(juce::Graphics& g, int width, int height)
	{
		if(const auto wave = m_editor.getData().getWave(m_waveIndex))
			paintWave(*wave, g, width>>1, 0, width>>1, height, juce::Colour(0xffffffff));

		TreeItem::paintItem(g, width, height);
	}
}
