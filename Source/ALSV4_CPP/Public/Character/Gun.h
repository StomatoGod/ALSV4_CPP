// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Canvas.h" // for FCanvasIcon
#include "GameFramework/Actor.h"
#include "Gun.generated.h"

class UAnimMontage;
class AALSBaseCharacter;
class UAudioComponent;
class UParticleSystemComponent;
class UForceFeedbackEffect;
class USoundCue;

namespace EGunState
{
	enum Type
	{
		Idle,
		Firing,
		Reloading,
		Equipping,
	};
}

USTRUCT()
struct FGunData
{
	GENERATED_USTRUCT_BODY()

		/** inifite ammo for reloads */
		UPROPERTY(EditDefaultsOnly, Category = Ammo)
		bool bInfiniteAmmo;

	/** infinite ammo in clip, no reload required */
	UPROPERTY(EditDefaultsOnly, Category = Ammo)
		bool bInfiniteClip;

	/** max ammo */
	UPROPERTY(EditDefaultsOnly, Category = Ammo)
		int32 MaxAmmo;

	/** clip size */
	UPROPERTY(EditDefaultsOnly, Category = Ammo)
		int32 AmmoPerClip;

	/** initial clips */
	UPROPERTY(EditDefaultsOnly, Category = Ammo)
		int32 InitialClips;

	/** time between two consecutive shots */
	UPROPERTY(EditDefaultsOnly, Category = GunStat)
		float TimeBetweenShots;

	/** failsafe reload duration if Gun doesn't have any animation for it */
	UPROPERTY(EditDefaultsOnly, Category = GunStat)
		float NoAnimReloadDuration;

	/** defaults */
	FGunData()
	{
		bInfiniteAmmo = false;
		bInfiniteClip = false;
		MaxAmmo = 100;
		AmmoPerClip = 20;
		InitialClips = 4;
		TimeBetweenShots = 0.2f;
		NoAnimReloadDuration = 1.0f;
	}
};

USTRUCT()
struct FGunAnim
{
	GENERATED_USTRUCT_BODY()

		/** animation played on pawn (1st person view) */
		UPROPERTY(EditDefaultsOnly, Category = Animation)
		UAnimMontage* Pawn1P;

	/** animation played on pawn (3rd person view) */
	UPROPERTY(EditDefaultsOnly, Category = Animation)
		UAnimMontage* Pawn3P;
};

//UCLASS(Abstract, Blueprintable)
UCLASS(Blueprintable)
class ALSV4_CPP_API AGun : public AActor
{
	//GENERATED_BODY()
	GENERATED_UCLASS_BODY()

		/** perform initial setup */
		virtual void PostInitializeComponents() override;

	virtual void Destroyed() override;

	//////////////////////////////////////////////////////////////////////////
	// Ammo

	enum class EAmmoType
	{
		EBullet,
		ERocket,
		EMax,
	};

	/** [server] add ammo */
	void GiveAmmo(int AddAmount);

	/** consume a bullet */
	void UseAmmo();

	/** query ammo type */
	virtual EAmmoType GetAmmoType() const
	{
		return EAmmoType::EBullet;
	}

	//////////////////////////////////////////////////////////////////////////
	// Inventory

	/** Gun is being equipped by owner pawn */
	virtual void OnEquip(const AGun* LastGun);

	/** Gun is now equipped by owner pawn */
	virtual void OnEquipFinished();

	/** Gun is holstered by owner pawn */
	virtual void OnUnEquip();

	/** [server] Gun was added to pawn's inventory */
	virtual void OnEnterInventory(AALSBaseCharacter* NewOwner);

	/** [server] Gun was removed from pawn's inventory */
	virtual void OnLeaveInventory();

	/** check if it's currently equipped */
	bool IsEquipped() const;

	/** check if mesh is already attached */
	bool IsAttachedToPawn() const;


	//////////////////////////////////////////////////////////////////////////
	// Input

	/** [local + server] start Gun fire */
	virtual void StartFire();

	/** [local + server] stop Gun fire */
	virtual void StopFire();

	/** [all] start Gun reload */
	virtual void StartReload(bool bFromReplication = false);

