#include "Manager/NEATTrainingManager.h"
#include "Components/RacingAgentComponent.h"
#include "Manager/PythonTrainingExecutor.h"
#include "NN/NEATGraphEvaluator.h"
#include "Import/NEATGenomeImporter.h"
#include "NN/NEATGenomeTypes.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"

// ============================================================================
// Lifecycle
// ============================================================================

void UNEATTrainingManager::StartTraining()
{
	if (TrainingState != ENEATTrainingState::Idle)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NEATTrainingManager] Training already running!"));
		return;
	}

	if (Agents.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] No agents registered!"));
		return;
	}

	// Initialize Python Executor
	if (!PythonExecutor)
	{
		PythonExecutor = NewObject<UPythonTrainingExecutor>(this);
		PythonExecutor->OnTrainingCompleted.AddDynamic(this, &UNEATTrainingManager::OnPythonEvolutionComplete);
	}

	// Reset stats
	TrainingStats = FNEATTrainingStats();
	TrainingStats.TrainingStartTime = FDateTime::Now();
	CurrentGeneration = 0;
	GenomeFitnessMap.Empty();

	// Log resolved contract at startup (canonical; Python receives same via manifest)
	const FNEATTrainingContract Contract = GetResolvedContract();
	UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] --- NEAT contract (source of truth) ---"));
	UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager]   fitness_dir=%s"), *Contract.FitnessDir);
	UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager]   genome_dir=%s"), *Contract.GenomeDir);
	UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager]   checkpoint_dir=%s"), *Contract.CheckpointDir);
	UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager]   best_genome_path=%s"), *Contract.BestGenomePath);
	UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager]   observation_size=%d action_size=%d"), Contract.ObservationSize, Contract.ActionSize);
	UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] --- end contract ---"));

	UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] Starting NEAT training"));
	UE_LOG(LogTemp, Log, TEXT("  Generations: %d"), NumGenerations);
	UE_LOG(LogTemp, Log, TEXT("  Population: %d"), PopulationSize);
	UE_LOG(LogTemp, Log, TEXT("  Agents: %d"), Agents.Num());

	// Start first generation
	TrainingState = ENEATTrainingState::Evaluating;

	LogNEATStatusSummary(TEXT("Start"));

	// First generation: Create initial genomes via Python
	TriggerPythonEvolution();
}

void UNEATTrainingManager::StopTraining()
{
	if (TrainingState == ENEATTrainingState::Idle)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[NEATTrainingManager] Stopping training..."));

	// Stop evaluation timer
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(EvaluationTickTimer);
	}

	// Stop Python if running
	if (PythonExecutor && PythonExecutor->IsTrainingInProgress())
	{
		PythonExecutor->StopTraining();
	}

	TrainingState = ENEATTrainingState::Idle;
	bWaitingForPython = false;

	OnTrainingComplete.Broadcast();
}

void UNEATTrainingManager::PauseTraining()
{
	if (TrainingState == ENEATTrainingState::Evaluating)
	{
		TrainingState = ENEATTrainingState::Idle;
		UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] Training paused"));
	}
}

void UNEATTrainingManager::ResumeTraining()
{
	if (TrainingState == ENEATTrainingState::Idle)
	{
		TrainingState = ENEATTrainingState::Evaluating;
		UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] Training resumed"));
	}
}

// ============================================================================
// Agent Management
// ============================================================================

void UNEATTrainingManager::RegisterAgent(URacingAgentComponent* Agent)
{
	if (!Agent)
	{
		return;
	}

	Agents.Add(Agent);
	Agent->OnEpisodeDone.AddDynamic(this, &UNEATTrainingManager::OnAgentEpisodeDone);

	// Activate component and start stepping loop (Initialize = Activate + ResetEpisode + tick enabled)
	Agent->Initialize();

	UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] Registered agent %d (Total: %d)"),
		Agent->GenomeID, Agents.Num());
}

