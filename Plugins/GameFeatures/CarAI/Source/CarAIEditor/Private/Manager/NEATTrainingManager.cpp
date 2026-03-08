#include "Manager/NEATTrainingManager.h"
#include "Components/RacingAgentComponent.h"
#include "Manager/PythonTrainingExecutor.h"
#include "NN/NEATGraphEvaluator.h"
#include "Import/NEATGenomeImporter.h"
#include "NN/NEATGenomeTypes.h"
#include "CarAIRuntimeLogging.h"
#include "Logging.h"

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
		UE_LOG(LogCarAITraining, Warning, TEXT("[NEATTrainingManager] Training already running!"));
		return;
	}

	if (Agents.Num() == 0)
	{
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] No agents registered!"));
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
	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] --- NEAT contract (source of truth) ---"));
	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager]   fitness_dir=%s"), *Contract.FitnessDir);
	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager]   genome_dir=%s"), *Contract.GenomeDir);
	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager]   checkpoint_dir=%s"), *Contract.CheckpointDir);
	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager]   best_genome_path=%s"), *Contract.BestGenomePath);
	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager]   observation_size=%d action_size=%d"), Contract.ObservationSize, Contract.ActionSize);
	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] NEAT observation schema: size=%d layout=SpeedNorm,YawRateNorm,PitchRateNorm,RollRateNorm,RayForward,RayLeft,RayRight,RayLeft45,RayRight45,RayForwardUp,RayForwardDown,RayGroundDist,GravityX,GravityY,GravityZ (LIDAR not used)"),
		FRacingObservation::NEAT_OBSERVATION_SIZE);
	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] --- end contract ---"));

	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Starting NEAT training"));
	UE_LOG(LogCarAITraining, Display, TEXT("  Generations: %d"), NumGenerations);
	UE_LOG(LogCarAITraining, Display, TEXT("  Population: %d"), PopulationSize);
	UE_LOG(LogCarAITraining, Display, TEXT("  Agents: %d"), Agents.Num());
	UE_LOG(LogCarAITraining, Display, TEXT("  TrainingMode: %s"), bFreshStart ? TEXT("FRESH (checkpoint + best-genome will be discarded by Python)") : TEXT("RESUME (checkpoint will be loaded if available)"));

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

	UE_LOG(LogCarAITraining, Warning, TEXT("[NEATTrainingManager] Stopping training..."));

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

	// Revoke evaluation authorization on all agents so they stop stepping immediately.
	for (TWeakObjectPtr<URacingAgentComponent>& WeakAgent : Agents)
	{
		if (URacingAgentComponent* Agent = WeakAgent.Get())
		{
			Agent->RevokeEvaluationAuthorization();
		}
	}
	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] StopTraining: evaluation authorization revoked on all %d agent(s)."), Agents.Num());

	TrainingState = ENEATTrainingState::Idle;
	bWaitingForPython = false;

	OnTrainingComplete.Broadcast();
}

void UNEATTrainingManager::PauseTraining()
{
	if (TrainingState == ENEATTrainingState::Evaluating)
	{
		TrainingState = ENEATTrainingState::Idle;
		UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Training paused"));
	}
}

void UNEATTrainingManager::ResumeTraining()
{
	if (TrainingState == ENEATTrainingState::Idle)
	{
		TrainingState = ENEATTrainingState::Evaluating;
		UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Training resumed"));
	}
}

// ============================================================================
// Agent Management
// ============================================================================

bool UNEATTrainingManager::RegisterAgent(URacingAgentComponent* Agent)
{
	// Passive registration: keep reference, bind delegate, do not Initialize or GrantEvaluationAuthorization.
	if (!Agent)
	{
		return false;
	}
	if (IsAgentRegistered(Agent))
	{
		UE_LOG(LogCarAITraining, Verbose, TEXT("[NEATTrainingManager] Agent already registered; skipping duplicate."));
		return false;
	}

	Agents.Add(Agent);
	Agent->OnEpisodeDone.AddDynamic(this, &UNEATTrainingManager::OnAgentEpisodeDone);

	// Ensure registered agents are unassigned (no genome/backend) until AssignGenomesToAgents runs.
	Agent->GenomeID = -1;
	Agent->SetPolicyBackend(nullptr);

	// Advance runtime state: Idle -> Registered. Does NOT grant evaluation authorization.
	// Evaluation starts only when genomes are loaded and GrantEvaluationAuthorization() is called.
	Agent->MarkRegistered();

	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Agent registered (RuntimeState=Registered; awaiting genome assignment and authorization). Total registered: %d"), Agents.Num());
	return true;
}

