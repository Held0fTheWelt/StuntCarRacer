#pragma once

#include "Modules/ModuleManager.h"

class FCarAIEditor : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** Menü-Registrierung */
	void RegisterMenus();

	/** Öffnet das Curriculum-Tool */
	void OpenCurriculum();

private:
	/** Referenz auf das EditorUtilityWidget-Blueprint */
	TSoftObjectPtr<class UEditorUtilityWidgetBlueprint> CurriculumWidget;
};
