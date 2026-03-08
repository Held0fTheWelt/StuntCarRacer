#include "CarAIEditor.h"
#include "UI/NeatTrainingEditorWidget.h"
#include "Logging.h"

#include "Editor.h"
#include "LevelEditor.h"
#include "ToolMenus.h"
#include "EditorUtilitySubsystem.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "Modules/ModuleManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "CarAIEditor"

FCarAIEditor& FCarAIEditor::Get()
{
	return FModuleManager::LoadModuleChecked<FCarAIEditor>("CarAIEditor");
}

void FCarAIEditor::StartupModule()
{
	// Identify running build so we can confirm behavior comes from current source (see BUILD.md).
	UE_LOG(LogCarAIEditor, Display, TEXT("[CarAI] CarAIEditor module loaded (plugin source build)."));

	// Soft references resolved at open-time to avoid loading assets at startup.
	CurriculumWidget = TSoftObjectPtr<UEditorUtilityWidgetBlueprint>(
		FSoftObjectPath(TEXT("/CarAI/Editor/EUW_AICarCurriculum.EUW_AICarCurriculum"))
	);
	NeatTrainingWidget = TSoftObjectPtr<UEditorUtilityWidgetBlueprint>(
		FSoftObjectPath(TEXT("/CarAI/Editor/EUW_NeatTraining.EUW_NeatTraining"))
	);

	// PIE lifecycle: bind once; unbind in ShutdownModule to avoid duplicate binding and leaks.
	PostPIEStartedHandle = FEditorDelegates::PostPIEStarted.AddRaw(this, &FCarAIEditor::OnPostPIEStarted);
	EndPIEHandle = FEditorDelegates::EndPIE.AddRaw(this, &FCarAIEditor::OnEndPIE);

	// Register menus after ToolMenus is ready.
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(
			this, &FCarAIEditor::RegisterMenus
		)
	);
}

void FCarAIEditor::ShutdownModule()
{
	FEditorDelegates::PostPIEStarted.Remove(PostPIEStartedHandle);
	FEditorDelegates::EndPIE.Remove(EndPIEHandle);
	PostPIEStartedHandle.Reset();
	EndPIEHandle.Reset();
	RegisteredNeatWidget.Reset();

	if (UToolMenus::Get())
	{
		UToolMenus::UnregisterOwner(this);
	}
}

void FCarAIEditor::RegisterNeatTrainingWidget(UNeatTrainingEditorWidget* Widget)
{
	if (Widget)
	{
		RegisteredNeatWidget = Widget;
		UE_LOG(LogTemp, Log, TEXT("[CarAIEditor] NEAT training widget registered for PIE lifecycle"));
	}
}

void FCarAIEditor::UnregisterNeatTrainingWidget(UNeatTrainingEditorWidget* Widget)
{
	if (RegisteredNeatWidget.Get() == Widget)
	{
		RegisteredNeatWidget.Reset();
		UE_LOG(LogTemp, Log, TEXT("[CarAIEditor] NEAT training widget unregistered"));
	}
}

void FCarAIEditor::OnPostPIEStarted(bool bSimulate)
{
	UE_LOG(LogTemp, Log, TEXT("[CarAIEditor] PIE started (simulate=%d)"), bSimulate ? 1 : 0);

	UWorld* PIEWorld = nullptr;
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World())
			{
				PIEWorld = Context.World();
				break;
			}
		}
	}

	if (PIEWorld)
	{
		UE_LOG(LogTemp, Log, TEXT("[CarAIEditor] PIE world resolved: %s"), *PIEWorld->GetName());
	}

	if (UNeatTrainingEditorWidget* Widget = RegisteredNeatWidget.Get())
	{
		if (Widget->bArmedForPIE)
		{
			Widget->OnPIEWorldStarted(PIEWorld);
		}
	}
}

void FCarAIEditor::OnEndPIE(bool bSimulate)
{
	UE_LOG(LogTemp, Log, TEXT("[CarAIEditor] PIE ended (simulate=%d)"), bSimulate ? 1 : 0);

	if (UNeatTrainingEditorWidget* Widget = RegisteredNeatWidget.Get())
	{
		Widget->OnPIEEnded();
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
