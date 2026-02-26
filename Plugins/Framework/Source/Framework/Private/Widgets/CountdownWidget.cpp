// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/CountdownWidget.h"


void UCountdownWidget::StartCountdown()
{
	// pass control to BP
	BP_StartCountdown();
}

void UCountdownWidget::FinishCountdown()
{
	// broadcast the delegate
	OnCountdownFinished.Broadcast();
}