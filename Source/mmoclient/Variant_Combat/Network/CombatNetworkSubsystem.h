// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CombatNetworkTypes.h"
#include "IWebSocket.h"
#include "CombatNetworkSubsystem.generated.h"

class ACombatRemotePlayer;
class ACombatCharacter;

DECLARE_LOG_CATEGORY_EXTERN(LogCombatNetwork, Log, All);

/**
 * Delegate for when connection state changes
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNetworkConnectionChanged, bool, bConnected);

/**
 * Delegate for when a remote player joins
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRemotePlayerJoined, const FString&, PlayerId, const FVector&, Position);

/**
 * Delegate for when a remote player leaves
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRemotePlayerLeft, const FString&, PlayerId);

/**
 * Game instance subsystem that manages WebSocket connection to the game server
 * and handles multiplayer state synchronization.
 */
UCLASS()
class UCombatNetworkSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	/** Initialize the subsystem */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Cleanup the subsystem */
	virtual void Deinitialize() override;

	/**
	 * Connect to the game server
	 * @param URL WebSocket URL to connect to (e.g., ws://localhost:8080/ws)
	 */
	UFUNCTION(BlueprintCallable, Category="Network")
	void Connect(const FString& URL);

	/**
	 * Disconnect from the game server
	 */
	UFUNCTION(BlueprintCallable, Category="Network")
	void Disconnect();

	/**
	 * Check if connected to the server
	 */
	UFUNCTION(BlueprintPure, Category="Network")
	bool IsConnected() const;

	/**
	 * Get the local player's assigned ID
	 */
	UFUNCTION(BlueprintPure, Category="Network")
	FString GetLocalPlayerId() const { return LocalPlayerId; }

	/**
	 * Send the local player's state to the server
	 */
	UFUNCTION(BlueprintCallable, Category="Network")
	void SendPlayerState(const FCombatNetworkState& State);

	/**
	 * Set the class to spawn for remote players
	 */
	UFUNCTION(BlueprintCallable, Category="Network")
	void SetRemotePlayerClass(TSubclassOf<ACombatRemotePlayer> InClass);

	/**
	 * Set the local player character reference
	 */
	UFUNCTION(BlueprintCallable, Category="Network")
	void SetLocalPlayerCharacter(ACombatCharacter* InCharacter);

	/**
	 * Start sending network updates at the specified rate
	 * @param TickRate Updates per second (default 20Hz)
	 */
	UFUNCTION(BlueprintCallable, Category="Network")
	void StartNetworkTick(float TickRate = 20.0f);

	/**
	 * Stop sending network updates
	 */
	UFUNCTION(BlueprintCallable, Category="Network")
	void StopNetworkTick();

public:

	/** Called when connection state changes */
	UPROPERTY(BlueprintAssignable, Category="Network")
	FOnNetworkConnectionChanged OnConnectionChanged;

	/** Called when a remote player joins */
	UPROPERTY(BlueprintAssignable, Category="Network")
	FOnRemotePlayerJoined OnRemotePlayerJoined;

	/** Called when a remote player leaves */
	UPROPERTY(BlueprintAssignable, Category="Network")
	FOnRemotePlayerLeft OnRemotePlayerLeft;

protected:

	/** WebSocket connection callbacks */
	void OnConnected();
	void OnConnectionError(const FString& Error);
	void OnClosed(int32 StatusCode, const FString& Reason, bool bWasClean);
	void OnMessage(const FString& Message);

	/** Message handlers */
	void HandleJoinResponse(const TSharedPtr<FJsonObject>& Data);
	void HandlePlayerJoined(const TSharedPtr<FJsonObject>& Data);
	void HandlePlayerState(const TSharedPtr<FJsonObject>& Data);
	void HandlePlayerLeft(const TSharedPtr<FJsonObject>& Data);

	/** Spawn a remote player pawn */
	ACombatRemotePlayer* SpawnRemotePlayer(const FString& PlayerId, const FVector& Position);

	/** Destroy a remote player pawn */
	void DestroyRemotePlayer(const FString& PlayerId);

	/** Network tick callback */
	void NetworkTick();

	/** Send a JSON message to the server */
	void SendMessage(const FString& Type, const TSharedPtr<FJsonObject>& Data);

private:

	/** WebSocket connection */
	TSharedPtr<IWebSocket> WebSocket;

	/** Local player's ID assigned by the server */
	FString LocalPlayerId;

	/** Map of remote player IDs to their pawn actors */
	UPROPERTY()
	TMap<FString, ACombatRemotePlayer*> RemotePlayers;

	/** Class to spawn for remote players */
	UPROPERTY()
	TSubclassOf<ACombatRemotePlayer> RemotePlayerClass;

	/** Reference to the local player's character */
	UPROPERTY()
	TWeakObjectPtr<ACombatCharacter> LocalPlayerCharacter;

	/** Timer handle for network tick */
	FTimerHandle NetworkTickTimer;

	/** Whether we're currently connected */
	bool bIsConnected = false;
};
