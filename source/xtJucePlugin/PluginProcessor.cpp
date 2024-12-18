#include "PluginProcessor.h"

#include "PluginEditorState.h"
#include "xtController.h"

// ReSharper disable once CppUnusedIncludeDirective
#include "BinaryData.h"
#include "jucePluginLib/processorPropertiesInit.h"

#include "xtLib/xtDevice.h"

#include "xtLib/xtRomLoader.h"

class Controller;

namespace
{
	juce::PropertiesFile::Options getOptions()
	{
		juce::PropertiesFile::Options opts;
		opts.applicationName = "DSP56300EmulatorXenia";
		opts.filenameSuffix = ".settings";
		opts.folderName = "DSP56300EmulatorXenia";
		opts.osxLibrarySubFolder = "Application Support/DSP56300EmulatorXenia";
		return opts;
	}
}

namespace xtJucePlugin
{
	AudioPluginAudioProcessor::AudioPluginAudioProcessor() :
	    Processor(BusesProperties()
	                   .withInput("Input", juce::AudioChannelSet::stereo(), true)
	                   .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#if JucePlugin_IsSynth
	                   .withOutput("Out 2", juce::AudioChannelSet::stereo(), true)
#endif
		, getOptions(), pluginLib::initProcessorProperties())
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
		synthLib::DeviceCreateParams p;
		getRemoteDeviceParams(p);
		return new xt::Device(p);
	}

	void AudioPluginAudioProcessor::getRemoteDeviceParams(synthLib::DeviceCreateParams& _params) const
	{
		Processor::getRemoteDeviceParams(_params);

		const auto rom = xt::RomLoader::findROM();

		if(rom.isValid())
		{
			_params.romData = rom.getData();
			_params.romName = rom.getFilename();
		}
	}

	pluginLib::Controller* AudioPluginAudioProcessor::createController()
	{
		return new Controller(*this);
	}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new xtJucePlugin::AudioPluginAudioProcessor();
}
