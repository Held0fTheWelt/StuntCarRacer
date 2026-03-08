#include "Manager/PythonTrainingExecutor.h"

#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "Async/Async.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/DateTime.h"

UPythonTrainingExecutor::UPythonTrainingExecutor()
{
	LastExitCode = -1;
	bTrainingInProgress = false;
}

FString UPythonTrainingExecutor::FindPythonScript(const FString& ScriptName) const
{
	if (ScriptName.IsEmpty())
	{
		return FString();
	}

	// Absolute path: use as-is if file exists
	if (!FPaths::IsRelative(ScriptName))
	{
		if (FPaths::FileExists(ScriptName))
		{
			return ScriptName;
		}
		UE_LOG(LogTemp, Warning, TEXT("PythonTrainingExecutor: Script path is absolute but file not found: %s"), *ScriptName);
		return FString();
	}

	// Relative: resolve against Plugins/GameFeatures/CarAI/Content/Python (script name only, no nested Content/Python)
	FString ScriptNameOnly = ScriptName;
	ScriptNameOnly.ReplaceInline(TEXT("Content/Python/"), TEXT(""));
	ScriptNameOnly.ReplaceInline(TEXT("Content\\Python\\"), TEXT(""));
	FString PluginContentDir = FPaths::ProjectPluginsDir() / TEXT("GameFeatures/CarAI/Content/Python");
	FString ScriptPath = FPaths::Combine(PluginContentDir, ScriptNameOnly);

	if (FPaths::FileExists(ScriptPath))
	{
		return ScriptPath;
	}
	UE_LOG(LogTemp, Warning, TEXT("PythonTrainingExecutor: Script not found at %s (resolved from %s)"), *ScriptPath, *ScriptName);
	return FString();
}

FString UPythonTrainingExecutor::FindPythonExecutable(const FString& ExecutableName) const
{
	if (ExecutableName.IsEmpty())
	{
		return TEXT("python");
	}
	
	// Wenn absoluter Pfad, verwende diesen
	if (FPaths::FileExists(ExecutableName))
	{
		return ExecutableName;
	}
	
	// Ansonsten verwende als-is (wird aus PATH genommen)
	return ExecutableName;
}

bool UPythonTrainingExecutor::ExecuteTraining(const FString& PythonScriptPath, const FString& PythonExecutablePath)
{
	if (bTrainingInProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("PythonTrainingExecutor: Training already in progress"));
		return false;
	}

	FString ScriptPath = FindPythonScript(PythonScriptPath);
	if (ScriptPath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("PythonTrainingExecutor: Script not found: %s"), *PythonScriptPath);
		return false;
	}

	FString PythonExe = FindPythonExecutable(PythonExecutablePath);
	
	// Erstelle Command-Line
	FString CommandLine = FString::Printf(TEXT("\"%s\""), *ScriptPath);
	
	UE_LOG(LogTemp, Log, TEXT("PythonTrainingExecutor: Starte Training: %s %s"), *PythonExe, *CommandLine);

	bTrainingInProgress = true;
	LastOutput = TEXT("");
	LastExitCode = -1;

	// Starte Prozess (synchrone Ausführung)
	TrainingProcessHandle = FPlatformProcess::CreateProc(
		*PythonExe,
		*CommandLine,
		false,  // bLaunchDetached
		true,   // bLaunchHidden
		true,   // bLaunchReallyHidden
		nullptr, // OutProcessID
		0,      // PriorityModifier
		nullptr, // OptionalWorkingDirectory
		nullptr, // PipeWriteChild
		nullptr, // PipeReadChild
		nullptr  // PipeStdErrChild
	);

	if (!TrainingProcessHandle.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("PythonTrainingExecutor: Failed to start process"));
		bTrainingInProgress = false;
		return false;
	}

	// Warte auf Fertigstellung
	FPlatformProcess::WaitForProc(TrainingProcessHandle);
	
	// Hole Exit-Code
	int32 ReturnCode = 0;
	FPlatformProcess::GetProcReturnCode(TrainingProcessHandle, &ReturnCode);
	LastExitCode = ReturnCode;
	
	// Cleanup
	FPlatformProcess::CloseProc(TrainingProcessHandle);
	TrainingProcessHandle.Reset();
	bTrainingInProgress = false;

	bool bSuccess = (LastExitCode == 0);
	
	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("PythonTrainingExecutor: Training completed successfully (Exit Code: %d)"), LastExitCode);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PythonTrainingExecutor: Training failed (Exit Code: %d)"), LastExitCode);
	}

	OnTrainingCompleted.Broadcast(bSuccess);
	
	return bSuccess;
}