	/** [local + server] interrupt Gun reload */
	virtual void StopReload();

	/** [server] performs actual reload */
	virtual void ReloadGun();

	/** trigger reload from server */
	UFUNCTION(reliable, client)
		void ClientStartReload();


	//////////////////////////////////////////////////////////////////////////
	// Control

	/** check if Gun can fire */
	bool CanFire() const;

	/** check if Gun can be reloaded */
	bool CanReload() const;


	//////////////////////////////////////////////////////////////////////////
	// Reading data

	/** get current Gun state */
	EGunState::Type GetCurrentState() const;

	/** get current ammo amount (total) */
	int32 GetCurrentAmmo() const;

	/** get current ammo amount (clip) */
	int32 GetCurrentAmmoInClip() const;

	/** get clip size */
	int32 GetAmmoPerClip() const;

	/** get max ammo amount */
	int32 GetMaxAmmo() const;

	/** get Gun mesh (needs pawn owner to determine variant) */
	USkeletalMeshComponent* GetGunMesh() const;

	/** get pawn owner */
	UFUNCTION(BlueprintCallable, Category = "Game|Gun")
		class AALSBaseCharacter* GetPawnOwner() const;

	/** icon displayed on the HUD when Gun is equipped as primary */
	UPROPERTY(EditDefaultsOnly, Category = HUD)
		FCanvasIcon PrimaryIcon;

	/** icon displayed on the HUD when Gun is secondary */
	UPROPERTY(EditDefaultsOnly, Category = HUD)
		FCanvasIcon SecondaryIcon;

	/** bullet icon used to draw current clip (left side) */
	UPROPERTY(EditDefaultsOnly, Category = HUD)
		FCanvasIcon PrimaryClipIcon;

	/** bullet icon used to draw secondary clip (left side) */
	UPROPERTY(EditDefaultsOnly, Category = HUD)
		FCanvasIcon SecondaryClipIcon;

	/** how many icons to draw per clip */
	UPROPERTY(EditDefaultsOnly, Category = HUD)
		float AmmoIconsCount;

	/** defines spacing between primary ammo icons (left side) */
	UPROPERTY(EditDefaultsOnly, Category = HUD)
		int32 PrimaryClipIconOffset;

	/** defines spacing between secondary ammo icons (left side) */
	UPROPERTY(EditDefaultsOnly, Category = HUD)
		int32 SecondaryClipIconOffset;

	/** crosshair parts icons (left, top, right, bottom and center) */
	UPROPERTY(EditDefaultsOnly, Category = HUD)
		FCanvasIcon Crosshair[5];

	/** crosshair parts icons when targeting (left, top, right, bottom and center) */
	UPROPERTY(EditDefaultsOnly, Category = HUD)
		FCanvasIcon AimingCrosshair[5];

	/** only use red colored center part of aiming crosshair */
	UPROPERTY(EditDefaultsOnly, Category = HUD)
		bool UseLaserDot;

	/** false = default crosshair */
	UPROPERTY(EditDefaultsOnly, Category = HUD)
		bool UseCustomCrosshair;

	/** false = use custom one if set, otherwise default crosshair */
	UPROPERTY(EditDefaultsOnly, Category = HUD)
		bool UseCustomAimingCrosshair;

	/** true - crosshair will not be shown unless aiming with the Gun */
	UPROPERTY(EditDefaultsOnly, Category = HUD)
		bool bHideCrosshairWhileNotAiming;

	/** Adjustment to handle frame rate affecting actual timer interval. */
	UPROPERTY(Transient)
		float TimerIntervalAdjustment;

	/** Whether to allow automatic Guns to catch up with shorter refire cycles */
	UPROPERTY(Config)
		bool bAllowAutomaticGunCatchup = true;

	/** check if Gun has infinite ammo (include owner's cheats) */
	bool HasInfiniteAmmo() const;

	/** check if Gun has infinite clip (include owner's cheats) */
	bool HasInfiniteClip() const;

	/** set the Gun's owning pawn */
	void SetOwningPawn(AALSBaseCharacter* AALSBaseCharacter);