void UNEATTrainingManager::UnregisterAgent(URacingAgentComponent* Agent)
{
	if (!Agent)
	{
		return;
	}

	Agent->OnEpisodeDone.RemoveDynamic(this, &UNEATTrainingManager::OnAgentEpisodeDone);
	Agents.Remove(Agent);
}

void UNEATTrainingManager::UnregisterAllAgents()
{
	for (TWeakObjectPtr<URacingAgentComponent>& WeakAgent : Agents)
	{
		if (URacingAgentComponent* Agent = WeakAgent.Get())
		{
			Agent->OnEpisodeDone.RemoveDynamic(this, &UNEATTrainingManager::OnAgentEpisodeDone);
		}
	}

	Agents.Empty();
	UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] All agents unregistered"));
}

// ============================================================================
// Contract (path resolution + manifest)
// ============================================================================

FNEATTrainingContract UNEATTrainingManager::GetResolvedContract() const
{
	FNEATTrainingContract C;
	const FString SavedDir = FPaths::ProjectSavedDir();
	// Single source of truth: all paths derived from manager config (relative dirs + Saved)
	C.FitnessDir = FPaths::Combine(SavedDir, FitnessExportDir);
	C.GenomeDir = FPaths::Combine(SavedDir, GenomeInputDir);
	C.CheckpointDir = C.GenomeDir;
	C.BestGenomePath = FPaths::Combine(C.GenomeDir, FNEATTrainingContract::BestGenomeFileName);
	C.ObservationSize = ObservationSize;
	C.ActionSize = ActionSize;
	return C;
}

FString UNEATTrainingManager::WriteContractManifest(const FNEATTrainingContract& Contract) const
{
	const FString ManifestDir = FPaths::Combine(FPaths::ProjectSavedDir(), FNEATTrainingContract::TrainingDirName);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*ManifestDir))
	{
		if (!PlatformFile.CreateDirectoryTree(*ManifestDir))
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] Failed to create manifest dir: %s"), *ManifestDir);
			return FString();
		}
	}

	const FString ManifestPath = FPaths::Combine(ManifestDir, FNEATTrainingContract::ManifestFileName);
	TSharedPtr<FJsonObject> Root = MakeShareable(new FJsonObject());
	Root->SetStringField(TEXT("fitness_dir"), Contract.FitnessDir);
	Root->SetStringField(TEXT("genome_dir"), Contract.GenomeDir);
	Root->SetStringField(TEXT("checkpoint_dir"), Contract.CheckpointDir);
	Root->SetStringField(TEXT("best_genome_path"), Contract.BestGenomePath);
	Root->SetNumberField(TEXT("observation_size"), Contract.ObservationSize);
	Root->SetNumberField(TEXT("action_size"), Contract.ActionSize);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] Failed to serialize contract manifest"));
		return FString();
	}
	if (!FFileHelper::SaveStringToFile(JsonString, *ManifestPath))
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] Failed to write manifest: %s"), *ManifestPath);
		return FString();
	}
	UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] Wrote contract manifest: %s"), *ManifestPath);
	return ManifestPath;
}

// ============================================================================
// Generation Cycle
// ============================================================================

