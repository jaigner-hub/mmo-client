// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "mmoclientGameMode.generated.h"

/**
 *  Simple GameMode for a third person game
 */
UCLASS(abstract)
class AmmoclientGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	
	/** Constructor */
	AmmoclientGameMode();
};



