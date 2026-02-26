#include "CarAIEditor.h"

#include "Editor.h"
#include "LevelEditor.h"

#include "ToolMenus.h"
#include "EditorUtilitySubsystem.h"
#include "EditorUtilityWidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "CarAIEditor"

void FCarAIEditor::StartupModule()
{
	// Pfad zu deinem EditorUtilityWidget-Blueprint
	CurriculumWidget = TSoftObjectPtr<UEditorUtilityWidgetBlueprint>(
		FSoftObjectPath(TEXT("/CarAI/Editor/EUW_AICarCurriculum.EUW_AICarCurriculum"))
	);

	// Menüs erst registrieren, wenn ToolMenus bereit sind
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
		FUIAction(
			FExecuteAction::CreateRaw(this, &FCarAIEditor::OpenCurriculum)
		)
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
		UE_LOG(LogTemp, Error, TEXT("CarAIEditor: CurriculumWidget not found"));
		return;
	}

	if (UEditorUtilitySubsystem* Subsystem =
		GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>())
	{
		Subsystem->SpawnAndRegisterTab(CurriculumWidget.Get());
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCarAIEditor, CarAIEditor)