bool UNEATTrainingManager::LoadGenerationGenomes()
{
	const FNEATTrainingContract Contract = GetResolvedContract();
	const FString GenomeListPath = FPaths::Combine(Contract.GenomeDir,
		FString::Printf(TEXT("generation_%d_genomes.json"), CurrentGeneration));

	if (!FPaths::FileExists(GenomeListPath))
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] VALIDATION FAILED: Genome list not found: %s"), *GenomeListPath);
		return false;
	}

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *GenomeListPath))
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] Failed to read genome list: %s"), *GenomeListPath);
		return false;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] Failed to parse genome list JSON"));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* GenomesArray;
	if (!JsonObject->TryGetArrayField(TEXT("genomes"), GenomesArray))
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] No genomes field in JSON"));
		return false;
	}

	// Validation: all genome files must exist before we accept the generation
	TArray<int32> GenomeIds;
	for (const TSharedPtr<FJsonValue>& GenomeValue : *GenomesArray)
	{
		const TSharedPtr<FJsonObject>* GenomeObj;
		if (!GenomeValue->TryGetObject(GenomeObj))
		{
			continue;
		}
		int32 GenomeID = (*GenomeObj)->GetIntegerField(TEXT("genome_id"));
		GenomeIds.Add(GenomeID);
		const FString GenomePath = FPaths::Combine(Contract.GenomeDir,
			FString::Printf(TEXT("genome_%d.json"), GenomeID));
		if (!FPaths::FileExists(GenomePath))
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] VALIDATION FAILED: Missing genome file: %s (genome_id=%d)"), *GenomePath, GenomeID);
			return false;
		}
	}

	CurrentGenomes.Empty();

	for (int32 GenomeID : GenomeIds)
	{
		const FString GenomePath = FPaths::Combine(Contract.GenomeDir,
			FString::Printf(TEXT("genome_%d.json"), GenomeID));

		FNEATGenome LoadedGenome;
		if (!UNEATGenomeImporter::LoadFromFile(GenomePath, LoadedGenome))
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] VALIDATION FAILED: LoadFromFile or parse failed: %s (genome_id=%d). Check unsupported activation / invalid JSON."), *GenomePath, GenomeID);
			return false;
		}
		if (!LoadedGenome.bIsValid)
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] VALIDATION FAILED: Genome invalid (structure/activation): %s (genome_id=%d)"), *GenomePath, GenomeID);
			return false;
		}
		// Observation/action size must match contract (no silent mismatch)
		if (LoadedGenome.NumInputs != Contract.ObservationSize)
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] VALIDATION FAILED: Observation size mismatch for genome_id=%d: genome has num_inputs=%d, contract ObservationSize=%d"),
				GenomeID, LoadedGenome.NumInputs, Contract.ObservationSize);
			return false;
		}
		if (LoadedGenome.NumOutputs != Contract.ActionSize)
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] VALIDATION FAILED: Action size mismatch for genome_id=%d: genome has num_outputs=%d, contract ActionSize=%d"),
				GenomeID, LoadedGenome.NumOutputs, Contract.ActionSize);
			return false;
		}
		CurrentGenomes.Add(LoadedGenome);
		UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] Genome file loaded: %s (genome_id=%d, inputs=%d outputs=%d)"), *GenomePath, GenomeID, LoadedGenome.NumInputs, LoadedGenome.NumOutputs);
	}

	UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] Loaded %d genomes for generation %d (all validated)"),
		CurrentGenomes.Num(), CurrentGeneration);

	return CurrentGenomes.Num() > 0;
}

bool UNEATTrainingManager::LoadGenomeFromJSON(const FString& FilePath, FNEATGenomeData& OutGenome)
{
	FNEATGenome LoadedGenome;
	if (!UNEATGenomeImporter::LoadFromFile(FilePath, LoadedGenome))
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] LoadFromFile failed for: %s"), *FilePath);
		return false;
	}
	if (!LoadedGenome.bIsValid)
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] Genome validation failed for: %s"), *FilePath);
		return false;
	}
	NEATGenomeToGenomeData(LoadedGenome, OutGenome);
	return true;
}

