#include "PluginProcessor.h"
#include "PluginEditorState.h"

#include "mqController.h"

#include "jucePluginLib/processor.h"
#include "mqLib/device.h"

class Controller;

namespace
{
	juce::PropertiesFile::Options getOptions()
	{
		juce::PropertiesFile::Options opts;
		opts.applicationName = "DSP56300EmulatorVavra";
		opts.filenameSuffix = ".settings";
		opts.folderName = "DSP56300EmulatorVavra";
		opts.osxLibrarySubFolder = "Application Support/DSP56300EmulatorVavra";
		return opts;
	}
}

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor() :
    Processor(BusesProperties()
                   .withInput("Input", juce::AudioChannelSet::stereo(), true)
                   .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#if JucePlugin_IsSynth
                   .withOutput("Out 2", juce::AudioChannelSet::stereo(), true)
                   .withOutput("Out 3", juce::AudioChannelSet::stereo(), true)
#endif
	, getOptions(), pluginLib::Processor::Properties{JucePlugin_Name, JucePlugin_IsSynth, JucePlugin_WantsMidiInput, JucePlugin_ProducesMidiOutput, JucePlugin_IsMidiEffect})
{
	getController();
	const auto latencyBlocks = getConfig().getIntValue("latencyBlocks", static_cast<int>(getPlugin().getLatencyBlocks()));
	Processor::setLatencyBlocks(latencyBlocks);
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
	destroyEditorState();
}

jucePluginEditorLib::PluginEditorState* AudioPluginAudioProcessor::createEditorState()
{
	return new PluginEditorState(*this);
}
synthLib::Device* AudioPluginAudioProcessor::createDevice()
{
	return new mqLib::Device();
}

pluginLib::Controller* AudioPluginAudioProcessor::createController()
{
	return new Controller(*this);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}
