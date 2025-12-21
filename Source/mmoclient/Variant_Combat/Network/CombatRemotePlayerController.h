// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "CombatRemotePlayerController.generated.h"

/**
 * Minimal AI controller for remote player pawns.
 * This controller simply possesses the pawn - no behavior tree or state tree needed
 * since the pawn's state is driven entirely by network updates.
 */
UCLASS()
class ACombatRemotePlayerController : public AAIController
{
	GENERATED_BODY()

public:
	ACombatRemotePlayerController();

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
};
