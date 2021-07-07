// Project:         Advanced Locomotion System V4 on C++
// Copyright:       Copyright (C) 2020 Doğa Can Yanıkoğlu
// License:         MIT License (http://www.opensource.org/licenses/mit-license.php)
// Source Code:     https://github.com/dyanikoglu/ALSV4_CPP
// Original Author: Doğa Can Yanıkoğlu
// Contributors:    Haziq Fadhil


#pragma once

#include "CoreMinimal.h"
#include "Components/TimelineComponent.h"
#include "Library/ALSCharacterEnumLibrary.h"
#include "Library/ALSCharacterStructLibrary.h"
#include "Engine/DataTable.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetSystemLibrary.h"
#include "DependencyFix/Public/Library/AAADStructLibrary.h"
#include "DependencyFix/Public/PhysicsObject.h"
#include "Character/AAADTypes.h"
#include "DependencyFix/Public/StationControl.h"
#include "ALSBaseCharacter.generated.h"


DECLARE_MULTICAST_DELEGATE_TwoParams(FOnEquipWeapon, AALSBaseCharacter*, AWeapon* /* new */);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnUnEquipWeapon, AALSBaseCharacter*, AWeapon* /* old */);

class AWeapon;
class APhysicsItem;
class ASingleShotTestGun;
class UTimelineComponent;
class UAnimInstance;
class UAnimMontage;
class UALSCharacterAnimInstance;
class ARoomDataHelper;

enum class EVisibilityBasedAnimTickOption : uint8;
enum class EWeaponType : uint8;

/*
 * Base character class
 */
UCLASS(BlueprintType)
class ALSV4_CPP_API AALSBaseCharacter : public ACharacter, public IPhysicsInterface
{
	GENERATED_BODY()

		UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
		class UCameraComponent* FirstPersonCameraComponent;

	UPROPERTY(VisibleAnywhere)
		class UStaticMeshComponent* CameraPoll;
	
		UCapsuleComponent* CapsuleComponent;
	/** get max health */
	int32 GetMaxHealth() const;


	/** Global notification when a character equips a weapon. Needed for replication graph. */
	static FOnEquipWeapon NotifyEquipWeapon;

	/** Global notification when a character un-equips a weapon. Needed for replication graph. */
	static FOnUnEquipWeapon NotifyUnEquipWeapon;

	void EquipWeapon(class AWeapon* Weapon);

	float Blur = 0.f;
	bool BlurBool = false;

public:
	AALSBaseCharacter(const FObjectInitializer& ObjectInitializer);

	void UpdateCameraRotation(FRotator &RotationOffset, FRotator& OldOffsetAndOutControlRot, float DeltaTime);
	
	

	void ZeroGravTest();
	void ReturnToGravity();

	virtual void Gravitate(FVector SourceLocation, FVector HitLocation, float Direction, float Strength) override;

	//Debug: 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Debug)
		bool DrawDebugStuff = false;

	UPROPERTY(BlueprintReadOnly, Category = "ID", Replicated)
		uint8 PlayerID;
		
	UFUNCTION(Reliable, Client)
	void ClientCalculateFlow(ARoomDataHelper* DataHelper, const TArray<FFloatBool>& AirCurrentData) const;

	
	UFUNCTION(reliable, server, WithValidation)
		void ServerUpdateFlow(const FCompressedForceArray& OutCompressedForceArray, ARoomDataHelper* BroadcastingDataHelper);
		
	//GasSystem: 
	//UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_LocalForces, Category = "ALS|Essential Information")
	TArray<FVector> LocalForces; 
		
	
	

	float BrakingDecelerationFlying;
	int32 GetCurrentRoomID();
	int32 GetPredictedNextRoomID();
	void SetPredictedRoomID(int32 NewRoomID);
	void SetCurrentRoomID(int32 NewRoomID);
	void SetCurrentRoomIDToPredicted();
	FGas GasSamplePlaceholder = FGas::DefaultAtmosphereComposition;
	FGas* GasSample = &GasSamplePlaceholder;
	class AAirCurrent* OverlappingAirCurrent;
	float PressurePlaceholder = 1.f;
	float* ClientCurrentRoomPressurePointer = &PressurePlaceholder;
	// laugh out loud oh my god
	void PassGasToHud();
	FVecGrav VecGravSample;
	FGridSample GridSample;
	/** Called on the actor right before replication occurs */
	virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;

