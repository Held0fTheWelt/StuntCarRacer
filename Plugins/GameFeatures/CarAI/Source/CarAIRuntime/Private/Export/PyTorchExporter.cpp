#include "Export/PyTorchExporter.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

void UPyTorchExporter::Initialize(const FString& InExportDirectory)
{
	ExportDirectory = InExportDirectory;
	
	// Erstelle Verzeichnis falls nicht vorhanden
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*ExportDirectory))
	{
		PlatformFile.CreateDirectoryTree(*ExportDirectory);
	}

	bInitialized = true;
	CurrentRolloutIndex = 0;
	ExportedExperiences.Reset();
	
	// Reset gesammelte Rollouts
	{
		FScopeLock Lock(&CollectedRolloutsMutex);
		CollectedRollouts.Reset();
	}

	UE_LOG(LogTemp, Log, TEXT("PyTorchExporter initialized. Export directory: %s"), *ExportDirectory);
}

void UPyTorchExporter::ExportExperience(const FTrainingExperience& Experience)
{
	if (!bInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("PyTorchExporter: Not initialized! Call Initialize() first."));
		return;
	}

	ExportedExperiences.Add(Experience);
}

void UPyTorchExporter::ExportRollout(const TArray<FTrainingExperience>& Experiences)
{
	if (!bInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("PyTorchExporter: Not initialized! Call Initialize() first."));
		return;
	}

	// Füge Rollout zur Sammlung hinzu (wird später beim StopTraining exportiert)
	FScopeLock Lock(&CollectedRolloutsMutex);
	CollectedRollouts.Add(Experiences);
	UE_LOG(LogTemp, Verbose, TEXT("PyTorchExporter: Rollout gesammelt (%d experiences, %d Rollouts insgesamt)"), 
		Experiences.Num(), CollectedRollouts.Num());
}

void UPyTorchExporter::FinalizeExportAsync()
{
	if (!bInitialized || ExportedExperiences.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("PyTorchExporter: Nothing to export!"));
		return;
	}

	if (bExportInProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("PyTorchExporter: Export already in progress, queuing..."));
		// Queue für später
		FScopeLock Lock(&ExportQueueMutex);
		ExportQueue.Add(ExportedExperiences);
		ExportedExperiences.Reset();
		CurrentRolloutIndex++;
		return;
	}

	// Kopiere Daten für Async-Task (kann bei großen Datenmengen teuer sein!)
	// Move-Semantik verwenden für bessere Performance
	TArray<FTrainingExperience> ExperiencesToExport;
	ExperiencesToExport = MoveTemp(ExportedExperiences);
	
	// Thread-safe Rollout-Index
	int32 RolloutIndex;
	{
		FScopeLock Lock(&RolloutIndexMutex);
		RolloutIndex = CurrentRolloutIndex;
		CurrentRolloutIndex++;
	}
	
	// Reset für nächsten Rollout (sofort, damit Training weiterlaufen kann)
	ExportedExperiences.Reset();
	
	// Starte Async-Export (muss wirklich asynchron sein!)
	bExportInProgress = true;
	
	// Verwende ENamedThreads::AnyHiPriThreadHiPriTask für höhere Priorität
	// oder ENamedThreads::AnyBackgroundThreadNormalTask für normale Priorität
	// WICHTIG: Lambda muss Daten by-value kopieren, nicht by-reference!
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, ExperiencesToExport = MoveTemp(ExperiencesToExport), RolloutIndex]() mutable
	{
		// Erstelle Dateinamen
		const FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
		const FString Filename = FString::Printf(TEXT("rollout_%d_%s"), RolloutIndex, *Timestamp);
		const FString ExportDir = this->ExportDirectory;
		const FString JSONPath = FPaths::Combine(ExportDir, Filename + TEXT(".json"));
		
		// Exportiere (blockiert nur diesen Thread, nicht den Game-Thread!)
		const bool bSuccess = WriteJSONFile(JSONPath, ExperiencesToExport);
		
		// Callback auf Game-Thread
		AsyncTask(ENamedThreads::GameThread, [this, bSuccess, JSONPath, NumExperiences = ExperiencesToExport.Num()]()
		{
			bExportInProgress = false;
			
			if (bSuccess)
			{
				UE_LOG(LogTemp, Log, TEXT("PyTorchExporter: Exported %d experiences to %s (async)"), 
					NumExperiences, *JSONPath);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("PyTorchExporter: Failed to export (async) to %s"), *JSONPath);
			}
			
			// Prüfe Queue
			FScopeLock QueueLock(&ExportQueueMutex);
			if (ExportQueue.Num() > 0)
			{
				TArray<FTrainingExperience> QueuedExperiences = MoveTemp(ExportQueue[0]);
				ExportQueue.RemoveAt(0);
				QueueLock.Unlock();
				
				// Thread-safe Rollout-Index
				int32 NextRolloutIndex;
				{
					FScopeLock IndexLock(&RolloutIndexMutex);
					NextRolloutIndex = CurrentRolloutIndex;
					CurrentRolloutIndex++;
				}
				
				// Rekursiver Aufruf für nächsten Export
				bExportInProgress = true;
				AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, QueuedExperiences = MoveTemp(QueuedExperiences), NextRolloutIndex]() mutable
				{
					const FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
					const FString Filename = FString::Printf(TEXT("rollout_%d_%s"), NextRolloutIndex, *Timestamp);
					const FString ExportDir = this->ExportDirectory;
					const FString JSONPath = FPaths::Combine(ExportDir, Filename + TEXT(".json"));
					const bool bSuccess = WriteJSONFile(JSONPath, QueuedExperiences);
					
					AsyncTask(ENamedThreads::GameThread, [this, bSuccess, JSONPath, NumExperiences = QueuedExperiences.Num()]()
					{
						bExportInProgress = false;
						if (bSuccess)
						{
							UE_LOG(LogTemp, Log, TEXT("PyTorchExporter: Exported %d experiences (queued) to %s"), NumExperiences, *JSONPath);
						}
					});
				});
			}
		});
	});
}

