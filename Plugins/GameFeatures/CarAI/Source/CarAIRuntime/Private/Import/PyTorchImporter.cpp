#include "Import/PyTorchImporter.h"
#include "NN/SimpleNeuralNetwork.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

bool UPyTorchImporter::ImportPyTorchModel(const FString& PyTorchModelPath, USimpleNeuralNetwork* TargetNetwork)
{
	if (!TargetNetwork)
	{
		UE_LOG(LogTemp, Error, TEXT("PyTorchImporter: TargetNetwork is null!"));
		return false;
	}

	if (!DoesModelExist(PyTorchModelPath))
	{
		UE_LOG(LogTemp, Error, TEXT("PyTorchImporter: Model file does not exist: %s"), *PyTorchModelPath);
		return false;
	}

	// Lade JSON-Datei (PyTorch-Modell wurde bereits zu JSON konvertiert)
	return LoadWeightsFromJSON(PyTorchModelPath, TargetNetwork);
}

bool UPyTorchImporter::DoesModelExist(const FString& ModelPath) const
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	return PlatformFile.FileExists(*ModelPath);
}

bool UPyTorchImporter::LoadWeightsFromJSON(const FString& JSONPath, USimpleNeuralNetwork* TargetNetwork)
{
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *JSONPath))
	{
		UE_LOG(LogTemp, Error, TEXT("PyTorchImporter: Failed to read JSON file: %s"), *JSONPath);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("PyTorchImporter: Failed to parse JSON file: %s"), *JSONPath);
		return false;
	}

	// Lade Network Config
	const TSharedPtr<FJsonObject>* NetworkConfigObj;
	if (!RootObject->TryGetObjectField(TEXT("network_config"), NetworkConfigObj))
	{
		UE_LOG(LogTemp, Error, TEXT("PyTorchImporter: Missing 'network_config' in JSON"));
		return false;
	}

	FNetworkConfig NetworkConfig;
	NetworkConfig.InputSize = (*NetworkConfigObj)->GetIntegerField(TEXT("input_size"));
	
	const TArray<TSharedPtr<FJsonValue>>* HiddenLayersArray;
	if ((*NetworkConfigObj)->TryGetArrayField(TEXT("hidden_layers"), HiddenLayersArray))
	{
		NetworkConfig.HiddenLayers.Reset();
		for (const TSharedPtr<FJsonValue>& Val : *HiddenLayersArray)
		{
			FDenseLayerConfig LayerConfig;
			LayerConfig.OutputSize = Val->AsNumber();
			LayerConfig.Activation = EActivationType::ReLU; // Default
			NetworkConfig.HiddenLayers.Add(LayerConfig);
		}
	}

	NetworkConfig.PolicyOutputSize = (*NetworkConfigObj)->GetIntegerField(TEXT("policy_output_size"));
	NetworkConfig.ValueOutputSize = (*NetworkConfigObj)->GetIntegerField(TEXT("value_output_size"));

	// Initialisiere Network falls noch nicht initialisiert
	if (!TargetNetwork->IsInitialized())
	{
		TargetNetwork->Initialize(NetworkConfig, 0);
	}

	// Lade Policy Layers
	const TArray<TSharedPtr<FJsonValue>>* PolicyLayersArray;
	if (RootObject->TryGetArrayField(TEXT("policy_layers"), PolicyLayersArray))
	{
		for (int32 i = 0; i < PolicyLayersArray->Num() && i < TargetNetwork->NetworkConfig.HiddenLayers.Num(); ++i)
		{
			const TSharedPtr<FJsonObject>* LayerObj;
			if ((*PolicyLayersArray)[i]->TryGetObject(LayerObj))
			{
				const TArray<TSharedPtr<FJsonValue>>* WeightsArray;
				const TArray<TSharedPtr<FJsonValue>>* BiasesArray;
				
				if ((*LayerObj)->TryGetArrayField(TEXT("weights"), WeightsArray) &&
					(*LayerObj)->TryGetArrayField(TEXT("biases"), BiasesArray))
				{
					TArray<float> Weights;
					TArray<float> Biases;
					
					// Gewichte können verschachtelt sein (2D-Array) - flache sie
					for (const TSharedPtr<FJsonValue>& RowVal : *WeightsArray)
					{
						if (RowVal->Type == EJson::Array)
						{
							// Verschachteltes Array (2D)
							const TArray<TSharedPtr<FJsonValue>>* RowArray;
							if (RowVal->TryGetArray(RowArray))
							{
								for (const TSharedPtr<FJsonValue>& Val : *RowArray)
								{
									Weights.Add(static_cast<float>(Val->AsNumber()));
								}
							}
						}
						else
						{
							// Flaches Array (1D)
							Weights.Add(static_cast<float>(RowVal->AsNumber()));
						}
					}
					
					// Biases sind immer 1D
					for (const TSharedPtr<FJsonValue>& Val : *BiasesArray)
					{
						Biases.Add(static_cast<float>(Val->AsNumber()));
					}
					
					UE_LOG(LogTemp, Log, TEXT("PyTorchImporter: Policy Layer %d - Weights: %d, Biases: %d"), i, Weights.Num(), Biases.Num());
					TargetNetwork->SetPolicyLayerWeights(i, Weights, Biases);
				}
			}
		}
	}

	// Lade Value Layers
	const TArray<TSharedPtr<FJsonValue>>* ValueLayersArray;
	if (RootObject->TryGetArrayField(TEXT("value_layers"), ValueLayersArray))
	{
		for (int32 i = 0; i < ValueLayersArray->Num() && i < TargetNetwork->NetworkConfig.HiddenLayers.Num(); ++i)
		{
			const TSharedPtr<FJsonObject>* LayerObj;
			if ((*ValueLayersArray)[i]->TryGetObject(LayerObj))
			{
				const TArray<TSharedPtr<FJsonValue>>* WeightsArray;
				const TArray<TSharedPtr<FJsonValue>>* BiasesArray;
				
				if ((*LayerObj)->TryGetArrayField(TEXT("weights"), WeightsArray) &&
					(*LayerObj)->TryGetArrayField(TEXT("biases"), BiasesArray))
				{
					TArray<float> Weights;
					TArray<float> Biases;
					
					// Gewichte können verschachtelt sein (2D-Array) - flache sie
					// Prüfe zuerst, ob das erste Element ein Array ist
					if (WeightsArray->Num() > 0)
					{
						const TSharedPtr<FJsonValue>& FirstVal = (*WeightsArray)[0];
						if (FirstVal->Type == EJson::Array)
						{
							// Verschachteltes Array (2D) - lese alle Zeilen
							for (const TSharedPtr<FJsonValue>& RowVal : *WeightsArray)
							{
								const TArray<TSharedPtr<FJsonValue>>* RowArray;
								if (RowVal->TryGetArray(RowArray))
								{
									for (const TSharedPtr<FJsonValue>& Val : *RowArray)
									{
										if (Val->Type == EJson::Number)
										{
											Weights.Add(static_cast<float>(Val->AsNumber()));
										}
									}
								}
							}
						}
						else
						{
							// Flaches Array (1D)
							for (const TSharedPtr<FJsonValue>& Val : *WeightsArray)
							{
								if (Val->Type == EJson::Number)
								{
									Weights.Add(static_cast<float>(Val->AsNumber()));
								}
							}
						}
					}
					
					// Biases sind immer 1D
					for (const TSharedPtr<FJsonValue>& Val : *BiasesArray)
					{
						if (Val->Type == EJson::Number)
						{
							Biases.Add(static_cast<float>(Val->AsNumber()));
						}
					}
					
					UE_LOG(LogTemp, Log, TEXT("PyTorchImporter: Value Layer %d - Weights: %d, Biases: %d"), i, Weights.Num(), Biases.Num());
					TargetNetwork->SetValueLayerWeights(i, Weights, Biases);
				}
			}
		}
	}

	// Lade Policy Head
	const TSharedPtr<FJsonObject>* PolicyHeadObj;
	if (RootObject->TryGetObjectField(TEXT("policy_head"), PolicyHeadObj))
	{
		const TArray<TSharedPtr<FJsonValue>>* WeightsArray;
		const TArray<TSharedPtr<FJsonValue>>* BiasesArray;
		
		if ((*PolicyHeadObj)->TryGetArrayField(TEXT("weights"), WeightsArray) &&
			(*PolicyHeadObj)->TryGetArrayField(TEXT("biases"), BiasesArray))
		{
			TArray<float> Weights;
			TArray<float> Biases;
			
			// Gewichte können verschachtelt sein (2D-Array) - flache sie
			// Prüfe zuerst, ob das erste Element ein Array ist
			if (WeightsArray->Num() > 0)
			{
				const TSharedPtr<FJsonValue>& FirstVal = (*WeightsArray)[0];
				if (FirstVal->Type == EJson::Array)
				{
					// Verschachteltes Array (2D) - lese alle Zeilen
					for (const TSharedPtr<FJsonValue>& RowVal : *WeightsArray)
					{
						const TArray<TSharedPtr<FJsonValue>>* RowArray;
						if (RowVal->TryGetArray(RowArray))
						{
							for (const TSharedPtr<FJsonValue>& Val : *RowArray)
							{
								if (Val->Type == EJson::Number)
								{
									Weights.Add(static_cast<float>(Val->AsNumber()));
								}
							}
						}
					}
				}
				else
				{
					// Flaches Array (1D)
					for (const TSharedPtr<FJsonValue>& Val : *WeightsArray)
					{
						if (Val->Type == EJson::Number)
						{
							Weights.Add(static_cast<float>(Val->AsNumber()));
						}
					}
				}
			}
			
			// Biases sind immer 1D
			for (const TSharedPtr<FJsonValue>& Val : *BiasesArray)
			{
				if (Val->Type == EJson::Number)
				{
					Biases.Add(static_cast<float>(Val->AsNumber()));
				}
			}
			
			UE_LOG(LogTemp, Log, TEXT("PyTorchImporter: Policy Head - Weights: %d, Biases: %d"), Weights.Num(), Biases.Num());
			TargetNetwork->SetPolicyHeadWeights(Weights, Biases);
		}
	}

	// Lade Value Head
	const TSharedPtr<FJsonObject>* ValueHeadObj;
	if (RootObject->TryGetObjectField(TEXT("value_head"), ValueHeadObj))
	{
		const TArray<TSharedPtr<FJsonValue>>* WeightsArray;
		const TArray<TSharedPtr<FJsonValue>>* BiasesArray;
		
		if ((*ValueHeadObj)->TryGetArrayField(TEXT("weights"), WeightsArray) &&
			(*ValueHeadObj)->TryGetArrayField(TEXT("biases"), BiasesArray))
		{
			TArray<float> Weights;
			TArray<float> Biases;
			
			// Gewichte können verschachtelt sein (2D-Array) - flache sie
			// Prüfe zuerst, ob das erste Element ein Array ist
			if (WeightsArray->Num() > 0)
			{
				const TSharedPtr<FJsonValue>& FirstVal = (*WeightsArray)[0];
				if (FirstVal->Type == EJson::Array)
				{
					// Verschachteltes Array (2D) - lese alle Zeilen
					for (const TSharedPtr<FJsonValue>& RowVal : *WeightsArray)
					{
						const TArray<TSharedPtr<FJsonValue>>* RowArray;
						if (RowVal->TryGetArray(RowArray))
						{
							for (const TSharedPtr<FJsonValue>& Val : *RowArray)
							{
								if (Val->Type == EJson::Number)
								{
									Weights.Add(static_cast<float>(Val->AsNumber()));
								}
							}
						}
					}
				}
				else
				{
					// Flaches Array (1D)
					for (const TSharedPtr<FJsonValue>& Val : *WeightsArray)
					{
						if (Val->Type == EJson::Number)
						{
							Weights.Add(static_cast<float>(Val->AsNumber()));
						}
					}
				}
			}
			
			// Biases sind immer 1D
			for (const TSharedPtr<FJsonValue>& Val : *BiasesArray)
			{
				if (Val->Type == EJson::Number)
				{
					Biases.Add(static_cast<float>(Val->AsNumber()));
				}
			}
			
			UE_LOG(LogTemp, Log, TEXT("PyTorchImporter: Value Head - Weights: %d, Biases: %d"), Weights.Num(), Biases.Num());
			TargetNetwork->SetValueHeadWeights(Weights, Biases);
		}
	}

	// Lade Action Log Std
	const TArray<TSharedPtr<FJsonValue>>* ActionLogStdArray;
	if (RootObject->TryGetArrayField(TEXT("action_log_std"), ActionLogStdArray))
	{
		TArray<float> LogStd;
		for (const TSharedPtr<FJsonValue>& Val : *ActionLogStdArray)
		{
			LogStd.Add(Val->AsNumber());
		}
		TargetNetwork->SetActionLogStd(LogStd);
	}

	UE_LOG(LogTemp, Log, TEXT("PyTorchImporter: Successfully loaded model from %s"), *JSONPath);
	return true;
}

bool UPyTorchImporter::ConvertLayer(const TArray<float>& Weights, const TArray<float>& Biases,
                                     FDenseLayer& OutLayer, int32 InputSize, int32 OutputSize)
{
	// Diese Funktion würde die Gewichte konvertieren, aber da wir keinen direkten
	// Zugriff auf FDenseLayer-Member haben, ist dies schwierig.
	// Wir müssten SimpleNeuralNetwork erweitern.
	
	UE_LOG(LogTemp, Warning, TEXT("PyTorchImporter::ConvertLayer: Not yet implemented - needs SimpleNeuralNetwork extension"));
	return false;
}