bool UNEATTrainingManager::IsAgentRegistered(URacingAgentComponent* Agent) const
{
	if (!Agent) return false;
	for (const TWeakObjectPtr<URacingAgentComponent>& Weak : Agents)
	{
		if (Weak.Get() == Agent) return true;
	}
	return false;
}

void UNEATTrainingManager::UnregisterAgent(URacingAgentComponent* Agent)
{
	if (!Agent)
	{
		return;
	}

	Agent->OnEpisodeDone.RemoveDynamic(this, &UNEATTrainingManager::OnAgentEpisodeDone);
	Agent->ResetToIdle();
	Agents.Remove(Agent);
	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Agent unregistered and reset to Idle."));
}

void UNEATTrainingManager::UnregisterAllAgents()
{
	for (TWeakObjectPtr<URacingAgentComponent>& WeakAgent : Agents)
	{
		if (URacingAgentComponent* Agent = WeakAgent.Get())
		{
			Agent->OnEpisodeDone.RemoveDynamic(this, &UNEATTrainingManager::OnAgentEpisodeDone);
			Agent->ResetToIdle();
		}
	}

	Agents.Empty();
	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] All agents unregistered and reset to Idle."));
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
	C.ObservationSize = FRacingObservation::NEAT_OBSERVATION_SIZE;
	if (ObservationSize != FRacingObservation::NEAT_OBSERVATION_SIZE)
	{
		UE_LOG(LogCarAITraining, Warning, TEXT("[NEATTrainingManager] ObservationSize %d ignored; NEAT contract uses canonical size %d (FRacingObservation::NEAT_OBSERVATION_SIZE)."),
			ObservationSize, FRacingObservation::NEAT_OBSERVATION_SIZE);
	}
	C.ActionSize = ActionSize;
	C.PopulationSize = PopulationSize;
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
			UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] Failed to create manifest dir: %s"), *ManifestDir);
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
	Root->SetNumberField(TEXT("population_size"), Contract.PopulationSize);
	// training_mode: "fresh" forces Python to discard checkpoint/best-genome before running;
	// "resume" loads the latest checkpoint (default). Set bFreshStart on this manager to control.
	Root->SetStringField(TEXT("training_mode"), bFreshStart ? TEXT("fresh") : TEXT("resume"));

	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Contract population parity: manifest population_size=%d (source: manager)."), Contract.PopulationSize);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
	{
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] Failed to serialize contract manifest"));
		return FString();
	}
	if (!FFileHelper::SaveStringToFile(JsonString, *ManifestPath))
	{
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] Failed to write manifest: %s"), *ManifestPath);
		return FString();
	}
	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Wrote contract manifest: %s"), *ManifestPath);
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
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] VALIDATION FAILED: Genome list not found: %s"), *GenomeListPath);
		return false;
	}

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *GenomeListPath))
	{
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] Failed to read genome list: %s"), *GenomeListPath);
		return false;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] Failed to parse genome list JSON"));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* GenomesArray;
	if (!JsonObject->TryGetArrayField(TEXT("genomes"), GenomesArray))
	{
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] No genomes field in JSON"));
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
			UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] VALIDATION FAILED: Missing genome file: %s (genome_id=%d)"), *GenomePath, GenomeID);
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
			UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] VALIDATION FAILED: LoadFromFile or parse failed: %s (genome_id=%d). Check unsupported activation / invalid JSON."), *GenomePath, GenomeID);
			return false;
		}
		if (!LoadedGenome.bIsValid)
		{
			UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] VALIDATION FAILED: Genome invalid (structure/activation): %s (genome_id=%d)"), *GenomePath, GenomeID);
			return false;
		}
		// Observation/action size must match contract (no silent mismatch)
		if (LoadedGenome.NumInputs != Contract.ObservationSize)
		{
			UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] VALIDATION FAILED: Observation size mismatch for genome_id=%d: genome has num_inputs=%d, contract ObservationSize=%d"),
				GenomeID, LoadedGenome.NumInputs, Contract.ObservationSize);
			return false;
		}
		if (LoadedGenome.NumOutputs != Contract.ActionSize)
		{
			UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] VALIDATION FAILED: Action size mismatch for genome_id=%d: genome has num_outputs=%d, contract ActionSize=%d"),
				GenomeID, LoadedGenome.NumOutputs, Contract.ActionSize);
			return false;
		}
		CurrentGenomes.Add(LoadedGenome);
		UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Genome loaded: %s (genome_id=%d, inputs=%d outputs=%d)"), *GenomePath, GenomeID, LoadedGenome.NumInputs, LoadedGenome.NumOutputs);
	}

	const int32 LoadedCount = CurrentGenomes.Num();
	if (LoadedCount != PopulationSize)
	{
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] Generation %d: loaded genome count %d does not match PopulationSize %d. Every genome must be evaluated; aborting to avoid invalid evolution."),
			CurrentGeneration, LoadedCount, PopulationSize);
		return false;
	}

	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Loaded %d genomes for generation %d (all validated, count matches PopulationSize); ready for batch evaluation"),
		LoadedCount, CurrentGeneration);

	LogNEATStatusSummary(TEXT("LoadGenomes"));

	return true;
}

