#include "Components/EngineSynthComponent.h"
#include "Sound/SoundWave.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"

// dr_wav single-header library
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

UEngineSynthComponent::UEngineSynthComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
    , SamplePos(0.0)
    , NextImpulseShapeIndex(0)
{
    PrimaryComponentTick.bCanEverTick = false;
    UE_LOG(LogTemp, Log, TEXT("[EngineSynthComponent] Constructor: default RPM=%.1f"), CurrentRPM);
}

void UEngineSynthComponent::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Log, TEXT("[EngineSynth] BeginPlay gestartet, ImpulseSounds.Num()=%d"), ImpulseSounds.Num());

    // Plugin-Pfad ermitteln
    TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("GameFeature_EngineSimulator"));
    if (!Plugin.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("[EngineSynth] Plugin GameFeature_EngineSimulator nicht gefunden"));
        return;
    }

    const FString PluginContentDir = Plugin->GetContentDir();
    const FString SmoothDir = FPaths::Combine(PluginContentDir, TEXT("Audio"), TEXT("Wav"));

    UE_LOG(LogTemp, Log, TEXT("[EngineSynth] SmoothDir = %s"), *SmoothDir);
    if (!IFileManager::Get().DirectoryExists(*SmoothDir))
    {
        UE_LOG(LogTemp, Error, TEXT("[EngineSynth] Smooth-Verzeichnis existiert nicht: %s"), *SmoothDir);
        return;
    }

    // Bisherige Daten zurücksetzen
    ImpulseSamples.Empty();
    ImpulseLengths.Empty();

    // Alle WAVs über SoftObjectPtr → Dateiname → Datei laden
    for (int32 i = 0; i < ImpulseSounds.Num(); ++i)
    {
        const TSoftObjectPtr<USoundBase>& SoundPtr = ImpulseSounds[i];
        FSoftObjectPath SoftPath = SoundPtr.ToSoftObjectPath();
        const FString AssetName = SoftPath.GetAssetName();  // z.B. "smooth_00"

        if (AssetName.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("[EngineSynth] ImpulseSounds[%d] hat keinen gültigen Asset-Namen"), i);
            ImpulseSamples.AddDefaulted();
            ImpulseLengths.Add(0);
            continue;
        }

        const FString CleanName = AssetName + TEXT(".wav");
        const FString FullPath = FPaths::Combine(SmoothDir, CleanName);

        UE_LOG(LogTemp, Log, TEXT("[EngineSynth] Versuche zu laden: %s"), *FullPath);

        // Datei in Byte-Array einlesen
        TArray<uint8> RawFile;
        if (!FFileHelper::LoadFileToArray(RawFile, *FullPath))
        {
            UE_LOG(LogTemp, Error, TEXT("[EngineSynth] Datei nicht eingelesen: %s"), *FullPath);
            ImpulseSamples.AddDefaulted();
            ImpulseLengths.Add(0);
            continue;
        }

        // WAV mit dr_wav parsen
        drwav wav;
        if (!drwav_init_memory(&wav, RawFile.GetData(), RawFile.Num(), nullptr))
        {
            UE_LOG(LogTemp, Error, TEXT("[EngineSynth] dr_wav konnte %s nicht parsen"), *FullPath);
            ImpulseSamples.AddDefaulted();
            ImpulseLengths.Add(0);
            continue;
        }

        const unsigned int Channels = wav.channels;
        const unsigned int SampleRate = wav.sampleRate;
        const drwav_uint64 TotalFrames = wav.totalPCMFrameCount;

        if (SampleRate != 48000)
        {
            UE_LOG(LogTemp, Warning, TEXT("[EngineSynth] SampleRate %u Hz != 48000 Hz – Wiedergabe dauert ggf. zu lang oder kurz"), SampleRate);
        }

        // PCM als float [-1..1] auslesen
        TArray<float> Samples;
        const int32 NumFrames = static_cast<int32>(TotalFrames);
        Samples.SetNumUninitialized(NumFrames);

        float* pData = static_cast<float*>(FMemory::Malloc(sizeof(float) * (size_t)NumFrames * Channels));
        drwav_read_pcm_frames_f32(&wav, TotalFrames, pData);

        if (Channels == 2)
        {
            // Stereo → Mono
            for (int32 f = 0; f < NumFrames; ++f)
            {
                Samples[f] = 0.5f * (pData[2 * f] + pData[2 * f + 1]);
            }
        }
        else
        {
            // Mono direkt
            for (int32 f = 0; f < NumFrames; ++f)
            {
                Samples[f] = pData[f];
            }
        }

        FMemory::Free(pData);
        drwav_uninit(&wav);

        ImpulseSamples.Add(MoveTemp(Samples));
        ImpulseLengths.Add(NumFrames);

        UE_LOG(LogTemp, Log, TEXT("[EngineSynth] %s geladen: %d Samples @%u Hz, %u Kanal(e)"),
            *CleanName, NumFrames, SampleRate, Channels);
    }

    // Cylinder-Konfiguration initialisieren
    SyncMotorConfig();
}