void UPythonTrainingExecutor::ExecuteTrainingAsync(const FString& PythonScriptPath, const FString& PythonExecutablePath, const FString& ManifestPath)
{
	if (bTrainingInProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("PythonTrainingExecutor: Training already in progress"));
		return;
	}

	if (ManifestPath.IsEmpty() || !FPaths::FileExists(ManifestPath))
	{
		UE_LOG(LogTemp, Error, TEXT("PythonTrainingExecutor: Manifest path missing or file not found: %s"), *ManifestPath);
		OnTrainingCompleted.Broadcast(false);
		return;
	}

	FString ScriptPath = FindPythonScript(PythonScriptPath);
	if (ScriptPath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("PythonTrainingExecutor: Script not found: %s"), *PythonScriptPath);
		OnTrainingCompleted.Broadcast(false);
		return;
	}

	FString PythonExe = FindPythonExecutable(PythonExecutablePath);

	FString LogDir = FPaths::ProjectSavedDir() / TEXT("Training/Logs");
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*LogDir))
	{
		PlatformFile.CreateDirectoryTree(*LogDir);
	}
	FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	PythonLogFilePath = LogDir / FString::Printf(TEXT("python_training_%s.log"), *Timestamp);

	FString BatchScriptPath = LogDir / FString::Printf(TEXT("run_training_%s.bat"), *Timestamp);
	FString ScriptDir = FPaths::GetPath(ScriptPath);
	// Run: python script --manifest <path> (all paths quoted)
	FString BatchScriptContent = FString::Printf(
		TEXT("@echo off\n")
		TEXT("cd /d \"%s\"\n")
		TEXT("\"%s\" \"%s\" --manifest \"%s\" > \"%s\" 2>&1\n")
		TEXT("set EXIT_CODE=%%ERRORLEVEL%%\n")
		TEXT("exit /b %%EXIT_CODE%%\n"),
		*ScriptDir,
		*PythonExe,
		*ScriptPath,
		*ManifestPath,
		*PythonLogFilePath
	);

	if (!FFileHelper::SaveStringToFile(BatchScriptContent, *BatchScriptPath))
	{
		UE_LOG(LogTemp, Error, TEXT("PythonTrainingExecutor: Failed to write batch script: %s"), *BatchScriptPath);
		OnTrainingCompleted.Broadcast(false);
		return;
	}

	FString CommandLine = FString::Printf(TEXT("/c \"%s\""), *BatchScriptPath);

	UE_LOG(LogTemp, Log, TEXT("PythonTrainingExecutor: Starting training (script=%s manifest=%s)"), *ScriptPath, *ManifestPath);
	UE_LOG(LogTemp, Log, TEXT("PythonTrainingExecutor: Log file: %s"), *PythonLogFilePath);

	bTrainingInProgress = true;
	LastOutput = TEXT("");
	LastExitCode = -1;
	LastLogReadPosition = 0;

	// Starte asynchronen Task (erfasse BatchScriptPath für späteres Löschen)
	FString BatchScriptPathToDelete = BatchScriptPath;
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, CommandLine, BatchScriptPathToDelete]()
	{
		// Starte Prozess mit cmd.exe für Shell-Umleitung
		FString CmdExe = TEXT("cmd.exe");
		FProcHandle ProcessHandle = FPlatformProcess::CreateProc(
			*CmdExe,
			*CommandLine,
			false,  // bLaunchDetached
			true,   // bLaunchHidden
			true,   // bLaunchReallyHidden
			nullptr, // OutProcessID
			0,      // PriorityModifier
			nullptr, // OptionalWorkingDirectory
			nullptr, // PipeWriteChild
			nullptr, // PipeReadChild
			nullptr  // PipeStdErrChild
		);

		if (!ProcessHandle.IsValid())
		{
			AsyncTask(ENamedThreads::GameThread, [this]()
			{
				bTrainingInProgress = false;
				LastExitCode = -1;
				OnTrainingCompleted.Broadcast(false);
			});
			return;
		}

		// Periodisch Log-Datei lesen während Prozess läuft
		while (FPlatformProcess::IsProcRunning(ProcessHandle))
		{
			ReadPythonLogToUnrealLog();
			FPlatformProcess::Sleep(0.5f); // Alle 0.5 Sekunden prüfen
		}

		// Letzte Lesung nach Prozess-Ende
		ReadPythonLogToUnrealLog();

		// Lese komplette Log-Datei für LastOutput
		if (FPaths::FileExists(PythonLogFilePath))
		{
			FFileHelper::LoadFileToString(LastOutput, *PythonLogFilePath);
		}

		// Warte auf Fertigstellung (sollte bereits fertig sein)
		FPlatformProcess::WaitForProc(ProcessHandle);
		
		int32 ExitCode = 0;
		FPlatformProcess::GetProcReturnCode(ProcessHandle, &ExitCode);
		FPlatformProcess::CloseProc(ProcessHandle);

		// Lösche Batch-Script (cleanup)
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (PlatformFile.FileExists(*BatchScriptPathToDelete))
		{
			PlatformFile.DeleteFile(*BatchScriptPathToDelete);
		}

		// Callback auf Game-Thread
		AsyncTask(ENamedThreads::GameThread, [this, ExitCode]()
		{
			bTrainingInProgress = false;
			LastExitCode = ExitCode;
			bool bSuccess = (ExitCode == 0);
			
			if (bSuccess)
			{
				UE_LOG(LogTemp, Log, TEXT("PythonTrainingExecutor: Training completed successfully (Exit Code: %d)"), ExitCode);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("PythonTrainingExecutor: Training failed (Exit Code: %d)"), ExitCode);
			}
			UE_LOG(LogTemp, Log, TEXT("PythonTrainingExecutor: Full output: %s"), *PythonLogFilePath);
			
			OnTrainingCompleted.Broadcast(bSuccess);
		});
	});
}

