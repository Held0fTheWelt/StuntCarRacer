#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
//#include "Interfaces/EngineSimulatorInterface.h"
#include "DataAssets/EngineSimulatorSetupDataAsset.h"
#include "EngineSynthComponent.generated.h"

USTRUCT()
struct FCylinderSynthState
{
    GENERATED_BODY()
    float FiringOffset = 0.f;
    float LastFirePhase = 0.f;
};

USTRUCT()
struct FActiveImpulse
{
    GENERATED_BODY()
    int32 Position = 0;
};

UCLASS(ClassGroup = Synth, meta = (BlueprintSpawnableComponent))
class GAMEFEATURE_ENGINESIMULATORRUNTIME_API UEngineSynthComponent
    : public USynthComponent
    //, public IEngineSimulatorInterface
{
    GENERATED_BODY()

public:
    UEngineSynthComponent(const FObjectInitializer& ObjectInitializer);

    /** Dein EngineConfig-Asset */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Engine Sound")
    UEngineSimulatorSetupDataAsset* EngineSoundSetting;

    /** Liste der Impuls-Assets (smooth_00..smooth_NN) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Engine Sound")
    TArray<TSoftObjectPtr<USoundBase>> ImpulseSounds;

    /** Aktuelle Drehzahl */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Engine Sound")
    float CurrentRPM = 300.0f;

protected:
    virtual void BeginPlay() override;
    virtual void OnStart() override;
    virtual void OnStop() override;
    virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

    /** Lädt Cylinder-Konfiguration aus deinem DataAsset */
    void SyncMotorConfig();

private:
    // IEngineSimulatorInterface
    //virtual void StartEngine_Implementation() override;
    //virtual void StopEngine_Implementation() override;
    //virtual void SetRPM_Implementation(float Value) override;

    // interne Synth-Daten
    double SamplePos;
    TArray<FCylinderSynthState> SynthCylinders;

    // Impuls-Queue
    TArray<FActiveImpulse> ImpulseQueue;
    TArray<int32>        ImpulseShapeQueueIndices;

    // geladene Pulse
    TArray<TArray<float>> ImpulseSamples;
    TArray<int32>         ImpulseLengths;
    int32                 NextImpulseShapeIndex;
};
