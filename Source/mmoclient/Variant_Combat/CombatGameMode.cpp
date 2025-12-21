// Copyright Epic Games, Inc. All Rights Reserved.


#include "Variant_Combat/CombatGameMode.h"
#include "CombatNetworkSubsystem.h"
#include "CombatRemotePlayer.h"
#include "CombatCharacter.h"
#include "Kismet/GameplayStatics.h"

ACombatGameMode::ACombatGameMode()
{
}

void ACombatGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoConnect)
	{
		// Delay connection slightly to ensure player is spawned
		FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &ACombatGameMode::ConnectToServer, 0.5f, false);
	}
}

void ACombatGameMode::ConnectToServer()
{
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	UCombatNetworkSubsystem* NetworkSubsystem = GameInstance->GetSubsystem<UCombatNetworkSubsystem>();
	if (!NetworkSubsystem)
	{
		return;
	}

	// Set the remote player class
	if (RemotePlayerClass)
	{
		NetworkSubsystem->SetRemotePlayerClass(RemotePlayerClass);
	}

	// Find and set the local player character
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (ACombatCharacter* CombatChar = Cast<ACombatCharacter>(PlayerPawn))
	{
		NetworkSubsystem->SetLocalPlayerCharacter(CombatChar);
	}

	// Connect to the server
	NetworkSubsystem->Connect(WebSocketURL);
}

void ACombatGameMode::DisconnectFromServer()
{
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	UCombatNetworkSubsystem* NetworkSubsystem = GameInstance->GetSubsystem<UCombatNetworkSubsystem>();
	if (NetworkSubsystem)
	{
		NetworkSubsystem->Disconnect();
	}
}