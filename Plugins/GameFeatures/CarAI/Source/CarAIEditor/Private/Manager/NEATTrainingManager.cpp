#include "Manager/NEATTrainingManager.h"
#include "Components/RacingAgentComponent.h"
#include "Manager/PythonTrainingExecutor.h"
#include "NN/SimpleNeuralNetwork.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
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

	UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] Starting NEAT training"));
	UE_LOG(LogTemp, Log, TEXT("  Generations: %d"), NumGenerations);
	UE_LOG(LogTemp, Log, TEXT("  Population: %d"), PopulationSize);
	UE_LOG(LogTemp, Log, TEXT("  Agents: %d"), Agents.Num());

	// Start first generation
	TrainingState = ENEATTrainingState::Evaluating;

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

	// Bind episode done event
	Agent->OnEpisodeDone.AddDynamic(this, &UNEATTrainingManager::OnAgentEpisodeDone);

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
// Generation Cycle
// ============================================================================

bool UNEATTrainingManager::LoadGenerationGenomes()
{
	// Load genome list from Python output
	FString GenomeListPath = FPaths::Combine(GenomeInputDir,
		FString::Printf(TEXT("generation_%d_genomes.json"), CurrentGeneration));

	if (!FPaths::FileExists(GenomeListPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("[NEATTrainingManager] Genome list not found: %s"), *GenomeListPath);
		return false;
	}

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *GenomeListPath))
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] Failed to read genome list"));
		return false;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] Failed to parse genome list JSON"));
		return false;
	}

	// Get genome list
	const TArray<TSharedPtr<FJsonValue>>* GenomesArray;
	if (!JsonObject->TryGetArrayField(TEXT("genomes"), GenomesArray))
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] No genomes field in JSON"));
		return false;
	}

	CurrentGenomes.Empty();

	// Load each genome
	for (const TSharedPtr<FJsonValue>& GenomeValue : *GenomesArray)
	{
		const TSharedPtr<FJsonObject>* GenomeObj;
		if (!GenomeValue->TryGetObject(GenomeObj))
		{
			continue;
		}

		int32 GenomeID = (*GenomeObj)->GetIntegerField(TEXT("genome_id"));

		// Load full genome data from separate file
		FString GenomePath = FPaths::Combine(GenomeInputDir,
			FString::Printf(TEXT("genome_%d.json"), GenomeID));

		FNEATGenomeData GenomeData;
		if (LoadGenomeFromJSON(GenomePath, GenomeData))
		{
			CurrentGenomes.Add(GenomeData);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] Loaded %d genomes for generation %d"),
		CurrentGenomes.Num(), CurrentGeneration);

	return CurrentGenomes.Num() > 0;
}

bool UNEATTrainingManager::LoadGenomeFromJSON(const FString& FilePath, FNEATGenomeData& OutGenome)
{
	if (!FPaths::FileExists(FilePath))
	{
		return false;
	}

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		return false;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return false;
	}

	// Parse genome data
	OutGenome.GenomeID = JsonObject->GetIntegerField(TEXT("genome_id"));
	OutGenome.Generation = JsonObject->GetIntegerField(TEXT("generation"));
	OutGenome.Fitness = JsonObject->GetNumberField(TEXT("fitness"));

	// Parse nodes
	const TArray<TSharedPtr<FJsonValue>>* NodesArray;
	if (JsonObject->TryGetArrayField(TEXT("nodes"), NodesArray))
	{
		for (const TSharedPtr<FJsonValue>& NodeValue : *NodesArray)
		{
			const TSharedPtr<FJsonObject>* NodeObj;
			if (NodeValue->TryGetObject(NodeObj))
			{
				int32 NodeID = (*NodeObj)->GetIntegerField(TEXT("id"));
				OutGenome.NodeIDs.Add(NodeID);

				// Store activation function
				FString Activation = (*NodeObj)->GetStringField(TEXT("activation"));
				OutGenome.Activations.Add(Activation);
			}
		}
	}

	// Parse connections
	const TArray<TSharedPtr<FJsonValue>>* ConnectionsArray;
	if (JsonObject->TryGetArrayField(TEXT("connections"), ConnectionsArray))
	{
		for (const TSharedPtr<FJsonValue>& ConnValue : *ConnectionsArray)
		{
			const TSharedPtr<FJsonObject>* ConnObj;
			if (ConnValue->TryGetObject(ConnObj))
			{
				int32 InNode = (*ConnObj)->GetIntegerField(TEXT("in_node"));
				int32 OutNode = (*ConnObj)->GetIntegerField(TEXT("out_node"));
				float Weight = (*ConnObj)->GetNumberField(TEXT("weight"));
				bool bEnabled = (*ConnObj)->GetBoolField(TEXT("enabled"));

				FString ConnStr = FString::Printf(TEXT("%d,%d,%.4f,%d"),
					InNode, OutNode, Weight, bEnabled ? 1 : 0);
				OutGenome.Connections.Add(ConnStr);
			}
		}
	}

	return true;
}