bool UNEATTrainingManager::LoadGenomeFromJSON(const FString& FilePath, FNEATGenomeData& OutGenome)
{
	FNEATGenome LoadedGenome;
	if (!UNEATGenomeImporter::LoadFromFile(FilePath, LoadedGenome))
	{
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] LoadFromFile failed for: %s"), *FilePath);
		return false;
	}
	if (!LoadedGenome.bIsValid)
	{
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] Genome validation failed for: %s"), *FilePath);
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
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] AssignGenomesToAgents: no agents to assign (batch_start=%d, genomes=%d, agents=%d)"),
			CurrentBatchStartIndex, NumGenomes, NumAgents);
		return;
	}

	const int32 BatchEnd = CurrentBatchStartIndex + NumAgentsInCurrentBatch;
	const int32 TotalBatches = (NumGenomes + NumAgents - 1) / NumAgents;
	const int32 CurrentBatchIndex = (NumGenomes <= NumAgents) ? 0 : (CurrentBatchStartIndex / NumAgents);

	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Registered agents=%d. Active batch: agent indices [0..%d], size=%d (genomes [%d..%d))."),
		NumAgents, NumAgentsInCurrentBatch - 1, NumAgentsInCurrentBatch, CurrentBatchStartIndex, BatchEnd);
	LogGenerationEvaluationProgress(TEXT("batch_start"));
	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Batch %d/%d: evaluating genomes [%d..%d) of %d"),
		CurrentBatchIndex + 1, TotalBatches, CurrentBatchStartIndex, BatchEnd, NumGenomes);

	int32 AssignedCount = 0;
	int32 AssignmentFailCount = 0;
	for (int32 i = 0; i < NumAgentsInCurrentBatch; ++i)
	{
		URacingAgentComponent* Agent = Agents[i].Get();
		if (!Agent)
		{
			UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] AssignGenomesToAgents: agent %d is null"), i);
			AssignmentFailCount++;
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
			// Grant evaluation authorization only after genome AND backend are successfully assigned.
			Agent->GrantEvaluationAuthorization();
			AssignedCount++;
			UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Genome assigned and authorization granted: agent_index=%d genome_id=%d genome_index=%d RuntimeState=%d"),
				i, Genome.GenomeID, GenomeIndex, (int32)Agent->GetRuntimeState());
		}
		else
		{
			Agent->SetPolicyBackend(nullptr);
			AssignmentFailCount++;
			UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] Assignment failure: could not build NEAT evaluator for genome_id=%d (genome_index=%d); authorization NOT granted for agent %d."),
				Genome.GenomeID, GenomeIndex, i);
		}
	}

	if (AssignmentFailCount > 0)
	{
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] AssignGenomesToAgents: %d agent(s) failed assignment in this batch. These agents will not step (no authorization)."), AssignmentFailCount);
	}
	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] AssignGenomesToAgents complete: %d/%d agents authorized (genomes %d..%d)"),
		AssignedCount, NumAgentsInCurrentBatch, CurrentBatchStartIndex, BatchEnd - 1);

	// Deactivate agents outside the active batch so they cannot fire episode-done events.
	// Revoke authorization on inactive agents (ForceEpisodeDone already does this via state transition).
	const int32 TotalAgents = Agents.Num();
	int32 DeactivatedCount = 0;
	for (int32 i = NumAgentsInCurrentBatch; i < TotalAgents; ++i)
	{
		URacingAgentComponent* InactiveAgent = Agents.IsValidIndex(i) ? Agents[i].Get() : nullptr;
		if (InactiveAgent)
		{
			InactiveAgent->ForceEpisodeDone();
			DeactivatedCount++;
		}
	}
	if (DeactivatedCount > 0)
	{
		UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Batch isolation: %d inactive agents deactivated (authorization revoked)."), DeactivatedCount);
	}
}