void UNEATTrainingManager::AssignGenomesToAgents()
{
	const int32 NumGenomes = CurrentGenomes.Num();
	const int32 NumAgents = Agents.Num();
	NumAgentsInCurrentBatch = FMath::Min(NumAgents, NumGenomes - CurrentBatchStartIndex);

	if (NumAgentsInCurrentBatch <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] AssignGenomesToAgents: no agents to assign (batch_start=%d, genomes=%d, agents=%d)"),
			CurrentBatchStartIndex, NumGenomes, NumAgents);
		return;
	}

	const int32 BatchEnd = CurrentBatchStartIndex + NumAgentsInCurrentBatch;
	const int32 TotalBatches = (NumGenomes + NumAgents - 1) / NumAgents;
	const int32 CurrentBatchIndex = (NumGenomes <= NumAgents) ? 0 : (CurrentBatchStartIndex / NumAgents);
	UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] Batch progress: evaluating genomes [%d..%d) of %d (batch %d/%d)"),
		CurrentBatchStartIndex, BatchEnd, NumGenomes, CurrentBatchIndex + 1, TotalBatches);

	int32 AssignedCount = 0;
	for (int32 i = 0; i < NumAgentsInCurrentBatch; ++i)
	{
		URacingAgentComponent* Agent = Agents[i].Get();
		if (!Agent)
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] AssignGenomesToAgents: agent %d is null"), i);
			continue;
		}

		const int32 GenomeIndex = CurrentBatchStartIndex + i;
		const FNEATGenome& Genome = CurrentGenomes[GenomeIndex];
		Agent->GenomeID = Genome.GenomeID;
		Agent->Generation = CurrentGeneration;

		UNEATGraphEvaluator* Evaluator = UNEATGraphEvaluator::CreateFromGenome(this, Genome);
		if (Evaluator)
		{
			Agent->SetPolicyBackend(Evaluator);
			AssignedCount++;
			UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] Genome assigned to agent (agent_index=%d, genome_id=%d, genome_index=%d)"), i, Genome.GenomeID, GenomeIndex);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] Assignment failure: could not build NEAT evaluator for genome_id=%d (genome_index=%d)"), Genome.GenomeID, GenomeIndex);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] AssignGenomesToAgents: %d agents assigned genomes for this batch (genomes %d..%d)"), AssignedCount, CurrentBatchStartIndex, BatchEnd - 1);
}

void UNEATTrainingManager::StartEpisodeEvaluation()
{
	UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] Starting episode evaluation (Gen %d)"), CurrentGeneration);

	// Reset all agents
	for (TWeakObjectPtr<URacingAgentComponent>& WeakAgent : Agents)
	{
		if (URacingAgentComponent* Agent = WeakAgent.Get())
		{
			Agent->ResetEpisode();
		}
	}

	// Start evaluation timer
	EvaluationTimeElapsed = 0.f;

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			EvaluationTickTimer,
			FTimerDelegate::CreateUObject(this, &UNEATTrainingManager::TickEvaluation, 0.1f),
			0.1f, // Timer rate
			true  // Looping
		);
	}
}

