// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatRemotePlayer.h"
#include "CombatRemotePlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "CombatLifeBar.h"

ACombatRemotePlayer::ACombatRemotePlayer()
{
	// Enable ticking for interpolation
	PrimaryActorTick.bCanEverTick = true;

	// Set the AI controller class
	AIControllerClass = ACombatRemotePlayerController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Don't use controller rotation - let movement component handle it
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	// Disable camera components for remote players
	if (GetCameraBoom())
	{
		GetCameraBoom()->SetActive(false);
	}
	if (GetFollowCamera())
	{
		GetFollowCamera()->SetActive(false);
	}

	// Tag as remote player
	Tags.Remove(FName("Player"));
	Tags.Add(FName("RemotePlayer"));
}

void ACombatRemotePlayer::BeginPlay()
{
	Super::BeginPlay();

	// Deactivate camera components
	if (GetCameraBoom())
	{
		GetCameraBoom()->Deactivate();
	}
	if (GetFollowCamera())
	{
		GetFollowCamera()->Deactivate();
	}

	// Configure movement component
	if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
	{
		MovementComp->GravityScale = 1.0f;
		MovementComp->bEnablePhysicsInteraction = false;
		// Face direction of movement
		MovementComp->bOrientRotationToMovement = true;
		MovementComp->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	}

	// Keep default pawn collision so attacks can hit remote players
	// bEnablePhysicsInteraction = false above prevents pushing

	// Initialize states
	CurrentState.Position = GetActorLocation();
	CurrentState.Rotation = GetActorRotation();
	PreviousState = CurrentState;
}

void ACombatRemotePlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Don't bind any input for remote players - state comes from network
}

float ACombatRemotePlayer::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	// Remote players don't take local damage - their HP is controlled by network state
	return 0.0f;
}

void ACombatRemotePlayer::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
{
	// Apply visual effects without modifying HP (HP comes from network)

	// Pause network position updates during hit reaction
	HitReactionTimer = 0.5f;

	// Apply small knockback impulse to movement component (reduced for remote players)
	GetCharacterMovement()->AddImpulse(DamageImpulse * 0.1f, true);

	// Set physics blend weight for partial ragdoll visual (same as local player's TakeDamage)
	GetMesh()->SetPhysicsBlendWeight(0.5f);
	GetMesh()->SetBodySimulatePhysics(PelvisBoneName, false);

	// Only add mesh impulse if already simulating physics (like local player does)
	if (GetMesh()->IsSimulatingPhysics())
	{
		GetMesh()->AddImpulseAtLocation(DamageImpulse * GetMesh()->GetMass() * 0.3f, DamageLocation);
	}

	// Call BP handler to play effects
	ReceivedDamage(Damage, DamageLocation, DamageImpulse.GetSafeNormal());
}

void ACombatRemotePlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Handle hit reaction timer
	if (HitReactionTimer > 0.0f)
	{
		HitReactionTimer -= DeltaTime;

		// When timer expires, reset physics blend
		if (HitReactionTimer <= 0.0f)
		{
			GetMesh()->SetPhysicsBlendWeight(0.0f);
		}

		// Don't chase network position during hit reaction
		return;
	}

	FVector CurrentLocation = GetActorLocation();
	FVector TargetLocation = CurrentState.Position;

	// Horizontal movement with AddMovementInput (drives animation)
	FVector ToTarget = TargetLocation - CurrentLocation;
	ToTarget.Z = 0;
	float Distance = ToTarget.Size();

	if (Distance > 5.0f)
	{
		FVector Direction = ToTarget.GetSafeNormal();
		float InputScale = FMath::Clamp(Distance / 100.0f, 0.5f, 1.0f);
		AddMovementInput(Direction, InputScale);
	}
}

