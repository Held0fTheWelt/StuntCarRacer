#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HAL/PlatformProcess.h"
#include "PythonTrainingExecutor.generated.h"

/**
 * Führt Python-Training-Scripts aus und überwacht den Prozess
 */
UCLASS()
class CARAIEDITOR_API UPythonTrainingExecutor : public UObject
{
	GENERATED_BODY()

public:
	UPythonTrainingExecutor();

	/** Startet Python-Training-Script und wartet auf Fertigstellung */
	UFUNCTION(BlueprintCallable, Category = "Python Training")
	bool ExecuteTraining(const FString& PythonScriptPath, const FString& PythonExecutablePath = TEXT("python"));

	/** Startet Python-Training-Script asynchron (non-blocking) */
	UFUNCTION(BlueprintCallable, Category = "Python Training")
	void ExecuteTrainingAsync(const FString& PythonScriptPath, const FString& PythonExecutablePath = TEXT("python"), int32 NumEpochs = 10);

	/** Prüft ob Training noch läuft */
	UFUNCTION(BlueprintCallable, Category = "Python Training")
	bool IsTrainingInProgress() const;

	/** Wartet auf Training-Fertigstellung (blockiert!) */
	UFUNCTION(BlueprintCallable, Category = "Python Training")
	bool WaitForTraining(float TimeoutSeconds = 300.0f);

	/** Stoppt laufendes Training */
	UFUNCTION(BlueprintCallable, Category = "Python Training")
	void StopTraining();

	/** Gibt Exit-Code des letzten Trainings zurück (0 = Erfolg) */
	UFUNCTION(BlueprintCallable, Category = "Python Training")
	int32 GetLastExitCode() const { return LastExitCode; }

	/** Gibt Output des letzten Trainings zurück */
	UFUNCTION(BlueprintCallable, Category = "Python Training")
	FString GetLastOutput() const { return LastOutput; }

	/** Gibt Pfad zur Python-Log-Datei zurück */
	UFUNCTION(BlueprintCallable, Category = "Python Training")
	FString GetPythonLogFilePath() const { return PythonLogFilePath; }

	/** Liest Python-Log-Datei ein und gibt sie in Unreal-Logs aus (optional) */
	UFUNCTION(BlueprintCallable, Category = "Python Training")
	void ShowPythonLog();

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTrainingCompleted, bool, bSuccess);
	UPROPERTY(BlueprintAssignable, Category = "Python Training")
	FOnTrainingCompleted OnTrainingCompleted;

protected:
	/** Callback für asynchrones Training */
	void OnTrainingCompletedInternal(bool bSuccess);

private:
	FProcHandle TrainingProcessHandle;
	FString LastOutput;
	int32 LastExitCode = -1;
	bool bTrainingInProgress = false;
	
	/** Pfad zur Python-Log-Datei (wird während Training geschrieben) */
	FString PythonLogFilePath;
	
	/** Position in Log-Datei (für inkrementelles Lesen) */
	int64 LastLogReadPosition = 0;

	FString FindPythonScript(const FString& ScriptName) const;
	FString FindPythonExecutable(const FString& ExecutableName) const;
	
	/** Liest neue Zeilen aus Python-Log-Datei und gibt sie im Unreal Log aus (wird in Background-Thread aufgerufen) */
	void ReadPythonLogToUnrealLog();
};