void UNEATTrainingManager::TickEvaluation(float DeltaTime)
{
	if (TrainingState != ENEATTrainingState::Evaluating)
	{
		return;
	}

	EvaluationTimeElapsed += DeltaTime;

	// Check if all agents in current batch are done
	if (AreAllAgentsDone())
	{
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().ClearTimer(EvaluationTickTimer);
		}

		const bool bGenerationComplete = (CurrentBatchStartIndex + NumAgentsInCurrentBatch >= CurrentGenomes.Num());

		if (bGenerationComplete)
		{
			// All genomes in this generation evaluated; export only once
			if (!IsGenerationFullyEvaluated())
			{
				UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] Generation %d: fitness map has %d entries but %d genomes; missing genome IDs. Aborting export."),
					CurrentGeneration, GenomeFitnessMap.Num(), CurrentGenomes.Num());
				StopTraining();
				return;
			}
			UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] Generation %d complete: all %d genomes evaluated."), CurrentGeneration, CurrentGenomes.Num());

			ExportFitnessValues();
			CurrentGeneration++;
			TrainingStats.CurrentGeneration = CurrentGeneration;
			OnGenerationComplete.Broadcast(CurrentGeneration - 1);

			if (CurrentGeneration >= NumGenerations)
			{
				TrainingState = ENEATTrainingState::Completed;
				OnTrainingComplete.Broadcast();
				UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] Training completed! (%d generations)"), NumGenerations);
			}
			else
			{
				TriggerPythonEvolution();
			}
		}
		else
		{
			// Next batch: more genomes to evaluate
			CurrentBatchStartIndex += NumAgentsInCurrentBatch;
			UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] Starting next batch: genome index from %d of %d"), CurrentBatchStartIndex, CurrentGenomes.Num());
			AssignGenomesToAgents();
			StartEpisodeEvaluation();
		}
	}
	else if (EvaluationTimeElapsed >= MaxEpisodeDuration)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NEATTrainingManager] Evaluation timeout (%.1fs) for current batch"), MaxEpisodeDuration);

		for (int32 i = 0; i < NumAgentsInCurrentBatch; ++i)
		{
			URacingAgentComponent* Agent = Agents.IsValidIndex(i) ? Agents[i].Get() : nullptr;
			if (Agent && !Agent->IsDone())
			{
				FEpisodeStats Stats = Agent->GetEpisodeStats();
				Stats.TerminationReason = TEXT("Timeout");
				Stats.CalculateNEATFitness();
				GenomeFitnessMap.Add(Agent->GenomeID, Stats.NEATFitness);
			}
		}

		if (GetWorld())
		{
			GetWorld()->GetTimerManager().ClearTimer(EvaluationTickTimer);
		}

		const bool bGenerationComplete = (CurrentBatchStartIndex + NumAgentsInCurrentBatch >= CurrentGenomes.Num());
		if (bGenerationComplete)
		{
			if (!IsGenerationFullyEvaluated())
			{
				UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] Generation %d (timeout): fitness map incomplete (%d/%d). Aborting export."),
					CurrentGeneration, GenomeFitnessMap.Num(), CurrentGenomes.Num());
				StopTraining();
				return;
			}
			ExportFitnessValues();
			CurrentGeneration++;
			TrainingStats.CurrentGeneration = CurrentGeneration;
			OnGenerationComplete.Broadcast(CurrentGeneration - 1);
			if (CurrentGeneration >= NumGenerations)
			{
				TrainingState = ENEATTrainingState::Completed;
				OnTrainingComplete.Broadcast();
				UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] Training completed! (%d generations)"), NumGenerations);
			}
			else
			{
				TriggerPythonEvolution();
			}
		}
		else
		{
			CurrentBatchStartIndex += NumAgentsInCurrentBatch;
			UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] Timeout: starting next batch from genome index %d of %d"), CurrentBatchStartIndex, CurrentGenomes.Num());
			AssignGenomesToAgents();
			StartEpisodeEvaluation();
		}
	}
}

bool UNEATTrainingManager::AreAllAgentsDone() const
{
	for (int32 i = 0; i < NumAgentsInCurrentBatch; ++i)
	{
		URacingAgentComponent* Agent = Agents.IsValidIndex(i) ? Agents[i].Get() : nullptr;
		if (Agent && !Agent->IsDone())
		{
			return false;
		}
	}
	return true;
}

bool UNEATTrainingManager::IsGenerationFullyEvaluated() const
{
	if (GenomeFitnessMap.Num() != CurrentGenomes.Num())
	{
		return false;
	}
	for (const FNEATGenome& G : CurrentGenomes)
	{
		if (!GenomeFitnessMap.Contains(G.GenomeID))
		{
			return false;
		}
	}
	return true;
}

void UNEATTrainingManager::LogNEATStatusSummary(const TCHAR* Phase) const
{
	const FString ExportedFile = LastExportedFitnessFilePath.IsEmpty() ? TEXT("(none)") : LastExportedFitnessFilePath;
	const FString BestGenomeFile = LastLoadedBestGenomePath.IsEmpty() ? TEXT("(none)") : LastLoadedBestGenomePath;
	UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] NEAT status [%s]: generation=%d, active_genomes=%d, completed_genomes=%d, exported_fitness_file=%s, loaded_best_genome=%s"),
		Phase, CurrentGeneration, CurrentGenomes.Num(), GenomeFitnessMap.Num(), *ExportedFile, *BestGenomeFile);
}