bool UPyTorchExporter::FinalizeExport()
{
	if (!bInitialized || ExportedExperiences.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("PyTorchExporter: Nothing to export!"));
		return false;
	}

	// Erstelle Dateinamen mit Timestamp
	const FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	const FString Filename = FString::Printf(TEXT("rollout_%d_%s"), CurrentRolloutIndex, *Timestamp);
	
	// Exportiere als JSON (einfacher zu lesen und zu debuggen)
	const FString JSONPath = FPaths::Combine(ExportDirectory, Filename + TEXT(".json"));
	if (!WriteJSONFile(JSONPath, ExportedExperiences))
	{
		UE_LOG(LogTemp, Error, TEXT("PyTorchExporter: Failed to write JSON file!"));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("PyTorchExporter: Exported %d experiences to %s"), 
		ExportedExperiences.Num(), *JSONPath);

	CurrentRolloutIndex++;
	return true;
}

// ExecuteExportAsync wurde entfernt - Logik ist jetzt direkt in FinalizeExportAsync

void UPyTorchExporter::ResetExport()
{
	ExportedExperiences.Reset();
}

void UPyTorchExporter::ExportAllCollectedRolloutsAsync()
{
	FScopeLock Lock(&CollectedRolloutsMutex);
	
	if (CollectedRollouts.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("PyTorchExporter: Keine gesammelten Rollouts zum Exportieren!"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("PyTorchExporter: Starte Bulk-Export von %d Rollouts (asynchron)"), CollectedRollouts.Num());

	// Kopiere alle Rollouts für Async-Task
	TArray<TArray<FTrainingExperience>> RolloutsToExport = CollectedRollouts;
	CollectedRollouts.Reset();
	Lock.Unlock();

	// Exportiere jeden Rollout asynchron (nacheinander in Queue)
	for (int32 RolloutIdx = 0; RolloutIdx < RolloutsToExport.Num(); ++RolloutIdx)
	{
		TArray<FTrainingExperience> RolloutData = MoveTemp(RolloutsToExport[RolloutIdx]);
		
		// Thread-safe Rollout-Index
		int32 RolloutIndex;
		{
			FScopeLock IndexLock(&RolloutIndexMutex);
			RolloutIndex = CurrentRolloutIndex;
			CurrentRolloutIndex++;
		}

		// Exportiere in Hintergrund-Thread
		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, RolloutData = MoveTemp(RolloutData), RolloutIndex, TotalRollouts = RolloutsToExport.Num(), CurrentRollout = RolloutIdx + 1]() mutable
		{
			const FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
			const FString Filename = FString::Printf(TEXT("rollout_%d_%s"), RolloutIndex, *Timestamp);
			const FString ExportDir = this->ExportDirectory;
			const FString JSONPath = FPaths::Combine(ExportDir, Filename + TEXT(".json"));
			
			const bool bSuccess = WriteJSONFile(JSONPath, RolloutData);
			
			// Callback auf Game-Thread
			AsyncTask(ENamedThreads::GameThread, [this, bSuccess, JSONPath, NumExperiences = RolloutData.Num(), TotalRollouts, CurrentRollout]()
			{
				if (bSuccess)
				{
					UE_LOG(LogTemp, Log, TEXT("PyTorchExporter: [%d/%d] Exported %d experiences to %s (bulk export)"), 
						CurrentRollout, TotalRollouts, NumExperiences, *JSONPath);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("PyTorchExporter: [%d/%d] Failed to export to %s"), CurrentRollout, TotalRollouts, *JSONPath);
				}
			});
		});
	}

	UE_LOG(LogTemp, Log, TEXT("PyTorchExporter: Bulk-Export gestartet - %d Rollouts werden asynchron exportiert"), RolloutsToExport.Num());
}

