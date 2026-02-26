#pragma once

#include "CoreMinimal.h"
#include "Types/RacingAgentTypes.h"
#include "RacingTrainingTypes.h"
#include "Async/Async.h"
#include "HAL/ThreadSafeBool.h"
#include "PyTorchExporter.generated.h"

/**
 * Exportiert Training-Daten für PyTorch-Training
 * Speichert Observations, Actions, Rewards in einem kompakten Format
 */
UCLASS(BlueprintType)
class CARAIRUNTIME_API UPyTorchExporter : public UObject
{
	GENERATED_BODY()

public:
	/** Initialisiert den Exporter mit einem Dateipfad */
	UFUNCTION(BlueprintCallable, Category = "PyTorch Export")
	void Initialize(const FString& InExportDirectory);

	/** Exportiert eine einzelne Experience */
	UFUNCTION(BlueprintCallable, Category = "PyTorch Export")
	void ExportExperience(const FTrainingExperience& Experience);

	/** Exportiert einen kompletten Rollout (Batch von Experiences) */
	UFUNCTION(BlueprintCallable, Category = "PyTorch Export")
	void ExportRollout(const TArray<FTrainingExperience>& Experiences);

	/** Finalisiert den Export und schreibt die Datei (asynchron) */
	UFUNCTION(BlueprintCallable, Category = "PyTorch Export")
	void FinalizeExportAsync();

	/** Finalisiert den Export synchron (für Debugging) */
	UFUNCTION(BlueprintCallable, Category = "PyTorch Export")
	bool FinalizeExport();

	/** Setzt den Export zurück (für neuen Rollout) */
	UFUNCTION(BlueprintCallable, Category = "PyTorch Export")
	void ResetExport();

	/** Prüft ob ein Export gerade läuft */
	UFUNCTION(BlueprintCallable, Category = "PyTorch Export")
	bool IsExportInProgress() const { return bExportInProgress; }

	/** Gibt zurück, ob der Exporter initialisiert ist */
	UFUNCTION(BlueprintCallable, Category = "PyTorch Export")
	bool IsInitialized() const { return bInitialized; }

	/** Gibt die Anzahl exportierter Experiences zurück */
	UFUNCTION(BlueprintCallable, Category = "PyTorch Export")
	int32 GetExportedCount() const { return ExportedExperiences.Num(); }

	/** Exportiert alle gesammelten Rollouts als Bulk (asynchron) - wird bei StopTraining aufgerufen */
	UFUNCTION(BlueprintCallable, Category = "PyTorch Export")
	void ExportAllCollectedRolloutsAsync();

	/** Gibt die Anzahl gesammelter Rollouts zurück (thread-safe) */
	UFUNCTION(BlueprintCallable, Category = "PyTorch Export")
	int32 GetCollectedRolloutCount() const;

private:
	UPROPERTY()
	FString ExportDirectory;

	UPROPERTY()
	TArray<FTrainingExperience> ExportedExperiences;

	UPROPERTY()
	bool bInitialized = false;

	UPROPERTY()
	int32 CurrentRolloutIndex = 0;

	/** Thread-safe Flag für laufenden Export */
	FThreadSafeBool bExportInProgress = false;

	/** Queue für Export-Anfragen (Thread-safe) */
	TArray<TArray<FTrainingExperience>> ExportQueue;
	FCriticalSection ExportQueueMutex;
	
	/** Mutex für CurrentRolloutIndex */
	mutable FCriticalSection RolloutIndexMutex;

	/** Gesammelte Rollouts (werden beim Stoppen exportiert) */
	TArray<TArray<FTrainingExperience>> CollectedRollouts;
	mutable FCriticalSection CollectedRolloutsMutex;

	/** Schreibt Daten im NumPy-Format (.npz) - kompatibel mit PyTorch */
	bool WriteNPZFile(const FString& Filepath);

	/** Schreibt Daten im JSON-Format (einfacher zu debuggen) */
	bool WriteJSONFile(const FString& Filepath, const TArray<FTrainingExperience>& Experiences);
};