	/** gets last time when this Gun was switched to */
	float GetEquipStartedTime() const;

	/** gets the duration of equipping Gun*/
	float GetEquipDuration() const;

protected:

	/** pawn owner */
	UPROPERTY(Transient, ReplicatedUsing = OnRep_MyPawn)
		class AALSBaseCharacter* MyPawn;

	/** Gun data */
	UPROPERTY(EditDefaultsOnly, Category = Config)
		FGunData GunConfig;

private:
	/** Gun mesh: 1st person view */
	UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
		USkeletalMeshComponent* Mesh1P;

	/** Gun mesh: 3rd person view */
	UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
		USkeletalMeshComponent* Mesh3P;
protected:

	/** firing audio (bLoopedFireSound set) */
	UPROPERTY(Transient)
		UAudioComponent* FireAC;

	/** name of bone/socket for muzzle in Gun mesh */
	UPROPERTY(EditDefaultsOnly, Category = Effects)
		FName MuzzleAttachPoint;

	/** FX for muzzle flash */
	UPROPERTY(EditDefaultsOnly, Category = Effects)
		UParticleSystem* MuzzleFX;

	/** spawned component for muzzle FX */
	UPROPERTY(Transient)
		UParticleSystemComponent* MuzzlePSC;

	/** spawned component for second muzzle FX (Needed for split screen) */
	UPROPERTY(Transient)
		UParticleSystemComponent* MuzzlePSCSecondary;

	
	/** force feedback effect to play when the Gun is fired */
	UPROPERTY(EditDefaultsOnly, Category = Effects)
		UForceFeedbackEffect* FireForceFeedback;

	/** single fire sound (bLoopedFireSound not set) */
	UPROPERTY(EditDefaultsOnly, Category = Sound)
		USoundCue* FireSound;

	/** looped fire sound (bLoopedFireSound set) */
	UPROPERTY(EditDefaultsOnly, Category = Sound)
		USoundCue* FireLoopSound;

	/** finished burst sound (bLoopedFireSound set) */
	UPROPERTY(EditDefaultsOnly, Category = Sound)
		USoundCue* FireFinishSound;

	/** out of ammo sound */
	UPROPERTY(EditDefaultsOnly, Category = Sound)
		USoundCue* OutOfAmmoSound;

	/** reload sound */
	UPROPERTY(EditDefaultsOnly, Category = Sound)
		USoundCue* ReloadSound;

	/** reload animations */
	UPROPERTY(EditDefaultsOnly, Category = Animation)
		FGunAnim ReloadAnim;

	/** equip sound */
	UPROPERTY(EditDefaultsOnly, Category = Sound)
		USoundCue* EquipSound;

	/** equip animations */
	UPROPERTY(EditDefaultsOnly, Category = Animation)
		FGunAnim EquipAnim;

	/** fire animations */
	UPROPERTY(EditDefaultsOnly, Category = Animation)
		FGunAnim FireAnim;

	/** is muzzle FX looped? */
	UPROPERTY(EditDefaultsOnly, Category = Effects)
		uint32 bLoopedMuzzleFX : 1;

	/** is fire sound looped? */
	UPROPERTY(EditDefaultsOnly, Category = Sound)
		uint32 bLoopedFireSound : 1;

	/** is fire animation looped? */
	UPROPERTY(EditDefaultsOnly, Category = Animation)
		uint32 bLoopedFireAnim : 1;

	/** is fire animation playing? */
	uint32 bPlayingFireAnim : 1;

	/** is Gun currently equipped? */
	uint32 bIsEquipped : 1;

	/** is Gun fire active? */
	uint32 bWantsToFire : 1;

	/** is reload animation playing? */
	UPROPERTY(Transient, ReplicatedUsing = OnRep_Reload)
		uint32 bPendingReload : 1;

	/** is equip animation playing? */
	uint32 bPendingEquip : 1;

	/** Gun is refiring */
	uint32 bRefiring;

	/** current Gun state */
	EGunState::Type CurrentState;

	/** time of last successful Gun fire */
	float LastFireTime;

	/** last time when this Gun was switched to */
	float EquipStartedTime;

