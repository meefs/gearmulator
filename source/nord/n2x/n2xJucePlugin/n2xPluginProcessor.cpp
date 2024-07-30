#include "n2xPluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>

#include "n2xController.h"
#include "n2xPluginEditorState.h"

#include "jucePluginLib/processor.h"

#include "n2xLib/n2xdevice.h"

class Controller;

namespace
{
	juce::PropertiesFile::Options getOptions()
	{
		juce::PropertiesFile::Options opts;
		opts.applicationName = "DSP56300EmulatorNodalRed";
		opts.filenameSuffix = ".settings";
		opts.folderName = "DSP56300EmulatorNodalRed";
		opts.osxLibrarySubFolder = "Application Support/DSP56300EmulatorNodalRed";
		return opts;
	}
}

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor() :
    Processor(BusesProperties()
                   .withOutput("Out AB", juce::AudioChannelSet::stereo(), true)
                   .withOutput("Out CD", juce::AudioChannelSet::stereo(), true)
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
	return new n2x::Device();
}

pluginLib::Controller* AudioPluginAudioProcessor::createController()
{
	return new Controller(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}
