// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatNetworkSubsystem.h"
#include "CombatRemotePlayer.h"
#include "CombatCharacter.h"
#include "WebSocketsModule.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"

DEFINE_LOG_CATEGORY(LogCombatNetwork);

void UCombatNetworkSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Ensure WebSockets module is loaded
	FModuleManager::Get().LoadModuleChecked(TEXT("WebSockets"));

	UE_LOG(LogCombatNetwork, Log, TEXT("CombatNetworkSubsystem initialized"));
}

void UCombatNetworkSubsystem::Deinitialize()
{
	Disconnect();
	Super::Deinitialize();
}

void UCombatNetworkSubsystem::Connect(const FString& URL)
{
	if (WebSocket.IsValid() && WebSocket->IsConnected())
	{
		UE_LOG(LogCombatNetwork, Warning, TEXT("Already connected to server"));
		return;
	}

	UE_LOG(LogCombatNetwork, Log, TEXT("Connecting to %s"), *URL);

	// Create WebSocket connection
	WebSocket = FWebSocketsModule::Get().CreateWebSocket(URL);

	// Bind callbacks
	WebSocket->OnConnected().AddUObject(this, &UCombatNetworkSubsystem::OnConnected);
	WebSocket->OnConnectionError().AddUObject(this, &UCombatNetworkSubsystem::OnConnectionError);
	WebSocket->OnClosed().AddUObject(this, &UCombatNetworkSubsystem::OnClosed);
	WebSocket->OnMessage().AddUObject(this, &UCombatNetworkSubsystem::OnMessage);

	// Connect
	WebSocket->Connect();
}

void UCombatNetworkSubsystem::Disconnect()
{
	StopNetworkTick();

	if (WebSocket.IsValid())
	{
		if (WebSocket->IsConnected())
		{
			// Send leave message
			TSharedPtr<FJsonObject> EmptyData = MakeShareable(new FJsonObject());
			SendMessage(TEXT("leave"), EmptyData);

			WebSocket->Close();
		}
		WebSocket.Reset();
	}

	// Destroy all remote players
	for (auto& Pair : RemotePlayers)
	{
		if (Pair.Value)
		{
			Pair.Value->Destroy();
		}
	}
	RemotePlayers.Empty();

	bIsConnected = false;
	LocalPlayerId.Empty();
}

bool UCombatNetworkSubsystem::IsConnected() const
{
	return bIsConnected && WebSocket.IsValid() && WebSocket->IsConnected();
}

void UCombatNetworkSubsystem::SendPlayerState(const FCombatNetworkState& State)
{
	if (!IsConnected())
	{
		return;
	}

	TSharedPtr<FJsonObject> Data = MakeShareable(new FJsonObject());

	// Position as array
	TArray<TSharedPtr<FJsonValue>> PosArray;
	PosArray.Add(MakeShareable(new FJsonValueNumber(State.Position.X)));
	PosArray.Add(MakeShareable(new FJsonValueNumber(State.Position.Y)));
	PosArray.Add(MakeShareable(new FJsonValueNumber(State.Position.Z)));
	Data->SetArrayField(TEXT("position"), PosArray);

	// Rotation as array
	TArray<TSharedPtr<FJsonValue>> RotArray;
	RotArray.Add(MakeShareable(new FJsonValueNumber(State.Rotation.Pitch)));
	RotArray.Add(MakeShareable(new FJsonValueNumber(State.Rotation.Yaw)));
	RotArray.Add(MakeShareable(new FJsonValueNumber(State.Rotation.Roll)));
	Data->SetArrayField(TEXT("rotation"), RotArray);

	// Velocity as array
	TArray<TSharedPtr<FJsonValue>> VelArray;
	VelArray.Add(MakeShareable(new FJsonValueNumber(State.Velocity.X)));
	VelArray.Add(MakeShareable(new FJsonValueNumber(State.Velocity.Y)));
	VelArray.Add(MakeShareable(new FJsonValueNumber(State.Velocity.Z)));
	Data->SetArrayField(TEXT("velocity"), VelArray);

	// Other state
	Data->SetNumberField(TEXT("anim_state"), static_cast<int32>(State.AnimState));
	Data->SetNumberField(TEXT("combo_stage"), State.ComboStage);
	Data->SetNumberField(TEXT("charge_progress"), State.ChargeProgress);
	Data->SetNumberField(TEXT("hp"), State.CurrentHP);
	Data->SetNumberField(TEXT("max_hp"), State.MaxHP);

	SendMessage(TEXT("state_update"), Data);
}