void UNEATTrainingManager::OnAgentEpisodeDone(const FEpisodeStats& Stats)
{
	// Find agent that triggered this
	for (TWeakObjectPtr<URacingAgentComponent>& WeakAgent : Agents)
	{
		if (URacingAgentComponent* Agent = WeakAgent.Get())
		{
			if (Agent->GetEpisodeStats().StartTime == Stats.StartTime)
			{
				// Record fitness
				GenomeFitnessMap.Add(Agent->GenomeID, Stats.NEATFitness);

				// Update best fitness
				if (Stats.NEATFitness > TrainingStats.BestFitness)
				{
					TrainingStats.BestFitness = Stats.NEATFitness;
					TrainingStats.BestGenomeID = Agent->GenomeID;

					OnNewBestGenome.Broadcast(Agent->GenomeID, Stats.NEATFitness);

					UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] New best genome! ID=%d, Fitness=%.2f"),
						Agent->GenomeID, Stats.NEATFitness);
				}

				break;
			}
		}
	}

	TrainingStats.TotalEvaluations++;
}

// ============================================================================
// Fitness Export
// ============================================================================

void UNEATTrainingManager::ExportFitnessValues()
{
	// Fail fast: do not export partial generation
	for (const FNEATGenome& G : CurrentGenomes)
	{
		if (!GenomeFitnessMap.Contains(G.GenomeID))
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] ExportFitnessValues: genome_id=%d has no fitness (partial export forbidden)."), G.GenomeID);
			return;
		}
	}

	const FNEATTrainingContract Contract = GetResolvedContract();
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*Contract.FitnessDir))
	{
		if (!PlatformFile.CreateDirectoryTree(*Contract.FitnessDir))
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] Failed to create fitness dir: %s"), *Contract.FitnessDir);
			return;
		}
	}

	TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject());
	RootObject->SetNumberField(TEXT("generation"), CurrentGeneration);

	TArray<TSharedPtr<FJsonValue>> GenomesArray;

	float TotalFitness = 0.f;
	for (const TPair<int32, float>& Pair : GenomeFitnessMap)
	{
		TSharedPtr<FJsonObject> GenomeObj = MakeShareable(new FJsonObject());
		GenomeObj->SetNumberField(TEXT("genome_id"), Pair.Key);
		GenomeObj->SetNumberField(TEXT("fitness"), Pair.Value);

		GenomesArray.Add(MakeShareable(new FJsonValueObject(GenomeObj)));
		TotalFitness += Pair.Value;
	}

	RootObject->SetArrayField(TEXT("genomes"), GenomesArray);

	// Calculate avg fitness
	TrainingStats.AvgFitness = GenomeFitnessMap.Num() > 0 ? TotalFitness / GenomeFitnessMap.Num() : 0.f;

	// Write JSON
	const FString OutputPath = FPaths::Combine(Contract.FitnessDir,
		FString::Printf(TEXT("generation_%d.json"), CurrentGeneration));

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

	if (!FFileHelper::SaveStringToFile(JsonString, *OutputPath))
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] VALIDATION FAILED: Failed to write fitness file: %s"), *OutputPath);
		return;
	}

	if (!PlatformFile.FileExists(*OutputPath))
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] VALIDATION FAILED: Fitness file not found after write: %s"), *OutputPath);
		return;
	}

	LastExportedFitnessFilePath = OutputPath;
	UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] Exported fitness for generation %d (%d genomes, Avg=%.2f) -> %s"),
		CurrentGeneration, GenomeFitnessMap.Num(), TrainingStats.AvgFitness, *OutputPath);

	// Clear for next generation
	GenomeFitnessMap.Empty();

	LogNEATStatusSummary(TEXT("AfterExport"));
}

// ============================================================================
// Python Integration
// ============================================================================

void UNEATTrainingManager::TriggerPythonEvolution()
{
	if (!PythonExecutor)
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] No Python executor!"));
		return;
	}

	const FNEATTrainingContract Contract = GetResolvedContract();
	const FString ManifestPath = WriteContractManifest(Contract);
	if (ManifestPath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] Cannot run Python without contract manifest"));
		return;
	}

	TrainingState = ENEATTrainingState::WaitingForPython;
	bWaitingForPython = true;

	UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] Triggering Python evolution (manifest=%s)"), *ManifestPath);

	PythonExecutor->ExecuteTrainingAsync(PythonScriptPath, PythonExecutable, ManifestPath);
}

