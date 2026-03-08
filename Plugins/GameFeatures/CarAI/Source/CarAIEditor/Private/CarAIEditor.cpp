#include "CarAIEditor.h"

#include "Editor.h"
#include "LevelEditor.h"
#include "ToolMenus.h"
#include "EditorUtilitySubsystem.h"
#include "EditorUtilityWidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "CarAIEditor"

void FCarAIEditor::StartupModule()
{
	// Soft references resolved at open-time to avoid loading assets at startup.
	CurriculumWidget = TSoftObjectPtr<UEditorUtilityWidgetBlueprint>(
		FSoftObjectPath(TEXT("/CarAI/Editor/EUW_AICarCurriculum.EUW_AICarCurriculum"))
	);
	NeatTrainingWidget = TSoftObjectPtr<UEditorUtilityWidgetBlueprint>(
		FSoftObjectPath(TEXT("/CarAI/Editor/EUW_NeatTraining.EUW_NeatTraining"))
	);

	// Register menus after ToolMenus is ready.
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(
			this, &FCarAIEditor::RegisterMenus
		)
	);
}

void FCarAIEditor::ShutdownModule()
{
	if (UToolMenus::Get())
	{
		UToolMenus::UnregisterOwner(this);
	}
}

void FCarAIEditor::RegisterMenus()
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	if (!ToolMenus)
	{
		return;
	}

	UToolMenu* Menu = ToolMenus->ExtendMenu("LevelEditor.MainMenu.Window");
	FToolMenuSection& Section = Menu->AddSection(
		"CarAI",
		LOCTEXT("CarAISection", "Car AI")
	);

	Section.AddMenuEntry(
		"OpenCarAICurriculum",
		LOCTEXT("OpenCarAICurriculum", "CarAI Curriculum"),
		LOCTEXT("OpenCarAICurriculum_Tooltip", "Open the CarAI Curriculum Editor"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FCarAIEditor::OpenCurriculum))
	);

	Section.AddMenuEntry(
		"OpenNeatTraining",
		LOCTEXT("OpenNeatTraining", "NEAT Training"),
		LOCTEXT("OpenNeatTraining_Tooltip", "Open the NEAT Training editor utility widget"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FCarAIEditor::OpenNeatTraining))
	);
}

void FCarAIEditor::OpenCurriculum()
{
	if (!CurriculumWidget.IsValid())
	{
		CurriculumWidget.LoadSynchronous();
	}

	if (!CurriculumWidget.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[CarAIEditor] CurriculumWidget not found at /CarAI/Editor/EUW_AICarCurriculum"));
		return;
	}

	if (UEditorUtilitySubsystem* Subsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>())
	{
		Subsystem->SpawnAndRegisterTab(CurriculumWidget.Get());
	}
}

void FCarAIEditor::OpenNeatTraining()
{
	if (!NeatTrainingWidget.IsValid())
	{
		NeatTrainingWidget.LoadSynchronous();
	}

	if (!NeatTrainingWidget.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[CarAIEditor] EUW_NeatTraining not found at /CarAI/Editor/EUW_NeatTraining. "
			"Ensure the asset exists and is reparented to UNeatTrainingEditorWidget."));
		return;
	}

	if (UEditorUtilitySubsystem* Subsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>())
	{
		UE_LOG(LogTemp, Log, TEXT("[CarAIEditor] Opening NEAT Training editor utility widget"));
		Subsystem->SpawnAndRegisterTab(NeatTrainingWidget.Get());
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCarAIEditor, CarAIEditor)