void UNEATTrainingManager::StartEpisodeEvaluation()
{
	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] StartEpisodeEvaluation: Gen=%d BatchStart=%d BatchSize=%d"),
		CurrentGeneration, CurrentBatchStartIndex, NumAgentsInCurrentBatch);

	// Hard gate: no evaluation episode may start with GenomeID < 0, missing backend, or without authorization.
	int32 BadCount = 0;
	for (int32 i = 0; i < NumAgentsInCurrentBatch; ++i)
	{
		URacingAgentComponent* Agent = Agents.IsValidIndex(i) ? Agents[i].Get() : nullptr;
		if (Agent)
		{
			if (Agent->GenomeID < 0)
			{
				UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] GUARD: Agent %d has GenomeID < 0; cannot start evaluation. RuntimeState=%d"),
					i, (int32)Agent->GetRuntimeState());
				BadCount++;
			}
			else if (!Agent->HasNEATPolicyBackend())
			{
				UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] GUARD: Agent %d (genome_id=%d) has no policy backend; cannot start evaluation. RuntimeState=%d"),
					i, Agent->GenomeID, (int32)Agent->GetRuntimeState());
				BadCount++;
			}
			else if (!Agent->IsEvaluationAuthorized())
			{
				UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] GUARD: Agent %d (genome_id=%d) is NOT authorized for evaluation. RuntimeState=%d. GrantEvaluationAuthorization() must be called first."),
					i, Agent->GenomeID, (int32)Agent->GetRuntimeState());
				BadCount++;
			}
		}
	}
	if (BadCount > 0)
	{
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] Aborting StartEpisodeEvaluation: %d active batch agent(s) failed readiness gate."), BadCount);
		return;
	}

	// Initialize authorized agents. Initialize() transitions state to Evaluating and enables ticking.
	int32 InitializedCount = 0;
	for (int32 i = 0; i < NumAgentsInCurrentBatch; ++i)
	{
		URacingAgentComponent* Agent = Agents.IsValidIndex(i) ? Agents[i].Get() : nullptr;
		if (Agent && Agent->IsEvaluationAuthorized())
		{
			Agent->Initialize();
			InitializedCount++;
		}
	}
	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] StartEpisodeEvaluation: %d agent(s) initialized and now evaluating (Gen=%d, batch_start=%d, batch_size=%d)."),
		InitializedCount, CurrentGeneration, CurrentBatchStartIndex, NumAgentsInCurrentBatch);

	// Start evaluation timer
	EvaluationTimeElapsed = 0.f;

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			EvaluationTickTimer,
			FTimerDelegate::CreateUObject(this, &UNEATTrainingManager::TickEvaluation, 0.1f),
			0.1f,
			true
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
			LogGenerationEvaluationProgress(TEXT("batch_done"));
			if (!IsGenerationFullyEvaluated())
			{
				UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] Generation %d: fitness map has %d entries but %d genomes; missing genome IDs. No genome may be silently skipped. Aborting export and stopping training."),
					CurrentGeneration, GenomeFitnessMap.Num(), CurrentGenomes.Num());
				StopTraining();
				return;
			}
			UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Generation %d evaluation complete: all %d genomes evaluated; exporting fitness."), CurrentGeneration, CurrentGenomes.Num());

			ExportFitnessValues();
			CurrentGeneration++;
			TrainingStats.CurrentGeneration = CurrentGeneration;
			OnGenerationComplete.Broadcast(CurrentGeneration - 1);

			if (CurrentGeneration >= NumGenerations)
			{
				TrainingState = ENEATTrainingState::Completed;
				UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Training completed (%d generations); finalizing best genome from last evaluated generation"), NumGenerations);
				if (!FinalizeAndLoadBestGenome())
				{
					UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] FinalizeAndLoadBestGenome failed after training; best genome artifact is missing or invalid. Check generation_%d.json and genome files."), CurrentGeneration - 1);
				}
				OnTrainingComplete.Broadcast();
			}
			else
			{
				TriggerPythonEvolution();
			}
		}
		else
		{
			CurrentBatchStartIndex += NumAgentsInCurrentBatch;
			UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Batch complete; starting next batch: genome index from %d of %d"), CurrentBatchStartIndex, CurrentGenomes.Num());
			AssignGenomesToAgents();
			StartEpisodeEvaluation();
		}
	}
	else if (EvaluationTimeElapsed >= MaxEpisodeDuration)
	{
		UE_LOG(LogCarAITraining, Warning, TEXT("[NEATTrainingManager] Evaluation timeout (%.1fs) for current batch"), MaxEpisodeDuration);

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
			LogGenerationEvaluationProgress(TEXT("timeout_batch_done"));
			if (!IsGenerationFullyEvaluated())
			{
				UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] Generation %d (timeout): fitness map incomplete (%d/%d). No genome may be silently skipped. Aborting export and stopping training."),
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
				UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Training completed (%d generations); finalizing best genome from last evaluated generation"), NumGenerations);
				if (!FinalizeAndLoadBestGenome())
				{
					UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] FinalizeAndLoadBestGenome failed after training; best genome artifact is missing or invalid. Check generation_%d.json and genome files."), CurrentGeneration - 1);
				}
				OnTrainingComplete.Broadcast();
			}
			else
			{
				TriggerPythonEvolution();
			}
		}
		else
		{
			CurrentBatchStartIndex += NumAgentsInCurrentBatch;
			LogGenerationEvaluationProgress(TEXT("timeout_next_batch"));
			UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Timeout: starting next batch from genome index %d of %d"), CurrentBatchStartIndex, CurrentGenomes.Num());
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
	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] NEAT status [%s]: generation=%d, total_genomes=%d, completed_fitness=%d, exported_fitness_file=%s, loaded_best_genome=%s"),
		Phase, CurrentGeneration, CurrentGenomes.Num(), GenomeFitnessMap.Num(), *ExportedFile, *BestGenomeFile);
}

