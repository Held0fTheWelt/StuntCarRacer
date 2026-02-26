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
	// Wenn absoluter Pfad angegeben, verwende diesen
	if (FPaths::IsRelative(ScriptName))
	{
		// Suche im Content/Python-Verzeichnis
		FString PluginContentDir = FPaths::ProjectPluginsDir() / TEXT("GameFeatures/CarAI/Content/Python");
		FString ScriptPath = PluginContentDir / ScriptName;
		
		if (FPaths::FileExists(ScriptPath))
		{
			return ScriptPath;
		}
	}
	else if (FPaths::FileExists(ScriptName))
	{
		return ScriptName;
	}
	
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
		UE_LOG(LogTemp, Warning, TEXT("PythonTrainingExecutor: Training läuft bereits!"));
		return false;
	}

	FString ScriptPath = FindPythonScript(PythonScriptPath);
	if (ScriptPath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("PythonTrainingExecutor: Script nicht gefunden: %s"), *PythonScriptPath);
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
		UE_LOG(LogTemp, Error, TEXT("PythonTrainingExecutor: Konnte Prozess nicht starten!"));
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
		UE_LOG(LogTemp, Log, TEXT("PythonTrainingExecutor: Training erfolgreich abgeschlossen (Exit Code: %d)"), LastExitCode);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PythonTrainingExecutor: Training fehlgeschlagen (Exit Code: %d)"), LastExitCode);
	}

	OnTrainingCompleted.Broadcast(bSuccess);
	
	return bSuccess;
}

void UPythonTrainingExecutor::ExecuteTrainingAsync(const FString& PythonScriptPath, const FString& PythonExecutablePath, int32 NumEpochs)
{
	if (bTrainingInProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("PythonTrainingExecutor: Training läuft bereits!"));
		return;
	}

	FString ScriptPath = FindPythonScript(PythonScriptPath);
	if (ScriptPath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("PythonTrainingExecutor: Script nicht gefunden: %s"), *PythonScriptPath);
		OnTrainingCompleted.Broadcast(false);
		return;
	}

	FString PythonExe = FindPythonExecutable(PythonExecutablePath);
	
	// Erstelle Kommandozeile mit Pfaden als Argumente
	FString ExportDir = FPaths::ProjectSavedDir() / TEXT("Training/Exports");
	FString ModelDir = FPaths::ProjectSavedDir() / TEXT("Training/Models");
	
	// Erstelle temporäre Log-Datei für Python-Output
	FString LogDir = FPaths::ProjectSavedDir() / TEXT("Training/Logs");
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*LogDir))
	{
		PlatformFile.CreateDirectoryTree(*LogDir);
	}
	FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	PythonLogFilePath = LogDir / FString::Printf(TEXT("python_training_%s.log"), *Timestamp);
	
	// Erstelle Batch-Script für zuverlässige Umleitung
	FString BatchScriptPath = LogDir / FString::Printf(TEXT("run_training_%s.bat"), *Timestamp);
	
	// Erstelle Batch-Script Inhalt - alle Pfade müssen in Anführungszeichen sein
	FString ScriptDir = FPaths::GetPath(ScriptPath);
	FString BatchScriptContent = FString::Printf(
		TEXT("@echo off\n")
		TEXT("cd /d \"%s\"\n")
		TEXT("\"%s\" \"%s\" \"%s\" \"%s\" \"%d\" > \"%s\" 2>&1\n")
		TEXT("set EXIT_CODE=%%ERRORLEVEL%%\n")
		TEXT("exit /b %%EXIT_CODE%%\n"),
		*ScriptDir,
		*PythonExe,
		*ScriptPath,
		*ExportDir,
		*ModelDir,
		NumEpochs,
		*PythonLogFilePath
	);
	
	// Schreibe Batch-Script
	if (!FFileHelper::SaveStringToFile(BatchScriptContent, *BatchScriptPath))
	{
		UE_LOG(LogTemp, Error, TEXT("PythonTrainingExecutor: Konnte Batch-Script nicht erstellen: %s"), *BatchScriptPath);
		OnTrainingCompleted.Broadcast(false);
		return;
	}
	
	// Kommandozeile für Batch-Script
	FString CommandLine = FString::Printf(TEXT("/c \"%s\""), *BatchScriptPath);

	UE_LOG(LogTemp, Log, TEXT("PythonTrainingExecutor: Starte Training asynchron"));
	UE_LOG(LogTemp, Log, TEXT("PythonTrainingExecutor: Python-Command: %s %s %s %s %d"), *PythonExe, *ScriptPath, *ExportDir, *ModelDir, NumEpochs);
	UE_LOG(LogTemp, Log, TEXT("PythonTrainingExecutor: Python-Output wird in Log-Datei geschrieben: %s"), *PythonLogFilePath);

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
			
			// Zeige finale Zusammenfassung im Log
			if (bSuccess)
			{
				UE_LOG(LogTemp, Log, TEXT("PythonTrainingExecutor: Training erfolgreich abgeschlossen (Exit Code: %d)"), ExitCode);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("PythonTrainingExecutor: Training fehlgeschlagen (Exit Code: %d)"), ExitCode);
			}
			
			UE_LOG(LogTemp, Log, TEXT("PythonTrainingExecutor: Vollständiger Output verfügbar in: %s"), *PythonLogFilePath);
			
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

	UE_LOG(LogTemp, Warning, TEXT("PythonTrainingExecutor: Stoppe Training..."));
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

	// Lese Datei ab LastLogReadPosition
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

	// Lese neue Daten
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

	// Konvertiere zu String und teile in Zeilen
	// Buffer enthält ANSI-Bytes, konvertiere zu FString
	FString NewContent = FString(BytesToRead, (const ANSICHAR*)Buffer.GetData());
	
	// Teile in Zeilen und logge jede Zeile
	TArray<FString> Lines;
	NewContent.ParseIntoArrayLines(Lines, false);
	
	for (const FString& Line : Lines)
	{
		if (!Line.IsEmpty())
		{
			// Logge auf Game-Thread (muss über AsyncTask, da wir im Background-Thread sind)
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
		UE_LOG(LogTemp, Warning, TEXT("PythonTrainingExecutor: Keine Log-Datei verfügbar (Pfad ist leer)"));
		return;
	}

	if (!FPaths::FileExists(PythonLogFilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("PythonTrainingExecutor: Log-Datei existiert nicht: %s"), *PythonLogFilePath);
		return;
	}

	// Lese komplette Log-Datei
	FString LogContent;
	if (!FFileHelper::LoadFileToString(LogContent, *PythonLogFilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("PythonTrainingExecutor: Konnte Log-Datei nicht lesen: %s"), *PythonLogFilePath);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("========================================"));
	UE_LOG(LogTemp, Log, TEXT("PYTHON TRAINING LOG: %s"), *PythonLogFilePath);
	UE_LOG(LogTemp, Log, TEXT("========================================"));

	// Teile in Zeilen und logge jede Zeile
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
	UE_LOG(LogTemp, Log, TEXT("ENDE PYTHON TRAINING LOG (%d Zeilen)"), Lines.Num());
	UE_LOG(LogTemp, Log, TEXT("========================================"));
}