	/** Returns True if the pawn can die in the current state */
	virtual bool CanDie(float KillingDamage, FDamageEvent const& DamageEvent, AController* Killer, AActor* DamageCauser) const;

	FVector NewWindForce = FVector::ZeroVector;
	FVector WindForce = FVector::ZeroVector;
	FVector OldWindForce = FVector::ZeroVector;
	/**
	* Kills pawn.  Server/authority only.
	* @param KillingDamage - Damage amount of the killing blow
	* @param DamageEvent - Damage event of the killing blow
	* @param Killer - Who killed this pawn
	* @param DamageCauser - the Actor that directly caused the damage (i.e. the Projectile that exploded, the Weapon that fired, etc)
	* @returns true if allowed
	*/
	virtual bool Die(float KillingDamage, struct FDamageEvent const& DamageEvent, class AController* Killer, class AActor* DamageCauser);


	/** Take damage, handle death */
	virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, class AActor* DamageCauser) override;

	/** Identifies if pawn is in its dying state */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Health)
		uint32 bIsDying : 1;

	// Current health of the Pawn
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = Health)
		float Health = 100.f;

	/** [server + local] change targeting state */
	void SetTargeting(bool bNewTargeting);

	UFUNCTION(BlueprintCallable, Category = "Game|Weapon")
		bool IsTargeting() const;
	

	/** current firing state */
	uint8 bWantsToFire : 1;

	/** [local] starts weapon fire */
	void StartWeaponFire();

	

	/** [local] stops weapon fire */
	void StopWeaponFire();

	/** player released start fire action */
	void OnStopFire();

	APhysicsObject* PhysicsObjectTest;
	AWeapon* GetWeapon();

	UPROPERTY(Transient, ReplicatedUsing = OnRep_CurrentWeapon)
	AWeapon* CurrentWeapon;

	UFUNCTION()
		void OnRep_CurrentWeapon(AWeapon* LastWeapon);

	void SetCurrentWeapon(class AWeapon* NewWeapon, class AWeapon* LastWeapon = NULL);
	/** equip weapon */
	UFUNCTION(reliable, server, WithValidation)
		void ServerEquipWeapon(class AWeapon* NewWeapon);
	
	void SpawnWeaponFromPickup(APhysicsItem* PickedUpItem);
	
	UFUNCTION(reliable, server, WithValidation)
		void ServerSpawnWeaponFromPickup(APhysicsItem* PickedUpItem);


	void SpawnWeapon(EWeaponType WeaponType);

	UFUNCTION(reliable, server, WithValidation)
		void ServerSpawnWeapon(EWeaponType WeaponType);

		bool CanReload();


	UPROPERTY(EditAnywhere)
		TSubclassOf<class AWeapon> WeaponToSpawn;

	UPROPERTY(EditAnywhere)
		TSubclassOf<class AWeapon> TestWeaponToSpawn;

	UPROPERTY(EditAnywhere)
		TSubclassOf<class AWeapon> VoodooGunToSpawn;

	UPROPERTY(Transient, Replicated)
	TArray<class AWeapon*>Arsenal;

	

	bool CanFire();
	
	

	//!!!!!!!!!!!!!!!!!111
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CameraSystem")
		FRotator CameraRotation;
	//!!!!!!!!!!!!!!!!!111


	UFUNCTION(BlueprintCallable, Category= "Movement")
		FORCEINLINE class UALSCharacterMovementComponent* GetMyMovementComponent() const
	{
		return MyCharacterMovementComponent;
	}

	
	void SetGravityDirection(FVector Direction);

	FVector GravityDirection;

	virtual void Tick(float DeltaTime) override;

	virtual void BeginPlay() override;

	virtual void PreInitializeComponents() override;

	virtual void Restart() override;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	virtual void PostInitializeComponents() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Ragdoll System */

	/** Implement on BP to get required get up animation according to character's state */
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "ALS|Ragdoll System")
	UAnimMontage* GetGetUpAnimation(bool bRagdollFaceUpState);

	UFUNCTION(BlueprintCallable, Category = "ALS|Ragdoll System")
	virtual void RagdollStart();

	UFUNCTION(BlueprintCallable, Category = "ALS|Ragdoll System")
	virtual void RagdollEnd();

	UFUNCTION(BlueprintCallable, Server, Unreliable, Category = "ALS|Ragdoll System")
	void Server_SetMeshLocationDuringRagdoll(FVector MeshLocation);

	UFUNCTION(BlueprintCallable, Server, Unreliable, Category = "Camera System")
		void Server_SetCameraRotation(FRotator Rot);

	UFUNCTION(BlueprintCallable, Server, Unreliable, Category = "Camera System")
		void Server_SetPitchAndYaw(float Yaw, float Pitch, FRotator SlerperRotation);

	/** Character States */

	UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
	void SetMovementState(EALSMovementState NewState);

	UFUNCTION(BlueprintGetter, Category = "ALS|Character States")
	EALSMovementState GetMovementState() const { return MovementState; }

	UFUNCTION(BlueprintGetter, Category = "ALS|Character States")
	EALSMovementState GetPrevMovementState() const { return PrevMovementState; }

	UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
	void SetMovementAction(EALSMovementAction NewAction);

	UFUNCTION(BlueprintGetter, Category = "ALS|Character States")
	EALSMovementAction GetMovementAction() const { return MovementAction; }

	UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
	void SetStance(EALSStance NewStance);

	UFUNCTION(BlueprintGetter, Category = "ALS|Character States")
	EALSStance GetStance() const { return Stance; }

	UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
	void SetGait(EALSGait NewGait);

	UFUNCTION(BlueprintGetter, Category = "ALS|Character States")
	EALSGait GetGait() const { return Gait; }
	

	UFUNCTION(BlueprintGetter, Category = "ALS|CharacterStates")
	EALSGait GetDesiredGait() const { return DesiredGait; }

	UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
	void SetRotationMode(EALSRotationMode NewRotationMode);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "ALS|Character States")
	void Server_SetRotationMode(EALSRotationMode NewRotationMode);

	UFUNCTION(BlueprintGetter, Category = "ALS|Character States")
	EALSRotationMode GetRotationMode() const { return RotationMode; }

	UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
	void SetViewMode(EALSViewMode NewViewMode);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "ALS|Character States")
	void Server_SetViewMode(EALSViewMode NewViewMode);

	UFUNCTION(BlueprintGetter, Category = "ALS|Character States")
	EALSViewMode GetViewMode() const { return ViewMode; }

	UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
	void SetOverlayState(EALSOverlayState NewState);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "ALS|Character States")
	void Server_SetOverlayState(EALSOverlayState NewState);

	UFUNCTION(BlueprintGetter, Category = "ALS|Character States")
	EALSOverlayState GetOverlayState() const { return OverlayState; }

	UFUNCTION(BlueprintGetter, Category = "ALS|Character States")
	EALSOverlayState SwitchRight() const { return OverlayState; }

	/** Landed, Jumped, Rolling, Mantling and Ragdoll*/
	/** On Landed*/
	UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
	void EventOnLanded();

	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "ALS|Character States")
	void Multicast_OnLanded();

	/** On Jumped*/
	UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
	void EventOnJumped();

	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "ALS|Character States")
	void Multicast_OnJumped();

	/** Rolling Montage Play Replication*/
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "ALS|Character States")
	void Server_PlayMontage(UAnimMontage* montage, float track);

	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "ALS|Character States")
	void Multicast_PlayMontage(UAnimMontage* montage, float track);

	/** Mantling*/
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "ALS|Character States")
	void Server_MantleStart(float MantleHeight, const FALSComponentAndTransform& MantleLedgeWS,
	                        EALSMantleType MantleType);

	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "ALS|Character States")
	void Multicast_MantleStart(float MantleHeight, const FALSComponentAndTransform& MantleLedgeWS,
	                           EALSMantleType MantleType);

	/** Ragdolling*/
	UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
	void ReplicatedRagdollStart();

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "ALS|Character States")
	void Server_RagdollStart();

	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "ALS|Character States")
	void Multicast_RagdollStart();

	UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
	void ReplicatedRagdollEnd();

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "ALS|Character States")
	void Server_RagdollEnd(FVector CharacterLocation);

	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "ALS|Character States")
	void Multicast_RagdollEnd(FVector CharacterLocation);

	/** Input */

	UFUNCTION(BlueprintGetter, Category = "ALS|Input")
	EALSStance GetDesiredStance() const { return DesiredStance; }

	UFUNCTION(BlueprintSetter, Category = "ALS|Input")
	void SetDesiredStance(EALSStance NewStance);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "ALS|Input")
	void Server_SetDesiredStance(EALSStance NewStance);

	UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
	void SetDesiredGait(EALSGait NewGait);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "ALS|Character States")
	void Server_SetDesiredGait(EALSGait NewGait);

	UFUNCTION(BlueprintGetter, Category = "ALS|Input")
	EALSRotationMode GetDesiredRotationMode() const { return DesiredRotationMode; }

	UFUNCTION(BlueprintSetter, Category = "ALS|Input")
	void SetDesiredRotationMode(EALSRotationMode NewRotMode);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "ALS|Character States")
	void Server_SetDesiredRotationMode(EALSRotationMode NewRotMode);

	UFUNCTION(BlueprintCallable, Category = "ALS|Input")
	FVector GetPlayerMovementInput() const;

	/** Rotation System */

	UFUNCTION(BlueprintCallable, Category = "ALS|Rotation System")
	void SetActorLocationAndTargetRotation(FVector NewLocation, FRotator NewRotation);

	/** Mantle System */

	/** Implement on BP to get correct mantle parameter set according to character state */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "ALS|Mantle System")
	FALSMantleAsset GetMantleAsset(EALSMantleType MantleType);

	UFUNCTION(BlueprintCallable, Category = "ALS|Mantle System")
	virtual bool MantleCheckGrounded();

	UFUNCTION(BlueprintCallable, Category = "ALS|Mantle System")
	virtual bool MantleCheckFalling();

	/** Movement System */

	UFUNCTION(BlueprintGetter, Category = "ALS|Movement System")
	bool HasMovementInput() const { return bHasMovementInput; }

	UFUNCTION(BlueprintCallable, Category = "ALS|Movement System")
	void SetHasMovementInput(bool bNewHasMovementInput);

	UFUNCTION(BlueprintCallable, Category = "ALS|Movement System")
	FALSMovementSettings GetTargetMovementSettings() const;

	UFUNCTION(BlueprintCallable, Category = "ALS|Movement System")
	EALSGait GetAllowedGait() const;

	UFUNCTION(BlueprintCallable, Category = "ALS|Movement States")
	EALSGait GetActualGait(EALSGait AllowedGait) const;

	UFUNCTION(BlueprintCallable, Category = "ALS|Movement System")
	bool CanSprint() const;

	/** BP implementable function that called when Breakfall starts */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ALS|Movement System")
	void OnBreakfall();
	virtual void OnBreakfall_Implementation();

	/** BP implementable function that called when A Montage starts, e.g. during rolling */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ALS|Movement System")
	void Replicated_PlayMontage(UAnimMontage* montage, float track);
	virtual void Replicated_PlayMontage_Implementation(UAnimMontage* montage, float track);

	/** Implement on BP to get required roll animation according to character's state */
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "ALS|Movement System")
	UAnimMontage* GetRollAnimation();

	/** Utility */

	UFUNCTION(BlueprintCallable, Category = "ALS|Utility")
	float GetAnimCurveValue(FName CurveName) const;

	/** Implement on BP to draw debug spheres */
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "ALS|Debug")
	void DrawDebugSpheres();

	/** Camera System */

	UFUNCTION(BlueprintGetter, Category = "ALS|Camera System")
	bool IsRightShoulder() const { return bRightShoulder; }

	UFUNCTION(BlueprintCallable, Category = "ALS|Camera System")
	void SetRightShoulder(bool bNewRightShoulder) { bRightShoulder = bNewRightShoulder; }

	UFUNCTION(BlueprintCallable, Category = "ALS|Camera System")
	virtual ECollisionChannel GetThirdPersonTraceParams(FVector& TraceOrigin, float& TraceRadius);

	UFUNCTION(BlueprintCallable, Category = "ALS|Camera System")
	virtual FTransform GetThirdPersonPivotTarget();

	UFUNCTION(BlueprintCallable, Category = "ALS|Camera System")
	virtual FVector GetFirstPersonCameraTarget();

	virtual UCameraComponent* GetFirstPersonCamera();


	virtual UStaticMeshComponent* GetCameraPoll();

	FRotator GetFirstPersonCameraRotation();

	UFUNCTION(BlueprintCallable, Category = "ALS|Camera System")
	void GetCameraParameters(float& TPFOVOut, float& FPFOVOut, bool& bRightShoulderOut) const;

	/** Essential Information Getters/Setters */

	UFUNCTION(BlueprintGetter, Category = "ALS|Essential Information")
	FVector GetAcceleration() const { return Acceleration; }

	UFUNCTION(BlueprintCallable, Category = "ALS|Essential Information")
	void SetAcceleration(const FVector& NewAcceleration);

	UFUNCTION(BlueprintGetter, Category = "ALS|Essential Information")
	bool IsMoving() const { return bIsMoving; }

	UFUNCTION(BlueprintCallable, Category = "ALS|Essential Information")
	void SetIsMoving(bool bNewIsMoving);

	UFUNCTION(BlueprintCallable, Category = "ALS|Essential Information")
	FVector GetMovementInput() const;

	UFUNCTION(BlueprintGetter, Category = "ALS|Essential Information")
	float GetMovementInputAmount() const { return MovementInputAmount; }

	UFUNCTION(BlueprintCallable, Category = "ALS|Essential Information")
	void SetMovementInputAmount(float NewMovementInputAmount);

	UFUNCTION(BlueprintGetter, Category = "ALS|Essential Information")
	float GetSpeed() const { return Speed; }

	UFUNCTION(BlueprintCallable, Category = "ALS|Essential Information")
	void SetSpeed(float NewSpeed);

	UFUNCTION(BlueprintCallable)
	FRotator GetAimingRotation() const { return AimingRotation; }

	UFUNCTION(BlueprintCallable)
	float GetDeltaPitch() const { return DeltaPitch; }

	UFUNCTION(BlueprintCallable)
		float GetDeltaYaw() const { return DeltaYaw; }

	UFUNCTION(BlueprintGetter, Category = "ALS|Essential Information")
	float GetAimYawRate() const { return AimYawRate; }

	UFUNCTION(BlueprintCallable, Category = "ALS|Essential Information")
	void SetAimYawRate(float NewAimYawRate);

	UFUNCTION(BlueprintCallable, Category = "ALS|Essential Information")
	void GetControlForwardRightVector(FVector& Forward, FVector& Right) const;

	/** Fighting and stuff **/

	void OnFire();

	FVector GetReplicatedForward();