int32 UPyTorchExporter::GetCollectedRolloutCount() const
{
	FScopeLock Lock(&CollectedRolloutsMutex);
	return CollectedRollouts.Num();
}

bool UPyTorchExporter::WriteJSONFile(const FString& Filepath, const TArray<FTrainingExperience>& Experiences)
{
	// Prüfe auf NaN/Infinity und filtere sie
	TArray<FTrainingExperience> ValidExperiences;
	for (const FTrainingExperience& Exp : Experiences)
	{
		bool bValid = true;
		
		// Prüfe State
		for (float Val : Exp.State)
		{
			if (!FMath::IsFinite(Val))
			{
				bValid = false;
				break;
			}
		}
		
		// Prüfe Action
		if (bValid && (!FMath::IsFinite(Exp.Action.Steer) || !FMath::IsFinite(Exp.Action.Throttle) || !FMath::IsFinite(Exp.Action.Brake)))
		{
			bValid = false;
		}
		
		// Prüfe Reward, LogProb, Value
		if (bValid && (!FMath::IsFinite(Exp.Reward) || !FMath::IsFinite(Exp.LogProb) || !FMath::IsFinite(Exp.Value)))
		{
			bValid = false;
		}
		
		if (bValid)
		{
			ValidExperiences.Add(Exp);
		}
	}
	
	if (ValidExperiences.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("PyTorchExporter: No valid experiences to export!"));
		return false;
	}
	
	if (ValidExperiences.Num() < Experiences.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("PyTorchExporter: Filtered out %d invalid experiences (NaN/Inf)"), 
			Experiences.Num() - ValidExperiences.Num());
	}

	TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);
	TArray<TSharedPtr<FJsonValue>> ExperiencesArray;

	for (const FTrainingExperience& Exp : ValidExperiences)
	{
		TSharedPtr<FJsonObject> ExpObject = MakeShareable(new FJsonObject);

		// State (Observation)
		TArray<TSharedPtr<FJsonValue>> StateArray;
		for (float Val : Exp.State)
		{
			StateArray.Add(MakeShareable(new FJsonValueNumber(Val)));
		}
		ExpObject->SetArrayField(TEXT("state"), StateArray);

		// Action
		TSharedPtr<FJsonObject> ActionObject = MakeShareable(new FJsonObject);
		ActionObject->SetNumberField(TEXT("steer"), Exp.Action.Steer);
		ActionObject->SetNumberField(TEXT("throttle"), Exp.Action.Throttle);
		ActionObject->SetNumberField(TEXT("brake"), Exp.Action.Brake);
		ExpObject->SetObjectField(TEXT("action"), ActionObject);

		// Reward, Done, etc.
		ExpObject->SetNumberField(TEXT("reward"), Exp.Reward);
		ExpObject->SetBoolField(TEXT("done"), Exp.bDone);
		ExpObject->SetNumberField(TEXT("log_prob"), Exp.LogProb);
		ExpObject->SetNumberField(TEXT("value"), Exp.Value);
		ExpObject->SetNumberField(TEXT("advantage"), Exp.Advantage);
		ExpObject->SetNumberField(TEXT("return"), Exp.Return);
		ExpObject->SetNumberField(TEXT("agent_index"), Exp.AgentIndex);

		ExperiencesArray.Add(MakeShareable(new FJsonValueObject(ExpObject)));
	}

	RootObject->SetArrayField(TEXT("experiences"), ExperiencesArray);
	RootObject->SetNumberField(TEXT("num_experiences"), ValidExperiences.Num());
	RootObject->SetStringField(TEXT("timestamp"), FDateTime::Now().ToString());

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

	// Atomares Schreiben: Erst in Temp-Datei, dann umbenennen
	const FString TempFilepath = Filepath + TEXT(".tmp");
	
	if (!FFileHelper::SaveStringToFile(OutputString, *TempFilepath))
	{
		UE_LOG(LogTemp, Error, TEXT("PyTorchExporter: Failed to write temp file: %s"), *TempFilepath);
		return false;
	}
	
	// Prüfe ob Temp-Datei vollständig ist (endet mit })
	if (OutputString.Len() > 0 && !OutputString.EndsWith(TEXT("}")))
	{
		UE_LOG(LogTemp, Error, TEXT("PyTorchExporter: JSON string incomplete!"));
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.DeleteFile(*TempFilepath);
		return false;
	}
	
	// Verschiebe Temp-Datei zur finalen Datei (atomar)
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (PlatformFile.FileExists(*Filepath))
	{
		PlatformFile.DeleteFile(*Filepath);
	}
	
	if (!PlatformFile.MoveFile(*Filepath, *TempFilepath))
	{
		UE_LOG(LogTemp, Error, TEXT("PyTorchExporter: Failed to move temp file to final location: %s"), *Filepath);
		return false;
	}
	
	return true;
}