void UCombatNetworkSubsystem::SetRemotePlayerClass(TSubclassOf<ACombatRemotePlayer> InClass)
{
	RemotePlayerClass = InClass;
}

void UCombatNetworkSubsystem::SetLocalPlayerCharacter(ACombatCharacter* InCharacter)
{
	LocalPlayerCharacter = InCharacter;
}

void UCombatNetworkSubsystem::StartNetworkTick(float TickRate)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	float Interval = 1.0f / FMath::Max(TickRate, 1.0f);
	World->GetTimerManager().SetTimer(
		NetworkTickTimer,
		this,
		&UCombatNetworkSubsystem::NetworkTick,
		Interval,
		true
	);

	UE_LOG(LogCombatNetwork, Log, TEXT("Started network tick at %.1f Hz"), TickRate);
}

void UCombatNetworkSubsystem::StopNetworkTick()
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(NetworkTickTimer);
	}
}

void UCombatNetworkSubsystem::OnConnected()
{
	UE_LOG(LogCombatNetwork, Log, TEXT("Connected to server"));
	bIsConnected = true;

	// Send join message
	TSharedPtr<FJsonObject> Data = MakeShareable(new FJsonObject());
	Data->SetStringField(TEXT("name"), TEXT("Player"));
	SendMessage(TEXT("join"), Data);

	OnConnectionChanged.Broadcast(true);
}

void UCombatNetworkSubsystem::OnConnectionError(const FString& Error)
{
	UE_LOG(LogCombatNetwork, Error, TEXT("Connection error: %s"), *Error);
	bIsConnected = false;
	OnConnectionChanged.Broadcast(false);
}

void UCombatNetworkSubsystem::OnClosed(int32 StatusCode, const FString& Reason, bool bWasClean)
{
	UE_LOG(LogCombatNetwork, Log, TEXT("Connection closed: %s (code %d)"), *Reason, StatusCode);
	bIsConnected = false;

	// Destroy all remote players
	for (auto& Pair : RemotePlayers)
	{
		if (Pair.Value)
		{
			Pair.Value->Destroy();
		}
	}
	RemotePlayers.Empty();

	OnConnectionChanged.Broadcast(false);
}

void UCombatNetworkSubsystem::OnMessage(const FString& Message)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogCombatNetwork, Warning, TEXT("Failed to parse message: %s"), *Message);
		return;
	}

	FString MessageType;
	if (!JsonObject->TryGetStringField(TEXT("type"), MessageType))
	{
		UE_LOG(LogCombatNetwork, Warning, TEXT("Message missing type field"));
		return;
	}

	// Get data object (may not exist for all message types)
	TSharedPtr<FJsonObject> Data = JsonObject->GetObjectField(TEXT("data"));

	if (MessageType == TEXT("join_response"))
	{
		HandleJoinResponse(Data);
	}
	else if (MessageType == TEXT("player_joined"))
	{
		HandlePlayerJoined(Data);
	}
	else if (MessageType == TEXT("player_state"))
	{
		HandlePlayerState(Data);
	}
	else if (MessageType == TEXT("player_left"))
	{
		HandlePlayerLeft(Data);
	}
	else if (MessageType == TEXT("position_correction"))
	{
		HandlePositionCorrection(Data);
	}
	else
	{
		UE_LOG(LogCombatNetwork, Warning, TEXT("Unknown message type: %s"), *MessageType);
	}
}

