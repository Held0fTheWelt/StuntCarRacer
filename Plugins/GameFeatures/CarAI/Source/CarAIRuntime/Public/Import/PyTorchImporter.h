#pragma once

#include "CoreMinimal.h"
#include "NN/SimpleNeuralNetwork.h"
#include "PyTorchImporter.generated.h"

/**
 * Importiert PyTorch-Modelle nach Unreal
 * Konvertiert PyTorch .pt Dateien in das Unreal Neural Network Format
 */
UCLASS(BlueprintType)
class CARAIRUNTIME_API UPyTorchImporter : public UObject
{
	GENERATED_BODY()

public:
	/** Lädt ein PyTorch-Modell und konvertiert es zu Unreal Neural Network */
	UFUNCTION(BlueprintCallable, Category = "PyTorch Import")
	bool ImportPyTorchModel(const FString& PyTorchModelPath, USimpleNeuralNetwork* TargetNetwork);

	/** Prüft ob eine PyTorch-Modell-Datei existiert */
	UFUNCTION(BlueprintCallable, Category = "PyTorch Import")
	bool DoesModelExist(const FString& ModelPath) const;

private:
	/** Lädt Gewichte aus JSON-Export (PyTorch -> JSON via Python Script) */
	bool LoadWeightsFromJSON(const FString& JSONPath, USimpleNeuralNetwork* TargetNetwork);

	/** Konvertiert PyTorch Layer zu Unreal Layer */
	bool ConvertLayer(const TArray<float>& Weights, const TArray<float>& Biases, 
	                  FDenseLayer& OutLayer, int32 InputSize, int32 OutputSize);
};