void UNEATTrainingManager::OnPythonEvolutionComplete(bool bSuccess)
{
	bWaitingForPython = false;

	if (!bSuccess)
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] Python evolution failed!"));
		StopTraining();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] Python evolution complete"));

	// Load new genomes (validation: missing files, observation/action size, activation)
	if (!LoadGenerationGenomes())
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] Failed to load new genomes (see validation errors above)."));
		StopTraining();
		return;
	}

	LogNEATStatusSummary(TEXT("AfterPythonLoad"));

	CurrentBatchStartIndex = 0;
	if (CurrentGenomes.Num() > Agents.Num())
	{
		const int32 NumBatches = (CurrentGenomes.Num() + Agents.Num() - 1) / Agents.Num();
		UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] Batch mode: %d genomes, %d agents; evaluating in %d batch(es)"), CurrentGenomes.Num(), Agents.Num(), NumBatches);
	}
	AssignGenomesToAgents();

	TrainingState = ENEATTrainingState::Evaluating;
	StartEpisodeEvaluation();
}

bool UNEATTrainingManager::LoadBestGenome()
{
	const FNEATTrainingContract Contract = GetResolvedContract();
	const FString BestGenomePath = Contract.BestGenomePath;

	if (!FPaths::FileExists(BestGenomePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("[NEATTrainingManager] Best genome file not found: %s"), *BestGenomePath);
		return false;
	}

	FNEATGenome BestGenome;
	if (!UNEATGenomeImporter::LoadFromFile(BestGenomePath, BestGenome))
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] LoadBestGenome: LoadFromFile failed for %s"), *BestGenomePath);
		return false;
	}
	if (!BestGenome.bIsValid)
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] LoadBestGenome: genome validation failed for %s"), *BestGenomePath);
		return false;
	}

	// Validation: observation/action size must match contract
	if (BestGenome.NumInputs != Contract.ObservationSize)
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] VALIDATION FAILED: Best genome observation size mismatch: genome num_inputs=%d, contract ObservationSize=%d"),
			BestGenome.NumInputs, Contract.ObservationSize);
		return false;
	}
	if (BestGenome.NumOutputs != Contract.ActionSize)
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] VALIDATION FAILED: Best genome action size mismatch: genome num_outputs=%d, contract ActionSize=%d"),
			BestGenome.NumOutputs, Contract.ActionSize);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] Best genome file loaded: %s (ID=%d, Gen=%d, Fitness=%.2f)"),
		*BestGenomePath, BestGenome.GenomeID, BestGenome.Generation, BestGenome.Fitness);

	UNEATGraphEvaluator* Evaluator = UNEATGraphEvaluator::CreateFromGenome(this, BestGenome);
	if (!Evaluator)
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] LoadBestGenome: failed to build NEAT evaluator for best genome"));
		return false;
	}

	if (Agents.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NEATTrainingManager] LoadBestGenome: no agents registered; best genome loaded but not assigned"));
		return true;
	}

	int32 AssignedCount = 0;
	for (TWeakObjectPtr<URacingAgentComponent>& WeakAgent : Agents)
	{
		URacingAgentComponent* Agent = WeakAgent.Get();
		if (!Agent)
		{
			continue;
		}
		Agent->GenomeID = BestGenome.GenomeID;
		Agent->Generation = BestGenome.Generation;
		Agent->SetPolicyBackend(Evaluator);
		AssignedCount++;
		UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] Best genome assigned to agent (GenomeID=%d)"), BestGenome.GenomeID);
	}

	LastLoadedBestGenomePath = BestGenomePath;
	LogNEATStatusSummary(TEXT("LoadBestGenome"));
	UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] LoadBestGenome: assigned to %d agent(s)"), AssignedCount);
	return true;
}