void UCombatNetworkSubsystem::HandleJoinResponse(const TSharedPtr<FJsonObject>& Data)
{
	if (!Data.IsValid())
	{
		return;
	}

	LocalPlayerId = Data->GetStringField(TEXT("player_id"));
	UE_LOG(LogCombatNetwork, Log, TEXT("Joined server with ID: %s"), *LocalPlayerId);

	// Server sends X/Y spawn position, we find Z via ground trace
	const TArray<TSharedPtr<FJsonValue>>* SpawnPosArray;
	if (Data->TryGetArrayField(TEXT("spawn_position"), SpawnPosArray) && SpawnPosArray->Num() >= 2)
	{
		float SpawnX = (*SpawnPosArray)[0]->AsNumber();
		float SpawnY = (*SpawnPosArray)[1]->AsNumber();

		if (LocalPlayerCharacter.IsValid())
		{
			UWorld* World = GetWorld();
			if (World)
			{
				// Trace down from high up to find the ground
				FVector TraceStart(SpawnX, SpawnY, 50000.0f);
				FVector TraceEnd(SpawnX, SpawnY, -50000.0f);

				FHitResult HitResult;
				FCollisionQueryParams QueryParams;
				QueryParams.AddIgnoredActor(LocalPlayerCharacter.Get());

				float SpawnZ = 0.0f;
				// Use WorldStatic channel which hits terrain/floors reliably
				if (World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
				{
					// Spawn above ground - use capsule half height to avoid clipping
					float CapsuleHalfHeight = LocalPlayerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
					SpawnZ = HitResult.Location.Z + CapsuleHalfHeight + 10.0f;
					UE_LOG(LogCombatNetwork, Log, TEXT("Ground trace hit at Z=%.1f, spawning at Z=%.1f"), HitResult.Location.Z, SpawnZ);
				}
				else
				{
					// Fallback: keep current Z if no ground found
					SpawnZ = LocalPlayerCharacter->GetActorLocation().Z;
					UE_LOG(LogCombatNetwork, Warning, TEXT("No ground found at spawn X=%.1f Y=%.1f, keeping current Z=%.1f"), SpawnX, SpawnY, SpawnZ);
				}

				FVector SpawnLocation(SpawnX, SpawnY, SpawnZ);
				LocalPlayerCharacter->SetActorLocation(SpawnLocation);
				UE_LOG(LogCombatNetwork, Log, TEXT("Teleported to server spawn position: X=%.1f Y=%.1f Z=%.1f"), SpawnX, SpawnY, SpawnZ);
			}
		}
	}

	// Start network tick now that we're joined
	StartNetworkTick(20.0f);
}

void UCombatNetworkSubsystem::HandlePlayerJoined(const TSharedPtr<FJsonObject>& Data)
{
	if (!Data.IsValid())
	{
		return;
	}

	FString PlayerId = Data->GetStringField(TEXT("player_id"));

	// Don't process ourselves
	if (PlayerId == LocalPlayerId)
	{
		return;
	}

	// Get spawn X/Y from server, find Z via ground trace
	FVector SpawnPosition = FVector::ZeroVector;
	const TArray<TSharedPtr<FJsonValue>>* PosArray;
	if (Data->TryGetArrayField(TEXT("position"), PosArray) && PosArray->Num() >= 2)
	{
		SpawnPosition.X = (*PosArray)[0]->AsNumber();
		SpawnPosition.Y = (*PosArray)[1]->AsNumber();

		// Find ground Z using WorldStatic trace
		UWorld* World = GetWorld();
		if (World)
		{
			FVector TraceStart(SpawnPosition.X, SpawnPosition.Y, 50000.0f);
			FVector TraceEnd(SpawnPosition.X, SpawnPosition.Y, -50000.0f);

			FHitResult HitResult;
			FCollisionQueryParams QueryParams;

			if (World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
			{
				// 100 units above ground for remote players (no capsule ref yet)
				SpawnPosition.Z = HitResult.Location.Z + 100.0f;
			}
		}
	}

	UE_LOG(LogCombatNetwork, Log, TEXT("Player joined: %s at position X=%.1f Y=%.1f Z=%.1f"),
		*PlayerId, SpawnPosition.X, SpawnPosition.Y, SpawnPosition.Z);

	// Spawn the remote player immediately at the calculated position
	SpawnRemotePlayer(PlayerId, SpawnPosition);

	OnRemotePlayerJoined.Broadcast(PlayerId, SpawnPosition);
}

void UCombatNetworkSubsystem::HandlePlayerState(const TSharedPtr<FJsonObject>& Data)
{
	if (!Data.IsValid())
	{
		return;
	}

	FString PlayerId = Data->GetStringField(TEXT("player_id"));

	// Ignore our own state
	if (PlayerId == LocalPlayerId)
	{
		return;
	}

	// Find or spawn the remote player
	ACombatRemotePlayer** FoundPlayer = RemotePlayers.Find(PlayerId);
	ACombatRemotePlayer* RemotePlayer = nullptr;

	if (FoundPlayer && *FoundPlayer)
	{
		RemotePlayer = *FoundPlayer;
	}
	else
	{
		// Player doesn't exist yet, spawn them
		FVector Position = FVector::ZeroVector;
		const TArray<TSharedPtr<FJsonValue>>* PosArray;
		if (Data->TryGetArrayField(TEXT("position"), PosArray) && PosArray->Num() >= 3)
		{
			Position.X = (*PosArray)[0]->AsNumber();
			Position.Y = (*PosArray)[1]->AsNumber();
			Position.Z = (*PosArray)[2]->AsNumber();
		}
		RemotePlayer = SpawnRemotePlayer(PlayerId, Position);
	}

	if (!RemotePlayer)
	{
		return;
	}

	// Build network state from data
	FCombatNetworkState State;

	// Position
	const TArray<TSharedPtr<FJsonValue>>* PosArray;
	if (Data->TryGetArrayField(TEXT("position"), PosArray) && PosArray->Num() >= 3)
	{
		State.Position.X = (*PosArray)[0]->AsNumber();
		State.Position.Y = (*PosArray)[1]->AsNumber();
		State.Position.Z = (*PosArray)[2]->AsNumber();
	}

	// Rotation
	const TArray<TSharedPtr<FJsonValue>>* RotArray;
	if (Data->TryGetArrayField(TEXT("rotation"), RotArray) && RotArray->Num() >= 3)
	{
		State.Rotation.Pitch = (*RotArray)[0]->AsNumber();
		State.Rotation.Yaw = (*RotArray)[1]->AsNumber();
		State.Rotation.Roll = (*RotArray)[2]->AsNumber();
	}

	// Velocity
	const TArray<TSharedPtr<FJsonValue>>* VelArray;
	if (Data->TryGetArrayField(TEXT("velocity"), VelArray) && VelArray->Num() >= 3)
	{
		State.Velocity.X = (*VelArray)[0]->AsNumber();
		State.Velocity.Y = (*VelArray)[1]->AsNumber();
		State.Velocity.Z = (*VelArray)[2]->AsNumber();
	}

	// Animation state
	State.AnimState = static_cast<ECombatAnimationState>(Data->GetIntegerField(TEXT("anim_state")));
	State.ComboStage = Data->GetIntegerField(TEXT("combo_stage"));
	State.ChargeProgress = Data->GetNumberField(TEXT("charge_progress"));
	State.CurrentHP = Data->GetNumberField(TEXT("hp"));
	State.MaxHP = Data->GetNumberField(TEXT("max_hp"));
	State.Timestamp = Data->GetNumberField(TEXT("timestamp"));

	// Apply state to remote player
	RemotePlayer->ApplyNetworkState(State);
}

void UCombatNetworkSubsystem::HandlePlayerLeft(const TSharedPtr<FJsonObject>& Data)
{
	if (!Data.IsValid())
	{
		return;
	}

	FString PlayerId = Data->GetStringField(TEXT("player_id"));
	UE_LOG(LogCombatNetwork, Log, TEXT("Player left: %s"), *PlayerId);

	DestroyRemotePlayer(PlayerId);
	OnRemotePlayerLeft.Broadcast(PlayerId);
}

void UCombatNetworkSubsystem::HandlePositionCorrection(const TSharedPtr<FJsonObject>& Data)
{
	if (!Data.IsValid())
	{
		return;
	}

	// Server is correcting our position (anti-cheat)
	const TArray<TSharedPtr<FJsonValue>>* PosArray;
	if (Data->TryGetArrayField(TEXT("position"), PosArray) && PosArray->Num() >= 3)
	{
		FVector CorrectedPosition;
		CorrectedPosition.X = (*PosArray)[0]->AsNumber();
		CorrectedPosition.Y = (*PosArray)[1]->AsNumber();
		CorrectedPosition.Z = (*PosArray)[2]->AsNumber();

		if (LocalPlayerCharacter.IsValid())
		{
			LocalPlayerCharacter->SetActorLocation(CorrectedPosition);
			UE_LOG(LogCombatNetwork, Warning, TEXT("Position corrected by server to X=%.1f Y=%.1f Z=%.1f"),
				CorrectedPosition.X, CorrectedPosition.Y, CorrectedPosition.Z);
		}
	}
}

ACombatRemotePlayer* UCombatNetworkSubsystem::SpawnRemotePlayer(const FString& PlayerId, const FVector& Position)
{
	// Check if already spawned
	if (RemotePlayers.Contains(PlayerId))
	{
		return RemotePlayers[PlayerId];
	}

	// Need a valid class
	if (!RemotePlayerClass)
	{
		UE_LOG(LogCombatNetwork, Error, TEXT("RemotePlayerClass not set"));
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	UE_LOG(LogCombatNetwork, Log, TEXT("Spawning remote player at X=%.1f Y=%.1f Z=%.1f"), Position.X, Position.Y, Position.Z);

	ACombatRemotePlayer* RemotePlayer = World->SpawnActor<ACombatRemotePlayer>(
		RemotePlayerClass,
		Position,
		FRotator::ZeroRotator,
		SpawnParams
	);

	if (RemotePlayer)
	{
		RemotePlayer->SetPlayerId(PlayerId);
		RemotePlayers.Add(PlayerId, RemotePlayer);
		UE_LOG(LogCombatNetwork, Log, TEXT("Spawned remote player: %s"), *PlayerId);
	}

	return RemotePlayer;
}

void UCombatNetworkSubsystem::DestroyRemotePlayer(const FString& PlayerId)
{
	ACombatRemotePlayer** FoundPlayer = RemotePlayers.Find(PlayerId);
	if (FoundPlayer && *FoundPlayer)
	{
		(*FoundPlayer)->Destroy();
		RemotePlayers.Remove(PlayerId);
	}
}

void UCombatNetworkSubsystem::NetworkTick()
{
	if (!IsConnected() || !LocalPlayerCharacter.IsValid())
	{
		return;
	}

	FCombatNetworkState State = LocalPlayerCharacter->GetNetworkState();
	SendPlayerState(State);
}

void UCombatNetworkSubsystem::SendMessage(const FString& Type, const TSharedPtr<FJsonObject>& Data)
{
	if (!WebSocket.IsValid() || !WebSocket->IsConnected())
	{
		return;
	}

	TSharedPtr<FJsonObject> Message = MakeShareable(new FJsonObject());
	Message->SetStringField(TEXT("type"), Type);
	if (Data.IsValid())
	{
		Message->SetObjectField(TEXT("data"), Data);
	}

	FString MessageString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&MessageString);
	FJsonSerializer::Serialize(Message.ToSharedRef(), Writer);

	WebSocket->Send(MessageString);
}
