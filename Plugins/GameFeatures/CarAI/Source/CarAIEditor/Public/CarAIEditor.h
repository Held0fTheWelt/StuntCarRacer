#pragma once

#include "Delegates/Delegate.h"
#include "Modules/ModuleManager.h"

class UNeatTrainingEditorWidget;
class UWorld;

class FCarAIEditor : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Returns the CarAIEditor module instance. Used by the NEAT training widget to register for PIE lifecycle. */
	static FCarAIEditor& Get();

	/** Register the NEAT training widget so the module can notify it on PIE start/end. Only one widget at a time. */
	void RegisterNeatTrainingWidget(UNeatTrainingEditorWidget* Widget);
	/** Unregister the NEAT training widget. Call when the widget is destroyed. */
	void UnregisterNeatTrainingWidget(UNeatTrainingEditorWidget* Widget);

private:
	/** Register entries under Window > Car AI in the Level Editor menu bar. */
	void RegisterMenus();

	/** Opens the CarAI Curriculum editor utility widget. */
	void OpenCurriculum();

	/** Opens the NEAT Training editor utility widget. */
	void OpenNeatTraining();

	/** PIE lifecycle: called when PIE has started and world is available. */
	void OnPostPIEStarted(bool bSimulate);
	/** PIE lifecycle: called when PIE is ending. */
	void OnEndPIE(bool bSimulate);

private:
	/** Soft reference to the Curriculum EditorUtilityWidget Blueprint asset. */
	TSoftObjectPtr<class UEditorUtilityWidgetBlueprint> CurriculumWidget;

	/** Soft reference to the NEAT Training EditorUtilityWidget Blueprint asset. */
	TSoftObjectPtr<class UEditorUtilityWidgetBlueprint> NeatTrainingWidget;

	/** Currently registered NEAT training widget (receives PIE start/end). Cleared on PIE end and when widget unregisters. */
	TWeakObjectPtr<UNeatTrainingEditorWidget> RegisteredNeatWidget;

	/** Delegate handles for PIE lifecycle; removed in ShutdownModule. */
	FDelegateHandle PostPIEStartedHandle;
	FDelegateHandle EndPIEHandle;
};