	/** how much time Gun needs to be equipped */
	float EquipDuration;

	/** current total ammo */
	UPROPERTY(Transient, Replicated)
		int32 CurrentAmmo;

	/** current ammo - inside clip */
	UPROPERTY(Transient, Replicated)
		int32 CurrentAmmoInClip;

	/** burst counter, used for replicating fire events to remote clients */
	UPROPERTY(Transient, ReplicatedUsing = OnRep_BurstCounter)
		int32 BurstCounter;

	/** Handle for efficient management of OnEquipFinished timer */
	FTimerHandle TimerHandle_OnEquipFinished;

	/** Handle for efficient management of StopReload timer */
	FTimerHandle TimerHandle_StopReload;

	/** Handle for efficient management of ReloadGun timer */
	FTimerHandle TimerHandle_ReloadGun;

	/** Handle for efficient management of HandleFiring timer */
	FTimerHandle TimerHandle_HandleFiring;

	//////////////////////////////////////////////////////////////////////////
	// Input - server side

	UFUNCTION(reliable, server, WithValidation)
		void ServerStartFire();

	UFUNCTION(reliable, server, WithValidation)
		void ServerStopFire();

	UFUNCTION(reliable, server, WithValidation)
		void ServerStartReload();

	UFUNCTION(reliable, server, WithValidation)
		void ServerStopReload();


	//////////////////////////////////////////////////////////////////////////
	// Replication & effects

	UFUNCTION()
		void OnRep_MyPawn();

	UFUNCTION()
		void OnRep_BurstCounter();

	UFUNCTION()
		void OnRep_Reload();

	/** Called in network play to do the cosmetic fx for firing */
	virtual void SimulateGunFire();

	/** Called in network play to stop cosmetic fx (e.g. for a looping shot). */
	virtual void StopSimulatingGunFire();


	//////////////////////////////////////////////////////////////////////////
	// Gun usage

	/** [local] Gun specific fire implementation */
	virtual void FireGun() PURE_VIRTUAL(AGun::FireGun, );

	/** [server] fire & update ammo */
	UFUNCTION(reliable, server, WithValidation)
		void ServerHandleFiring();

	/** [local + server] handle Gun refire, compensating for slack time if the timer can't sample fast enough */
	void HandleReFiring();

	/** [local + server] handle Gun fire */
	void HandleFiring();

	/** [local + server] firing started */
	virtual void OnBurstStarted();

	/** [local + server] firing finished */
	virtual void OnBurstFinished();

	/** update Gun state */
	void SetGunState(EGunState::Type NewState);

	/** determine current Gun state */
	void DetermineGunState();


	//////////////////////////////////////////////////////////////////////////
	// Inventory

	/** attaches Gun mesh to pawn's mesh */
	void AttachMeshToPawn();

	/** detaches Gun mesh from pawn */
	void DetachMeshFromPawn();


	//////////////////////////////////////////////////////////////////////////
	// Gun usage helpers

	/** play Gun sounds */
	UAudioComponent* PlayGunSound(USoundCue* Sound);

	/** play Gun animations */
	float PlayGunAnimation(const FGunAnim& Animation);

	/** stop playing Gun animations */
	void StopGunAnimation(const FGunAnim& Animation);

	/** Get the aim of the Gun, allowing for adjustments to be made by the Gun */
	virtual FVector GetAdjustedAim() const;

	/** Get the aim of the camera */
	FVector GetCameraAim() const;

	/** get the originating location for camera damage */
	FVector GetCameraDamageStartLocation(const FVector& AimDir) const;

	/** get the muzzle location of the Gun */
	FVector GetMuzzleLocation() const;

	/** get direction of Gun's muzzle */
	FVector GetMuzzleDirection() const;

	/** find hit */
	FHitResult GunTrace(const FVector& TraceFrom, const FVector& TraceTo) const;

protected:
	/** Returns Mesh1P subobject **/
	FORCEINLINE USkeletalMeshComponent* GetMesh1P() const { return Mesh1P; }
	/** Returns Mesh3P subobject **/
	FORCEINLINE USkeletalMeshComponent* GetMesh3P() const { return Mesh3P; }
};