void UNEATTrainingManager::LogGenerationEvaluationProgress(const TCHAR* Phase) const
{
	const int32 Total = CurrentGenomes.Num();
	const int32 Completed = GenomeFitnessMap.Num();
	const int32 Pending = Total - Completed;
	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Generation evaluation progress [%s]: completed=%d total=%d pending=%d current_batch_agents=%d"),
		Phase, Completed, Total, Pending, NumAgentsInCurrentBatch);
}

void UNEATTrainingManager::OnAgentEpisodeDone(const FEpisodeStats& Stats)
{
	// Ignore events when not actively evaluating (can fire from force-ended or leftover episodes)
	if (TrainingState != ENEATTrainingState::Evaluating)
	{
		UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Ignoring episode-done event outside Evaluating state (agent_id=%d, genome_id ignored)"), Stats.AgentInstanceID);
		return;
	}

	// Match by AgentInstanceID within the active batch only (indices [0..NumAgentsInCurrentBatch-1])
	for (int32 i = 0; i < NumAgentsInCurrentBatch; ++i)
	{
		URacingAgentComponent* Agent = Agents.IsValidIndex(i) ? Agents[i].Get() : nullptr;
		if (!Agent)
		{
			continue;
		}
		if ((int32)Agent->GetUniqueID() != Stats.AgentInstanceID)
		{
			continue;
		}

		// Unambiguous match: this active-batch agent produced this episode result
		GenomeFitnessMap.Add(Agent->GenomeID, Stats.NEATFitness);
		UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Fitness attributed: agent_id=%d genome_id=%d fitness=%.2f termination=%s"),
			Stats.AgentInstanceID, Agent->GenomeID, Stats.NEATFitness, *Stats.TerminationReason);

		if (Stats.NEATFitness > TrainingStats.BestFitness)
		{
			TrainingStats.BestFitness = Stats.NEATFitness;
			TrainingStats.BestGenomeID = Agent->GenomeID;
			OnNewBestGenome.Broadcast(Agent->GenomeID, Stats.NEATFitness);
			UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] New best genome: genome_id=%d fitness=%.2f"), Agent->GenomeID, Stats.NEATFitness);
		}

		TrainingStats.TotalEvaluations++;
		return;
	}

	// No match in active batch: reject with explicit log
	UE_LOG(LogCarAITraining, Warning, TEXT("[NEATTrainingManager] Episode-done event rejected: agent_id=%d not found in active batch (batch_size=%d, total_agents=%d). This may be a deactivated inactive agent."),
		Stats.AgentInstanceID, NumAgentsInCurrentBatch, Agents.Num());
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
			UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] ExportFitnessValues: genome_id=%d has no fitness (partial export forbidden)."), G.GenomeID);
			return;
		}
	}

	const FNEATTrainingContract Contract = GetResolvedContract();
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*Contract.FitnessDir))
	{
		if (!PlatformFile.CreateDirectoryTree(*Contract.FitnessDir))
		{
			UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] Failed to create fitness dir: %s"), *Contract.FitnessDir);
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
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] VALIDATION FAILED: Failed to write fitness file: %s"), *OutputPath);
		return;
	}

	if (!PlatformFile.FileExists(*OutputPath))
	{
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] VALIDATION FAILED: Fitness file not found after write: %s"), *OutputPath);
		return;
	}

	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] HARD GATE: fitness export verified at %s (%d genomes)"), *OutputPath, GenomeFitnessMap.Num());

	LastExportedFitnessFilePath = OutputPath;
	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Exported fitness for generation %d (%d genomes, Avg=%.2f) -> %s"),
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
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] No Python executor!"));
		return;
	}

	const FNEATTrainingContract Contract = GetResolvedContract();
	const FString ManifestPath = WriteContractManifest(Contract);
	if (ManifestPath.IsEmpty())
	{
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] Cannot run Python without contract manifest"));
		return;
	}

	TrainingState = ENEATTrainingState::WaitingForPython;
	bWaitingForPython = true;

	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Python/evolution triggered (generation %d, manifest=%s)"), CurrentGeneration, *ManifestPath);

	PythonExecutor->ExecuteTrainingAsync(PythonScriptPath, PythonExecutable, ManifestPath);
}