bool UPythonTrainingExecutor::IsTrainingInProgress() const
{
	return bTrainingInProgress;
}

bool UPythonTrainingExecutor::WaitForTraining(float TimeoutSeconds)
{
	if (!bTrainingInProgress)
	{
		return true; // Nicht gestartet oder bereits fertig
	}

	const double StartTime = FPlatformTime::Seconds();
	while (bTrainingInProgress && (FPlatformTime::Seconds() - StartTime) < TimeoutSeconds)
	{
		FPlatformProcess::Sleep(0.1f); // Warte 100ms
	}

	return !bTrainingInProgress;
}

void UPythonTrainingExecutor::StopTraining()
{
	if (!bTrainingInProgress || !TrainingProcessHandle.IsValid())
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("PythonTrainingExecutor: Stopping training..."));
	FPlatformProcess::TerminateProc(TrainingProcessHandle, true);
	FPlatformProcess::CloseProc(TrainingProcessHandle);
	TrainingProcessHandle.Reset();
	bTrainingInProgress = false;
	LastExitCode = -1;
}

void UPythonTrainingExecutor::ReadPythonLogToUnrealLog()
{
	if (PythonLogFilePath.IsEmpty() || !FPaths::FileExists(PythonLogFilePath))
	{
		return;
	}

	IFileHandle* FileHandle = FPlatformFileManager::Get().GetPlatformFile().OpenRead(*PythonLogFilePath);
	if (!FileHandle)
	{
		return;
	}

	int64 FileSize = FileHandle->Size();
	if (FileSize <= LastLogReadPosition)
	{
		delete FileHandle;
		return;
	}

	int64 BytesToRead = FileSize - LastLogReadPosition;
	TArray<uint8> Buffer;
	Buffer.SetNumUninitialized(BytesToRead);
	
	FileHandle->Seek(LastLogReadPosition);
	if (!FileHandle->Read(Buffer.GetData(), BytesToRead))
	{
		delete FileHandle;
		return;
	}
	
	delete FileHandle;
	LastLogReadPosition = FileSize;

	FString NewContent = FString(BytesToRead, (const ANSICHAR*)Buffer.GetData());
	
	TArray<FString> Lines;
	NewContent.ParseIntoArrayLines(Lines, false);
	for (const FString& Line : Lines)
	{
		if (!Line.IsEmpty())
		{
			FString LineToLog = Line; // Kopie für Lambda
			AsyncTask(ENamedThreads::GameThread, [LineToLog]()
			{
				UE_LOG(LogTemp, Log, TEXT("[Python] %s"), *LineToLog);
			});
		}
	}
}

void UPythonTrainingExecutor::ShowPythonLog()
{
	if (PythonLogFilePath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("PythonTrainingExecutor: No log file (path empty)"));
		return;
	}

	if (!FPaths::FileExists(PythonLogFilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("PythonTrainingExecutor: Log file not found: %s"), *PythonLogFilePath);
		return;
	}

	FString LogContent;
	if (!FFileHelper::LoadFileToString(LogContent, *PythonLogFilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("PythonTrainingExecutor: Failed to read log file: %s"), *PythonLogFilePath);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("========================================"));
	UE_LOG(LogTemp, Log, TEXT("PYTHON TRAINING LOG: %s"), *PythonLogFilePath);
	UE_LOG(LogTemp, Log, TEXT("========================================"));

	TArray<FString> Lines;
	LogContent.ParseIntoArrayLines(Lines, false);
	for (const FString& Line : Lines)
	{
		if (!Line.IsEmpty())
		{
			UE_LOG(LogTemp, Log, TEXT("[Python] %s"), *Line);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("========================================"));
	UE_LOG(LogTemp, Log, TEXT("END PYTHON TRAINING LOG (%d lines)"), Lines.Num());
	UE_LOG(LogTemp, Log, TEXT("========================================"));
}
