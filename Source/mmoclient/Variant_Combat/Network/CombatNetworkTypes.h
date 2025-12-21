// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CombatNetworkTypes.generated.h"

/**
 * Animation states that can be synchronized over the network
 */
UENUM(BlueprintType)
enum class ECombatAnimationState : uint8
{
	Idle,
	Moving,
	ComboAttack,
	ChargedAttackCharging,
	ChargedAttackRelease,
	TakingDamage,
	Dead
};

/**
 * Network-syncable state for combat characters
 */
USTRUCT(BlueprintType)
struct FCombatNetworkState
{
	GENERATED_BODY()

	/** World position */
	UPROPERTY(BlueprintReadWrite, Category="Network")
	FVector Position = FVector::ZeroVector;

	/** Actor rotation */
	UPROPERTY(BlueprintReadWrite, Category="Network")
	FRotator Rotation = FRotator::ZeroRotator;

	/** Movement velocity for blend space interpolation */
	UPROPERTY(BlueprintReadWrite, Category="Network")
	FVector Velocity = FVector::ZeroVector;

	/** Current animation state */
	UPROPERTY(BlueprintReadWrite, Category="Network")
	ECombatAnimationState AnimState = ECombatAnimationState::Idle;

	/** Current combo attack stage (0-N) */
	UPROPERTY(BlueprintReadWrite, Category="Network")
	int32 ComboStage = 0;

	/** Charged attack progress (0-1) */
	UPROPERTY(BlueprintReadWrite, Category="Network")
	float ChargeProgress = 0.0f;

	/** Current HP */
	UPROPERTY(BlueprintReadWrite, Category="Network")
	float CurrentHP = 0.0f;

	/** Maximum HP */
	UPROPERTY(BlueprintReadWrite, Category="Network")
	float MaxHP = 0.0f;

	/** Server timestamp for interpolation */
	UPROPERTY(BlueprintReadWrite, Category="Network")
	double Timestamp = 0.0;
};