int32 UNEATTrainingManager::ReadExportedGeneration() const
{
	const FNEATTrainingContract Contract = GetResolvedContract();
	const FString StatePath = FPaths::Combine(Contract.GenomeDir, FNEATTrainingContract::TrainingStateFileName);

	if (!FPaths::FileExists(StatePath))
	{
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] VALIDATION FAILED: training_state.json not found: %s. Python must write this file after every generation export."), *StatePath);
		return -1;
	}

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *StatePath))
	{
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] Failed to read training_state.json: %s"), *StatePath);
		return -1;
	}

	TSharedPtr<FJsonObject> Obj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
	{
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] Failed to parse training_state.json: %s"), *StatePath);
		return -1;
	}

	if (!Obj->HasField(TEXT("exported_generation")))
	{
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] VALIDATION FAILED: training_state.json missing 'exported_generation' field: %s"), *StatePath);
		return -1;
	}

	const int32 ExportedGen = Obj->GetIntegerField(TEXT("exported_generation"));
	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] training_state.json: exported_generation=%d (path=%s)"), ExportedGen, *StatePath);
	return ExportedGen;
}

void UNEATTrainingManager::OnPythonEvolutionComplete(bool bSuccess)
{
	bWaitingForPython = false;

	if (!bSuccess)
	{
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] Python evolution failed!"));
		StopTraining();
		return;
	}

	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Python evolution complete"));

	// Synchronize CurrentGeneration from training_state.json written by Python.
	// This is the canonical source of truth for which generation Python just exported.
	// Without this sync, Unreal would use its own CurrentGeneration (which starts at 0
	// at every session), causing resume runs to load the wrong generation.
	{
		const int32 ExportedGen = ReadExportedGeneration();
		if (ExportedGen < 0)
		{
			UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] HARD GATE: training_state.json is missing or invalid; cannot determine exported generation. Stopping training."));
			StopTraining();
			return;
		}
		if (ExportedGen != CurrentGeneration)
		{
			UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Generation sync: CurrentGeneration %d -> %d (from training_state.json; likely a resumed run)"), CurrentGeneration, ExportedGen);
		}
		else
		{
			UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Generation sync: CurrentGeneration=%d matches exported_generation=%d (no correction needed)"), CurrentGeneration, ExportedGen);
		}
		CurrentGeneration = ExportedGen;
	}

	// Hard validate that the genome list file matches the synchronized generation.
	// This confirms training_state.json and the actual genome export files agree before
	// committing to LoadGenerationGenomes(). Prevents silent load of wrong generation.
	{
		const FNEATTrainingContract Contract = GetResolvedContract();
		const FString ExpectedGenomeList = FPaths::Combine(Contract.GenomeDir,
			FString::Printf(TEXT("generation_%d_genomes.json"), CurrentGeneration));
		if (!FPaths::FileExists(ExpectedGenomeList))
		{
			UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] HARD GATE: training_state.json says exported_generation=%d but genome list not found: %s. Python export may be incomplete or paths mismatched."),
				CurrentGeneration, *ExpectedGenomeList);
			StopTraining();
			return;
		}
		UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Import handshake confirmed: training_state exported_generation=%d, genome list exists -> loading generation %d"),
			CurrentGeneration, CurrentGeneration);
	}

	// Load new genomes (validation: missing files, observation/action size, activation)
	if (!LoadGenerationGenomes())
	{
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] Failed to load new genomes (see validation errors above)."));
		StopTraining();
		return;
	}

	LogNEATStatusSummary(TEXT("AfterPythonLoad"));

	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Python evolution complete: generation=%d, %d genomes loaded, %d agents registered"),
		CurrentGeneration, CurrentGenomes.Num(), Agents.Num());

	CurrentBatchStartIndex = 0;
	if (CurrentGenomes.Num() > Agents.Num())
	{
		const int32 NumBatches = (CurrentGenomes.Num() + Agents.Num() - 1) / Agents.Num();
		UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Batch mode: %d genomes, %d agents; evaluating in %d batch(es)"), CurrentGenomes.Num(), Agents.Num(), NumBatches);
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
		UE_LOG(LogCarAITraining, Warning, TEXT("[NEATTrainingManager] Best genome file not found: %s"), *BestGenomePath);
		return false;
	}

	FNEATGenome BestGenome;
	if (!UNEATGenomeImporter::LoadFromFile(BestGenomePath, BestGenome))
	{
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] LoadBestGenome: LoadFromFile failed for %s"), *BestGenomePath);
		return false;
	}
	if (!BestGenome.bIsValid)
	{
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] LoadBestGenome: genome validation failed for %s"), *BestGenomePath);
		return false;
	}

	// Validation: observation/action size must match contract
	if (BestGenome.NumInputs != Contract.ObservationSize)
	{
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] VALIDATION FAILED: Best genome observation size mismatch: genome num_inputs=%d, contract ObservationSize=%d"),
			BestGenome.NumInputs, Contract.ObservationSize);
		return false;
	}
	if (BestGenome.NumOutputs != Contract.ActionSize)
	{
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] VALIDATION FAILED: Best genome action size mismatch: genome num_outputs=%d, contract ActionSize=%d"),
			BestGenome.NumOutputs, Contract.ActionSize);
		return false;
	}

	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Best genome loaded: %s (ID=%d, Gen=%d, Fitness=%.2f); building NEAT runtime backend"),
		*BestGenomePath, BestGenome.GenomeID, BestGenome.Generation, BestGenome.Fitness);

	UNEATGraphEvaluator* Evaluator = UNEATGraphEvaluator::CreateFromGenome(this, BestGenome);
	if (!Evaluator)
	{
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] LoadBestGenome failed: could not build NEAT evaluator for best genome"));
		return false;
	}

	if (Agents.Num() == 0)
	{
		UE_LOG(LogCarAITraining, Warning, TEXT("[NEATTrainingManager] LoadBestGenome: no agents registered; best genome loaded but not assigned"));
		LastLoadedBestGenomePath = BestGenomePath;
		return true;
	}

	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] NEAT runtime backend built for best genome; assigning to %d agent(s)"), Agents.Num());

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
		UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Best genome assigned: NEAT runtime backend set on agent (GenomeID=%d)"), BestGenome.GenomeID);
	}

	LastLoadedBestGenomePath = BestGenomePath;
	LogNEATStatusSummary(TEXT("LoadBestGenome"));
	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] LoadBestGenome complete: assigned to %d agent(s)"), AssignedCount);
	return true;
}

