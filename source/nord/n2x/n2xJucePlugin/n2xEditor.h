#pragma once

#include "jucePluginEditorLib/pluginEditor.h"

class Controller;

namespace jucePluginEditorLib
{
	class FocusedParameter;
	class Processor;
}

namespace pluginLib
{
	class ParameterBinding;
}

namespace n2xJucePlugin
{
	class PatchManager;

	class Editor final : public jucePluginEditorLib::Editor
	{
	public:
		Editor(jucePluginEditorLib::Processor& _processor, pluginLib::ParameterBinding& _binding, std::string _skinFolder, const std::string& _jsonFilename);
		~Editor() override;

		Editor(Editor&&) = delete;
		Editor(const Editor&) = delete;
		Editor& operator = (Editor&&) = delete;
		Editor& operator = (const Editor&) = delete;

		static const char* findEmbeddedResource(const std::string& _filename, uint32_t& _size);
		const char* findResourceByFilename(const std::string& _filename, uint32_t& _size) override;
		std::pair<std::string, std::string> getDemoRestrictionText() const override;

		Controller& getN2xController() const { return m_controller; }

	private:
		Controller& m_controller;
		pluginLib::ParameterBinding& m_parameterBinding;
	};
}
