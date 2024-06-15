#pragma once

#include "juce_gui_basics/juce_gui_basics.h"

namespace xtJucePlugin
{
	class WaveEditor;
	class Editor;

	class Tree : public juce::TreeView
	{
	public:
		explicit Tree(WaveEditor& _editor);
		~Tree();

		WaveEditor& getWaveEditor() const { return m_editor; }

	private:
		WaveEditor& m_editor;

		void paint(juce::Graphics& _g) override;
	};
}