protected:
	
	bool IsUseTraceValid(FHitResult InHit, FVector Start, FVector End);
	

	/** Ragdoll System */

	void RagdollUpdate(float DeltaTime);

	void SetActorLocationDuringRagdoll(float DeltaTime);

	/** State Changes */

	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;

	virtual void OnMovementStateChanged(EALSMovementState PreviousState);

	virtual void OnMovementActionChanged(EALSMovementAction PreviousAction);

	virtual void OnStanceChanged(EALSStance PreviousStance);

	virtual void OnRotationModeChanged(EALSRotationMode PreviousRotationMode);

	virtual void OnGaitChanged(EALSGait PreviousGait);

	virtual void OnViewModeChanged(EALSViewMode PreviousViewMode);

	virtual void OnOverlayStateChanged(EALSOverlayState PreviousState);

	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	virtual void OnJumped_Implementation() override;

	virtual void Landed(const FHitResult& Hit) override;

	void OnLandFrictionReset();

	void SetEssentialValues(float DeltaTime);

	void UpdateCharacterMovement();

	void UpdateDynamicMovementSettingsNetworked(EALSGait AllowedGait);

	void UpdateDynamicMovementSettingsStandalone(EALSGait AllowedGait);

	void UpdateGroundedRotation(float DeltaTime);

	void UpdateInAirRotation(float DeltaTime);

	/** Mantle System */

	virtual void MantleStart(float MantleHeight, const FALSComponentAndTransform& MantleLedgeWS,
	                         EALSMantleType MantleType);

	virtual bool MantleCheck(const FALSMantleTraceSettings& TraceSettings,
	                         EDrawDebugTrace::Type DebugType = EDrawDebugTrace::Type::ForOneFrame);

	UFUNCTION()
	virtual void MantleUpdate(float BlendIn);

	UFUNCTION()
	virtual void MantleEnd();

	/** Utils */

	float GetMappedSpeed() const;

	void SmoothCharacterRotationYaw(float DeltaQuatYaw, FRotator Target, float TargetInterpSpeed, float ActorInterpSpeed, float DeltaTime);

	void SmoothCharacterRotation(FRotator Target, float TargetInterpSpeed, float ActorInterpSpeed, float DeltaTime);


	void SmoothGravityCharacterRotation(FRotator Target, float TargetInterpSpeed, float ActorInterpSpeed, float DeltaTime);

	float CalculateGroundedRotationRate() const;

	void LimitRotation(float AimYawMin, float AimYawMax, float InterpSpeed, float DeltaTime);

	void SetMovementModel();

	/** Input */

	void PlayerForwardMovementInput(float Value);

	void PlayerRightMovementInput(float Value);

	void PlayerCameraUpInput(float Value);

	void PlayerCameraRightInput(float Value);

	void UsePressedAction();

	void ReloadPressedAction();

	void JumpPressedAction();

	void EscMenuPressedAction();

	void JumpReleasedAction();

	void SprintPressedAction();

	void SprintReleasedAction();

	void AimPressedAction();

	void AimReleasedAction();

	void CameraPressedAction();

	void CameraReleasedAction();

	void OnSwitchCameraMode();

	void StancePressedAction();

	void WalkPressedAction();

	void RagdollPressedAction();

	void VelocityDirectionPressedAction();

	void LookingDirectionPressedAction();

	/** Replication */
	UFUNCTION()
	void OnRep_RotationMode(EALSRotationMode PrevRotMode);

	UFUNCTION()
	void OnRep_ViewMode(EALSViewMode PrevViewMode);

	UFUNCTION()
	void OnRep_OverlayState(EALSOverlayState PrevOverlayState);
	 
	 // weapon stuff 

	UPROPERTY(Transient, Replicated)
		uint8 bIsTargeting : 1;

	/** notification when killed, for both the server and client. */
	virtual void OnDeath(float KillingDamage, struct FDamageEvent const& DamageEvent, class APawn* InstigatingPawn, class AActor* DamageCauser);

	/** sets up the replication for taking a hit */
	void ReplicateHit(float Damage, struct FDamageEvent const& DamageEvent, class APawn* InstigatingPawn, class AActor* DamageCauser, bool bKilled);
