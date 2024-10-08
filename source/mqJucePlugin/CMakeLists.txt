cmake_minimum_required(VERSION 3.15)
project(jucePlugin VERSION ${CMAKE_PROJECT_VERSION}) 

set(SOURCES
	parameterDescriptions_mq.json
    PluginEditorState.cpp PluginEditorState.h
    PluginProcessor.cpp PluginProcessor.h
	mqController.cpp mqController.h
	mqEditor.cpp mqEditor.h
	mqFrontPanel.cpp mqFrontPanel.h
	mqLcd.cpp mqLcd.h
	mqLcdBase.cpp mqLcdBase.h
	mqLcdText.cpp mqLcdText.h
	mqPartButton.cpp mqPartButton.h
	mqPartSelect.cpp mqPartSelect.h
	mqPatchManager.cpp mqPatchManager.h
	mqPatchBrowser.h
)

# https://forum.juce.com/t/help-needed-using-binarydata-with-cmake-juce-6/40486
# "This might be because the BinaryData files are generated during the build, so the IDE may not be able to find them until the build has been run once (and even then, some IDEs might need a bit of a nudge to re-index the binary directory…)"
SET(ASSETS "parameterDescriptions_mq.json")

include(skins/mqFrontPanel/assets.cmake)
include(skins/mqDefault/assets.cmake)

juce_add_binary_data(mqJucePlugin_BinaryData SOURCES ${ASSETS} ${ASSETS_mqFrontPanel} ${ASSETS_mqDefault})

createJucePluginWithFX(mqJucePlugin "Vavra" "Tmqs" "Tmqf" mqJucePlugin_BinaryData mqLib)
