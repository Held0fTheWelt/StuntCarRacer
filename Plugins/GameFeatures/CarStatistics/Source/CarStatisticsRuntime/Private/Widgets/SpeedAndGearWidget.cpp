// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/SpeedAndGearWidget.h"

#include "Components/TextBlock.h"

#include "Kismet/KismetTextLibrary.h"

void USpeedAndGearWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (Label_Unit)
	{
		Label_Unit->SetText(bIsMPH ? FText::FromString(TEXT("MPH")) : FText::FromString(TEXT("KPH")));
	}

	OnSpeedUpdated.AddDynamic(this, &USpeedAndGearWidget::UpdateSpeed);
	OnGearUpdated.AddDynamic(this, &USpeedAndGearWidget::UpdateGear);
}

void USpeedAndGearWidget::UpdateSpeed(float NewSpeed)
{
	// format the speed to KPH or MPH
	float FormattedSpeed = FMath::Abs(NewSpeed) * (bIsMPH ? 0.022f : 0.036f);

	Label_Speed->SetText(UKismetTextLibrary::Conv_DoubleToText(FormattedSpeed, ERoundingMode::HalfToEven, false, true, 3, 3, 0, 0));

	OnSpeedUpdate(FormattedSpeed);
}

void USpeedAndGearWidget::UpdateGear(int32 NewGear)
{
	switch (NewGear)
	{
		case -1:
		Label_Gear->SetText(FText::FromString(TEXT("R")));
		break;
		case 0:
		Label_Gear->SetText(FText::FromString(TEXT("N")));
		break;
		default:
		Label_Gear->SetText(UKismetTextLibrary::Conv_IntToText(NewGear, false, true, 1, 1));
		break;
	}

	OnGearUpdate(NewGear);
}