void ACombatRemotePlayer::ApplyNetworkState(const FCombatNetworkState& NewState)
{
	// Store previous state for interpolation reference
	PreviousState = CurrentState;
	CurrentState = NewState;
	StateReceiveTime = GetWorld()->GetTimeSeconds();

	// If this is a big horizontal position change (like first update or teleport), teleport there
	FVector CurrentLoc = GetActorLocation();
	float HorizontalDistSq = FMath::Square(CurrentLoc.X - NewState.Position.X) + FMath::Square(CurrentLoc.Y - NewState.Position.Y);
	if (HorizontalDistSq > 40000.0f) // More than 200 units away horizontally
	{
		// Teleport X/Y only, keep current Z so we fall to ground
		SetActorLocation(FVector(NewState.Position.X, NewState.Position.Y, CurrentLoc.Z));
	}

	// Check for animation state changes
	if (NewState.AnimState != LastAnimState || NewState.ComboStage != LastComboStage)
	{
		OnAnimationStateChanged(NewState.AnimState, NewState.ComboStage);
		LastAnimState = NewState.AnimState;
		LastComboStage = NewState.ComboStage;
	}

	// Update life bar if HP changed
	if (NewState.CurrentHP != PreviousState.CurrentHP || NewState.MaxHP != PreviousState.MaxHP)
	{
		UpdateLifeBarFromNetwork(NewState.CurrentHP, NewState.MaxHP);
	}
}

void ACombatRemotePlayer::OnAnimationStateChanged(ECombatAnimationState NewState, int32 NewComboStage)
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (!AnimInstance)
	{
		return;
	}

	switch (NewState)
	{
		case ECombatAnimationState::Idle:
		case ECombatAnimationState::Moving:
			// Stop any playing montages for idle/moving states
			AnimInstance->Montage_Stop(0.2f);
			break;

		case ECombatAnimationState::ComboAttack:
			// Play combo attack montage
			if (ComboAttackMontage)
			{
				// If already playing, jump to the correct section
				if (AnimInstance->Montage_IsPlaying(ComboAttackMontage))
				{
					if (NewComboStage > 0 && NewComboStage < ComboSectionNames.Num())
					{
						AnimInstance->Montage_JumpToSection(ComboSectionNames[NewComboStage], ComboAttackMontage);
					}
				}
				else
				{
					// Start playing the montage
					AnimInstance->Montage_Play(ComboAttackMontage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f, true);

					// Jump to correct section if not first attack
					if (NewComboStage > 0 && NewComboStage < ComboSectionNames.Num())
					{
						AnimInstance->Montage_JumpToSection(ComboSectionNames[NewComboStage], ComboAttackMontage);
					}
				}
			}
			break;

		case ECombatAnimationState::ChargedAttackCharging:
			// Play charged attack montage (charging phase)
			if (ChargedAttackMontage)
			{
				if (!AnimInstance->Montage_IsPlaying(ChargedAttackMontage))
				{
					AnimInstance->Montage_Play(ChargedAttackMontage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f, true);
				}
				// Stay in charge loop section
				AnimInstance->Montage_JumpToSection(ChargeLoopSection, ChargedAttackMontage);
			}
			break;

		case ECombatAnimationState::ChargedAttackRelease:
			// Play charged attack release
			if (ChargedAttackMontage)
			{
				if (!AnimInstance->Montage_IsPlaying(ChargedAttackMontage))
				{
					AnimInstance->Montage_Play(ChargedAttackMontage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f, true);
				}
				// Jump to attack section
				AnimInstance->Montage_JumpToSection(ChargeAttackSection, ChargedAttackMontage);
			}
			break;

		case ECombatAnimationState::TakingDamage:
			// Physics blend causes floor clipping - skip it for remote players
			break;

		case ECombatAnimationState::Dead:
			// Don't call HandleDeath() - it enables ragdoll which falls through floor
			// Just stop movement and let them stay in place
			if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
			{
				MovementComp->DisableMovement();
			}
			break;
	}
}

void ACombatRemotePlayer::UpdateLifeBarFromNetwork(float HP, float MaxHPValue)
{
	// Check if HP decreased (took damage)
	bool bTookDamage = HP < CurrentHP && CurrentHP > 0.0f;

	// Update internal HP values
	CurrentHP = HP;
	MaxHP = MaxHPValue;

	// Update life bar widget if available
	if (LifeBarWidget)
	{
		float Percentage = MaxHPValue > 0.0f ? HP / MaxHPValue : 0.0f;
		LifeBarWidget->SetLifePercentage(Percentage);
	}

	// Play hit reaction if took damage
	if (bTookDamage && HP > 0.0f)
	{
		// Apply a knockback impulse (no physics blend - it causes floor clipping)
		if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
		{
			FVector KnockbackDir = -GetActorForwardVector();
			MovementComp->AddImpulse(KnockbackDir * 500.0f, true);
		}
	}
}
