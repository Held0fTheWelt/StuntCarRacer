#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HAL/PlatformProcess.h"
#include "HAL/CriticalSection.h"
#include "PythonTrainingExecutor.generated.h"

/**
 * Runs Python training scripts and monitors the process.
 * Script path: filename only (e.g. train_neat.py). Resolved relative to
 * CarAI plugin Content/Python: <ProjectPlugins>/GameFeatures/CarAI/Content/Python/<script>.
 * All NEAT paths and config are passed via the manifest file written by NEATTrainingManager.
 */
UCLASS()
class CARAIEDITOR_API UPythonTrainingExecutor : public UObject
{
	GENERATED_BODY()

public:
	UPythonTrainingExecutor();

	/**
	 * Runs Python training script synchronously (blocks game thread until complete).
	 * ManifestPath must be the neat_contract.json path written by NEATTrainingManager (source of truth for Python).
	 * Prefer ExecuteTrainingAsync for training; this path is provided for completeness but blocks the game thread.
	 * Python receives --manifest <ManifestPath> on the command line.
	 */
	UFUNCTION(BlueprintCallable, Category = "Python Training")
	bool ExecuteTraining(const FString& PythonScriptPath, const FString& ManifestPath, const FString& PythonExecutablePath = TEXT("python"));

	/** Runs Python training script asynchron. ManifestPath must be the neat_contract.json path written by NEATTrainingManager (source of truth for Python). */
	UFUNCTION(BlueprintCallable, Category = "Python Training")
	void ExecuteTrainingAsync(const FString& PythonScriptPath, const FString& PythonExecutablePath, const FString& ManifestPath);

	/** Returns true if training is still running */
	UFUNCTION(BlueprintCallable, Category = "Python Training")
	bool IsTrainingInProgress() const;

	/** Blocks until training completes or timeout */
	UFUNCTION(BlueprintCallable, Category = "Python Training")
	bool WaitForTraining(float TimeoutSeconds = 300.0f);

	/** Stops running training */
	UFUNCTION(BlueprintCallable, Category = "Python Training")
	void StopTraining();

	/** Last run exit code (0 = success) */
	UFUNCTION(BlueprintCallable, Category = "Python Training")
	int32 GetLastExitCode() const { return LastExitCode; }

	/** Last run full output */
	UFUNCTION(BlueprintCallable, Category = "Python Training")
	FString GetLastOutput() const { return LastOutput; }

	/** Path to Python log file */
	UFUNCTION(BlueprintCallable, Category = "Python Training")
	FString GetPythonLogFilePath() const { return PythonLogFilePath; }

	/** Reads Python log and prints to Unreal log */
	UFUNCTION(BlueprintCallable, Category = "Python Training")
	void ShowPythonLog();

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTrainingCompleted, bool, bSuccess);
	UPROPERTY(BlueprintAssignable, Category = "Python Training")
	FOnTrainingCompleted OnTrainingCompleted;

protected:
	void OnTrainingCompletedInternal(bool bSuccess);

private:
	FProcHandle TrainingProcessHandle;
	FString LastOutput;
	int32 LastExitCode = -1;
	bool bTrainingInProgress = false;
	FString PythonLogFilePath;
	int64 LastLogReadPosition = 0;

	/** Guards TrainingProcessHandle for cross-thread access (background task writes, game thread reads in StopTraining). */
	FCriticalSection ProcessHandleLock;

	/** Resolves script to absolute path. Input: filename only (e.g. train_neat.py); strips any Content/Python prefix. Result: Plugin Content/Python + filename. */
	FString FindPythonScript(const FString& ScriptName) const;
	FString FindPythonExecutable(const FString& ExecutableName) const;
	void ReadPythonLogToUnrealLog();
};
