// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CombatCharacter.h"
#include "CombatNetworkTypes.h"
#include "CombatRemotePlayer.generated.h"

/**
 * Remote player pawn that displays another player's state received over the network.
 * Inherits from CombatCharacter to reuse visuals, animations, and life bar,
 * but disables input handling since state is driven by network updates.
 */
UCLASS(Blueprintable)
class ACombatRemotePlayer : public ACombatCharacter
{
	GENERATED_BODY()

public:
	ACombatRemotePlayer();

	/**
	 * Apply a network state update to this remote player
	 * Handles interpolation and animation state changes
	 */
	UFUNCTION(BlueprintCallable, Category="Network")
	void ApplyNetworkState(const FCombatNetworkState& NewState);

	/**
	 * Set the player ID for this remote player
	 */
	UFUNCTION(BlueprintCallable, Category="Network")
	void SetPlayerId(const FString& InPlayerId) { PlayerId = InPlayerId; }

	/**
	 * Get the player ID for this remote player
	 */
	UFUNCTION(BlueprintPure, Category="Network")
	FString GetPlayerId() const { return PlayerId; }

protected:

	/** Called every frame */
	virtual void Tick(float DeltaTime) override;

	/** Called when play begins */
	virtual void BeginPlay() override;

	/** Override to prevent input binding */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Override to ignore local damage - HP comes from network */
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator, AActor* DamageCauser) override;

	/** Override to apply visual effects without modifying HP */
	virtual void ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;

	/** Handle animation state changes from network */
	void OnAnimationStateChanged(ECombatAnimationState NewState, int32 NewComboStage);

	/** Update life bar from network state */
	void UpdateLifeBarFromNetwork(float HP, float MaxHPValue);

private:

	/** This remote player's network ID */
	UPROPERTY()
	FString PlayerId;

	/** Previous network state for interpolation */
	FCombatNetworkState PreviousState;

	/** Current target network state */
	FCombatNetworkState CurrentState;

	/** Time when we received the current state */
	float StateReceiveTime = 0.0f;

	/** Last animation state we processed */
	ECombatAnimationState LastAnimState = ECombatAnimationState::Idle;

	/** Last combo stage we processed */
	int32 LastComboStage = 0;

	/** Timer for hit reaction - pause network position updates during hit */
	float HitReactionTimer = 0.0f;

	/** Position interpolation speed */
	UPROPERTY(EditDefaultsOnly, Category="Network")
	float PositionInterpSpeed = 10.0f;

	/** Rotation interpolation speed */
	UPROPERTY(EditDefaultsOnly, Category="Network")
	float RotationInterpSpeed = 10.0f;
};
