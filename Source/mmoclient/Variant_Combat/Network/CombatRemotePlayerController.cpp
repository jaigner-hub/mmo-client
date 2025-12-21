// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatRemotePlayerController.h"
#include "CombatRemotePlayer.h"

ACombatRemotePlayerController::ACombatRemotePlayerController()
{
	bWantsPlayerState = false;
}

void ACombatRemotePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
}

void ACombatRemotePlayerController::OnUnPossess()
{
	Super::OnUnPossess();
}