bool UNEATTrainingManager::FinalizeAndLoadBestGenome()
{
	// CurrentGeneration has already been incremented by the time this is called;
	// the last evaluated generation is CurrentGeneration - 1.
	const int32 LastEvaluatedGen = CurrentGeneration - 1;
	const FNEATTrainingContract Contract = GetResolvedContract();

	// Read the fitness file that ExportFitnessValues() just wrote for this generation
	const FString FitnessFilePath = FPaths::Combine(Contract.FitnessDir,
		FString::Printf(TEXT("generation_%d.json"), LastEvaluatedGen));

	if (!FPaths::FileExists(FitnessFilePath))
	{
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] FinalizeAndLoadBestGenome: fitness file not found for generation %d: %s"),
			LastEvaluatedGen, *FitnessFilePath);
		return false;
	}

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FitnessFilePath))
	{
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] FinalizeAndLoadBestGenome: failed to read fitness file: %s"), *FitnessFilePath);
		return false;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] FinalizeAndLoadBestGenome: failed to parse fitness JSON: %s"), *FitnessFilePath);
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* GenomesArray;
	if (!JsonObject->TryGetArrayField(TEXT("genomes"), GenomesArray) || GenomesArray->Num() == 0)
	{
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] FinalizeAndLoadBestGenome: no genomes in fitness file: %s"), *FitnessFilePath);
		return false;
	}

	int32 BestGenomeID = -1;
	float BestFitness = -FLT_MAX;
	for (const TSharedPtr<FJsonValue>& Val : *GenomesArray)
	{
		const TSharedPtr<FJsonObject>* Obj;
		if (!Val->TryGetObject(Obj)) continue;
		const int32 GID = (*Obj)->GetIntegerField(TEXT("genome_id"));
		const float Fit = static_cast<float>((*Obj)->GetNumberField(TEXT("fitness")));
		if (Fit > BestFitness)
		{
			BestFitness = Fit;
			BestGenomeID = GID;
		}
	}

	if (BestGenomeID < 0)
	{
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] FinalizeAndLoadBestGenome: could not determine best genome_id from %s"), *FitnessFilePath);
		return false;
	}

	const FString SourcePath = FPaths::Combine(Contract.GenomeDir,
		FString::Printf(TEXT("genome_%d.json"), BestGenomeID));
	if (!FPaths::FileExists(SourcePath))
	{
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] FinalizeAndLoadBestGenome: genome file not found: %s (genome_id=%d generation=%d)"),
			*SourcePath, BestGenomeID, LastEvaluatedGen);
		return false;
	}

	// Copy genome file to the canonical best_genome_path from the contract
	const FString& DestPath = Contract.BestGenomePath;
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const FString DestDir = FPaths::GetPath(DestPath);
	if (!PlatformFile.DirectoryExists(*DestDir))
	{
		PlatformFile.CreateDirectoryTree(*DestDir);
	}
	if (!PlatformFile.CopyFile(*DestPath, *SourcePath))
	{
		UE_LOG(LogCarAITraining, Error, TEXT("[NEATTrainingManager] FinalizeAndLoadBestGenome: failed to copy %s -> %s"), *SourcePath, *DestPath);
		return false;
	}

	UE_LOG(LogCarAITraining, Display, TEXT("[NEATTrainingManager] Best genome artifact finalized: generation=%d genome_id=%d fitness=%.2f -> %s"),
		LastEvaluatedGen, BestGenomeID, BestFitness, *DestPath);

	return LoadBestGenome();
}