void UEngineSynthComponent::SyncMotorConfig()
{
    SynthCylinders.Empty();

    if (!EngineSoundSetting)
    {
        UE_LOG(LogTemp, Error, TEXT("[EngineSynthComponent] SyncMotorConfig: EngineSoundSetting NULL"));
        return;
    }

    const auto& Cyls = EngineSoundSetting->EngineConfig.Cylinders;
    UE_LOG(LogTemp, Log, TEXT("[EngineSynthComponent] SyncMotorConfig: found %d cylinders"), Cyls.Num());

    for (int32 i = 0; i < Cyls.Num(); ++i)
    {
        const FCylinderConfig& C = Cyls[i];
        FCylinderSynthState S;
        S.FiringOffset = C.FiringOffset;
        S.LastFirePhase = 0.f;
        SynthCylinders.Add(S);

        UE_LOG(LogTemp, Log, TEXT("  Cylinder[%d] Offset=%.1f"), i, C.FiringOffset);
    }
}

//void UEngineSynthComponent::StartEngine_Implementation()
//{
//    OnStart();
//    Start();
//    UE_LOG(LogTemp, Log, TEXT("[EngineSynthComponent] StartEngine called"));
//}
//
//void UEngineSynthComponent::StopEngine_Implementation()
//{
//    OnStop();
//    Stop();
//    UE_LOG(LogTemp, Log, TEXT("[EngineSynthComponent] StopEngine called"));
//}
//
//void UEngineSynthComponent::SetRPM_Implementation(float Value)
//{
//    CurrentRPM = FMath::Clamp(Value, 300.f, 10000.f);
//    UE_LOG(LogTemp, Log, TEXT("[EngineSynthComponent] SetRPM to %.1f"), CurrentRPM);
//}

void UEngineSynthComponent::OnStart()
{
    SamplePos = 0.0;
    NextImpulseShapeIndex = 0;
    ImpulseQueue.Empty();
    ImpulseShapeQueueIndices.Empty();
    SyncMotorConfig();
    UE_LOG(LogTemp, Log, TEXT("[EngineSynthComponent] OnStart: cleared state"));
}

void UEngineSynthComponent::OnStop()
{
    SynthCylinders.Empty();
    ImpulseQueue.Empty();
    ImpulseShapeQueueIndices.Empty();
    UE_LOG(LogTemp, Log, TEXT("[EngineSynthComponent] OnStop: cleared all"));
}

int32 UEngineSynthComponent::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
    static int BlockID = 0;
    ++BlockID;

    UE_LOG(LogTemp, VeryVerbose, TEXT("[EngineSynth] OnGenerateAudio aufgerufen: Block=%d, NumSamples=%d, SamplePos=%.1f"),
        BlockID, NumSamples, SamplePos);

    // Validation
    if (!EngineSoundSetting || SynthCylinders.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[EngineSynthComponent] Block %d: no config/cylinders"), BlockID);
        FMemory::Memset(OutAudio, 0, NumSamples * sizeof(float));
        return NumSamples;
    }
    if (ImpulseSamples.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[EngineSynthComponent] Block %d: no impulse samples loaded"), BlockID);
        FMemory::Memset(OutAudio, 0, NumSamples * sizeof(float));
        return NumSamples;
    }

    constexpr double SampleRate = 48000.0;
    const double RPM = FMath::Max(300.0, CurrentRPM);
    const double DP = 720.0; // 4-Takt

    for (int32 s = 0; s < NumSamples; ++s)
    {
        double TimeSec = SamplePos / SampleRate;
        double Crank = FMath::Fmod(TimeSec * RPM * DP / 60.0, DP);
        float  OutVal = 0.f;

        // pro Zylinder Zündung prüfen
        for (int32 c = 0; c < SynthCylinders.Num(); ++c)
        {
            auto& Cyl = SynthCylinders[c];
            double Phase = FMath::Fmod(Crank + Cyl.FiringOffset, DP);
            bool   fire = (Cyl.LastFirePhase > Phase);
            if (fire)
            {
                // neuen Impuls einklinken
                FActiveImpulse I;
                I.Position = 0;
                ImpulseQueue.Add(I);
                ImpulseShapeQueueIndices.Add(NextImpulseShapeIndex);

                UE_LOG(LogTemp, Verbose, TEXT("[EngineSynth] Block %d Sample %d: Cylinder %d ignite → shape %d"),
                    BlockID, s, c, NextImpulseShapeIndex);

                // Round-robin Auswahl
                NextImpulseShapeIndex = (NextImpulseShapeIndex + 1) % ImpulseSamples.Num();
            }
            Cyl.LastFirePhase = Phase;
        }

        // alle aktiven Impulse summieren
        for (int32 q = ImpulseQueue.Num() - 1; q >= 0; --q)
        {
            auto& I = ImpulseQueue[q];
            int32 shapeIdx = ImpulseShapeQueueIndices[q];
            int32 len = ImpulseLengths[shapeIdx];
            if (I.Position < len)
            {
                OutVal += ImpulseSamples[shapeIdx][I.Position++];
            }
            else
            {
                // Impuls ausgespielt → aus Queue entfernen
                ImpulseQueue.RemoveAt(q);
                ImpulseShapeQueueIndices.RemoveAt(q);
            }
        }

        // Ausgabe clampen und schreiben
        OutAudio[s] = FMath::Clamp(OutVal * 0.2f, -1.0f, 1.0f);
        SamplePos += 1.0;
    }

    UE_LOG(LogTemp, VeryVerbose, TEXT("[EngineSynth] Block %d: finished, QueueSize=%d"), BlockID, ImpulseQueue.Num());
    return NumSamples;
}
