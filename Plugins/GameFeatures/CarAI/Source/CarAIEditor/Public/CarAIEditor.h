#pragma once

#include "Modules/ModuleManager.h"

class FCarAIEditor : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** Register entries under Window > Car AI in the Level Editor menu bar. */
	void RegisterMenus();

	/** Opens the CarAI Curriculum editor utility widget. */
	void OpenCurriculum();

	/** Opens the NEAT Training editor utility widget. */
	void OpenNeatTraining();

private:
	/** Soft reference to the Curriculum EditorUtilityWidget Blueprint asset. */
	TSoftObjectPtr<class UEditorUtilityWidgetBlueprint> CurriculumWidget;

	/** Soft reference to the NEAT Training EditorUtilityWidget Blueprint asset. */
	TSoftObjectPtr<class UEditorUtilityWidgetBlueprint> NeatTrainingWidget;
};