protected:
//GasMovement System
	int32 CurrentRoomID;
	
	int32 PredictedNextRoomID;

	/* Custom movement component*/
	UPROPERTY()
		UALSCharacterMovementComponent* MyCharacterMovementComponent;

	/** Replicate where this pawn was last hit and damaged */
	UPROPERTY(Transient, ReplicatedUsing = OnRep_LastTakeHitInfo)
		struct FTakeHitInfo LastTakeHitInfo;

	/** play hit or death on client */
	UFUNCTION()
		void OnRep_LastTakeHitInfo();

	/** Time at which point the last take hit info for the actor times out and won't be replicated; Used to stop join-in-progress effects all over the screen */
	float LastTakeHitTimeTimeout;

	/** play effects on hit */
	virtual void PlayHit(float DamageTaken, struct FDamageEvent const& DamageEvent, class APawn* PawnInstigator, class AActor* DamageCauser);

	/** Input */
	UPROPERTY(EditAnywhere, Category = "ALS|Input")
	float RotationLerpRate = 1.f;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = "ALS|Input")
	EALSRotationMode DesiredRotationMode = EALSRotationMode::LookingDirection;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = "ALS|Input")
	EALSGait DesiredGait = EALSGait::Running;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "ALS|Input")
	EALSStance DesiredStance = EALSStance::Standing;

	UPROPERTY(EditDefaultsOnly, Category = "ALS|Input", BlueprintReadOnly)
	float LookUpDownRate = 1.25f;

	UPROPERTY(EditDefaultsOnly, Category = "ALS|Input", BlueprintReadOnly)
	float LookLeftRightRate = 1.25f;

	UPROPERTY(EditDefaultsOnly, Category = "ALS|Input", BlueprintReadOnly)
	float RollDoubleTapTimeout = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category = "ALS|Input", BlueprintReadOnly)
	float ViewModeSwitchHoldTime = 0.2f;

	UPROPERTY(Category = "ALS|Input", BlueprintReadOnly)
	int32 TimesPressedStance = 0;

	UPROPERTY(Category = "ALS|Input", BlueprintReadOnly)
	bool bBreakFall = false;

	UPROPERTY(Category = "ALS|Input", BlueprintReadOnly)
	bool bSprintHeld = false;

	/** Camera System */

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ALS|Camera System")
	float ThirdPersonFOV = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ALS|Camera System")
	float FirstPersonFOV = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ALS|Camera System")
	bool bRightShoulder = false;

	/** State Values */

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ALS|State Values", ReplicatedUsing = OnRep_OverlayState)
	EALSOverlayState OverlayState = EALSOverlayState::Default;

	/** Movement System */

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Movement System")
	FALSMovementSettings CurrentMovementSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ALS|Movement System")
	FDataTableRowHandle MovementModel;

	/** Mantle System */

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ALS|Mantle System")
	FALSMantleTraceSettings GroundedTraceSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ALS|Mantle System")
	FALSMantleTraceSettings AutomaticTraceSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ALS|Mantle System")
	FALSMantleTraceSettings FallingTraceSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ALS|Mantle System")
	UCurveFloat* MantleTimelineCurve;

	/** If a dynamic object has a velocity bigger than this value, do not start mantle */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ALS|Mantle System")
	float AcceptableVelocityWhileMantling = 10.0f;

	/** Components */

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ALS|Components")
	UTimelineComponent* MantleTimeline = nullptr;

	/** Essential Information */

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Essential Information")
	FVector Acceleration = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Essential Information")
	bool bIsMoving = false;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Essential Information")
	bool bHasMovementInput = false;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Essential Information")
	FRotator LastVelocityRotation;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Essential Information")
	FVector LastVelocityDirection;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Essential Information")
	FRotator LastMovementInputRotation;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Essential Information")
	float Speed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Essential Information")
	float MovementInputAmount = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Essential Information")
	float AimYawRate = 0.0f;

	/** Replicated Essential Information*/

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Essential Information")
	float EasedMaxAcceleration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "ALS|Essential Information")
	FVector ReplicatedCurrentAcceleration = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "ALS|Essential Information")
	FRotator ReplicatedControlRotation = FRotator::ZeroRotator;
	
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "ALS|Essential Information")
		FVector ReplicatedGravityDirection = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "ALS|Essential Information")
		FRotator ReplicatedQuatYawRotation = FRotator::ZeroRotator;

		FVector LocalCorrectedRight; 
		FVector LocalCorrectedForward;

		
		

	/** State Values */

	UPROPERTY(BlueprintReadOnly, Category = "ALS|State Values")
	EALSMovementState MovementState = EALSMovementState::None;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|State Values")
	EALSMovementState PrevMovementState = EALSMovementState::None;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|State Values")
	EALSMovementAction MovementAction = EALSMovementAction::None;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|State Values", ReplicatedUsing = OnRep_RotationMode)
	EALSRotationMode RotationMode = EALSRotationMode::LookingDirection;
	


	UPROPERTY(BlueprintReadOnly, Category = "ALS|State Values")
	EALSGait Gait = EALSGait::Walking;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ALS|State Values")
	EALSStance Stance = EALSStance::Standing;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ALS|State Values", ReplicatedUsing = OnRep_ViewMode)
	EALSViewMode ViewMode = EALSViewMode::ThirdPerson;

	/** Movement System */

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Movement System")
	FALSMovementStateSettings MovementData;

	/** Rotation System */

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Rotation System")
	FRotator TargetRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Rotation System")
	FRotator InAirRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Rotation System")
	float YawOffset = 0.0f;

	/** Mantle System */

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Mantle System")
	FALSMantleParams MantleParams;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Mantle System")
	FALSComponentAndTransform MantleLedgeLS;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Mantle System")
	FTransform MantleTarget = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Mantle System")
	FTransform MantleActualStartOffset = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Mantle System")
	FTransform MantleAnimatedStartOffset = FTransform::Identity;

	/** Breakfall System */

	/** If player hits to the ground with a specified amount of velocity, switch to breakfall state */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "ALS|Breakfall System")
	bool bBreakfallOnLand = true;

	/** If player hits to the ground with an amount of velocity greater than specified value, switch to breakfall state */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "ALS|Breakfall System", meta = (EditCondition =
		"bBreakfallOnLand"))
	float BreakfallOnLandVelocity = 600.0f;

	/** Ragdoll System */

	/** If player hits to the ground with a specified amount of velocity, switch to ragdoll state */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "ALS|Ragdoll System")
	bool bRagdollOnLand = false;

	/** If player hits to the ground with an amount of velocity greater than specified value, switch to ragdoll state */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "ALS|Ragdoll System", meta = (EditCondition =
		"bRagdollOnLand"))
	float RagdollOnLandVelocity = 1000.0f;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Ragdoll System")
	bool bRagdollOnGround = false;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Ragdoll System")
	bool bRagdollFaceUp = false;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Ragdoll System")
	FVector LastRagdollVelocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = Health)
	float MaxStunTimerValue = 4.f;

	float StunTimer = MaxStunTimerValue;
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "ALS|Ragdoll System")
	FVector TargetRagdollLocation = FVector::ZeroVector;


	/* Server ragdoll pull force storage*/
	float ServerRagdollPull = 0.0f;

	/* Dedicated server mesh default visibility based anim tick option*/
	EVisibilityBasedAnimTickOption DefVisBasedTickOp;

	/** Cached Variables */

	FVector PreviousVelocity = FVector::ZeroVector;

	float PreviousAimYaw = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	UALSCharacterAnimInstance* MainAnimInstance = nullptr;

	/** Last time the 'first' crouch/roll button is pressed */
	float LastStanceInputTime = 0.0f;

	/** Last time the camera action button is pressed */
	float CameraActionPressedTime = 0.0f;

	/* Timer to manage camera mode swap action */
	FTimerHandle OnCameraModeSwapTimer;

	/* Timer to manage reset of braking friction factor after on landed event */
	FTimerHandle OnLandedFrictionResetTimer;

	/* Smooth out aiming by interping control rotation*/
	FRotator AimingRotation = FRotator::ZeroRotator;
	FRotator QuatYawRotation = FRotator::ZeroRotator;
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CameraSystem")
	float DeltaPitch = 0.f;
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CameraSystem")
	float DeltaYaw = 0.f;

	void UpdateDeltaPitch();
	void UpdateDeltaYaw();

	/** We won't use curve based movement on networked games */
	bool bDisableCurvedMovement = false;


	UFUNCTION(reliable, server, WithValidation)
		void ServerSetTargeting(bool bNewTargeting);

	UFUNCTION(reliable, server, WithValidation)
		void ServerTestActionRep();

	void TestActionRep();

};