void UNEATTrainingManager::AssignGenomesToAgents()
{
	int32 AssignedCount = 0;

	for (int32 i = 0; i < Agents.Num() && i < CurrentGenomes.Num(); ++i)
	{
		URacingAgentComponent* Agent = Agents[i].Get();
		if (!Agent)
		{
			continue;
		}

		const FNEATGenomeData& Genome = CurrentGenomes[i];

		// Assign genome ID
		Agent->GenomeID = Genome.GenomeID;
		Agent->Generation = CurrentGeneration;

		// TODO: Load genome into SimpleNeuralNetwork
		// For now, agents will use random/heuristic actions
		// Full NEAT genome loading would require NEAT-compatible network builder

		AssignedCount++;
	}

	UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] Assigned %d genomes to agents"), AssignedCount);
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

	// Check if all agents are done
	if (AreAllAgentsDone())
	{
		// Stop evaluation
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().ClearTimer(EvaluationTickTimer);
		}

		// Export fitness
		ExportFitnessValues();

		// Move to next generation
		CurrentGeneration++;
		TrainingStats.CurrentGeneration = CurrentGeneration;

		OnGenerationComplete.Broadcast(CurrentGeneration - 1);

		if (CurrentGeneration >= NumGenerations)
		{
			// Training complete!
			TrainingState = ENEATTrainingState::Completed;
			OnTrainingComplete.Broadcast();

			UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] Training completed! (%d generations)"), NumGenerations);
		}
		else
		{
			// Trigger Python evolution for next generation
			TriggerPythonEvolution();
		}
	}
	else if (EvaluationTimeElapsed >= MaxEpisodeDuration)
	{
		// Timeout - force all agents done
		UE_LOG(LogTemp, Warning, TEXT("[NEATTrainingManager] Evaluation timeout (%.1fs)"), MaxEpisodeDuration);

		for (TWeakObjectPtr<URacingAgentComponent>& WeakAgent : Agents)
		{
			if (URacingAgentComponent* Agent = WeakAgent.Get())
			{
				if (!Agent->IsDone())
				{
					// Force episode end
					FEpisodeStats Stats = Agent->GetEpisodeStats();
					Stats.TerminationReason = TEXT("Timeout");
					Stats.CalculateNEATFitness();

					// Record fitness
					GenomeFitnessMap.Add(Agent->GenomeID, Stats.NEATFitness);
				}
			}
		}

		// Export and continue
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().ClearTimer(EvaluationTickTimer);
		}

		ExportFitnessValues();
		CurrentGeneration++;
		TriggerPythonEvolution();
	}
}

bool UNEATTrainingManager::AreAllAgentsDone() const
{
	for (const TWeakObjectPtr<URacingAgentComponent>& WeakAgent : Agents)
	{
		if (URacingAgentComponent* Agent = WeakAgent.Get())
		{
			if (!Agent->IsDone())
			{
				return false;
			}
		}
	}

	return true;
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
	// Create export directory
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*FitnessExportDir))
	{
		PlatformFile.CreateDirectoryTree(*FitnessExportDir);
	}

	// Build JSON
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
	FString OutputPath = FPaths::Combine(FitnessExportDir,
		FString::Printf(TEXT("generation_%d.json"), CurrentGeneration));

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

	if (FFileHelper::SaveStringToFile(JsonString, *OutputPath))
	{
		UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] Exported fitness for generation %d (%d genomes, Avg=%.2f)"),
			CurrentGeneration, GenomeFitnessMap.Num(), TrainingStats.AvgFitness);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] Failed to export fitness!"));
	}

	// Clear for next generation
	GenomeFitnessMap.Empty();
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

	TrainingState = ENEATTrainingState::WaitingForPython;
	bWaitingForPython = true;

	UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] Triggering Python evolution..."));

	// Build command line arguments
	// python train_neat.py [fitness_dir] [output_dir] [num_generations]
	FString Args = FString::Printf(TEXT("%s %s %d"),
		*FitnessExportDir,
		*GenomeInputDir,
		1  // Process 1 generation at a time
	);

	// Execute Python script
	PythonExecutor->ExecuteTrainingAsync(PythonScriptPath, PythonExecutable, 1);
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

	// Load new genomes
	if (!LoadGenerationGenomes())
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] Failed to load new genomes!"));
		StopTraining();
		return;
	}

	// Assign to agents
	AssignGenomesToAgents();

	// Start evaluation
	TrainingState = ENEATTrainingState::Evaluating;
	StartEpisodeEvaluation();
}

bool UNEATTrainingManager::LoadBestGenome()
{
	FString BestGenomePath = FPaths::Combine(GenomeInputDir, TEXT("best_genome.json"));

	if (!FPaths::FileExists(BestGenomePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("[NEATTrainingManager] Best genome not found: %s"), *BestGenomePath);
		return false;
	}

	FNEATGenomeData BestGenome;
	if (!LoadGenomeFromJSON(BestGenomePath, BestGenome))
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATTrainingManager] Failed to load best genome"));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[NEATTrainingManager] Loaded best genome: ID=%d, Gen=%d, Fitness=%.2f"),
		BestGenome.GenomeID, BestGenome.Generation, BestGenome.Fitness);

	// TODO: Convert NEAT genome to SimpleNeuralNetwork
	// This would require a NEAT-to-MLP converter or using a NEAT-compatible network

	return true;
}