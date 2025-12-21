// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CombatGameMode.generated.h"

class ACombatRemotePlayer;

/**
 *  Simple GameMode for a third person combat game with network support
 */
UCLASS(abstract)
class ACombatGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	ACombatGameMode();

protected:

	/** Called when the game starts */
	virtual void BeginPlay() override;

	/** WebSocket URL to connect to */
	UPROPERTY(EditDefaultsOnly, Category="Network")
	FString WebSocketURL = TEXT("ws://localhost:8080/ws");

	/** Class to spawn for remote players */
	UPROPERTY(EditDefaultsOnly, Category="Network")
	TSubclassOf<ACombatRemotePlayer> RemotePlayerClass;

	/** Whether to automatically connect on BeginPlay */
	UPROPERTY(EditDefaultsOnly, Category="Network")
	bool bAutoConnect = true;

public:

	/**
	 * Manually connect to the network server
	 */
	UFUNCTION(BlueprintCallable, Category="Network")
	void ConnectToServer();

	/**
	 * Disconnect from the network server
	 */
	UFUNCTION(BlueprintCallable, Category="Network")
	void DisconnectFromServer();
};
