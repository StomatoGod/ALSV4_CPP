// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Gun.h"
#include "Particles/ParticleSystemComponent.h"
#include "Character/ALSPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "Net/UnrealNetwork.h"
#include "Components/AudioComponent.h"
#include "Character/ALSBaseCharacter.h"


//TODO: Create player state. Handle NotifyEquipGun, Handle canFire, Handle Can Reload,
//Handle GetHud and NotifyHud from playercontroller (Happens in HandleFire)
//Handle Start and Stop Gun animation
// Handle GetAdjustedAim
// Handle Play Gun sound
AGun::AGun(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Mesh1P = ObjectInitializer.CreateDefaultSubobject<USkeletalMeshComponent>(this, TEXT("GunMesh1P"));
	Mesh1P->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	Mesh1P->bReceivesDecals = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetCollisionObjectType(ECC_WorldDynamic);
	Mesh1P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh1P->SetCollisionResponseToAllChannels(ECR_Ignore);
	RootComponent = Mesh1P;

	Mesh3P = ObjectInitializer.CreateDefaultSubobject<USkeletalMeshComponent>(this, TEXT("GunMesh3P"));
	Mesh3P->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	Mesh3P->bReceivesDecals = false;
	Mesh3P->CastShadow = true;
	Mesh3P->SetCollisionObjectType(ECC_WorldDynamic);
	Mesh3P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh3P->SetCollisionResponseToAllChannels(ECR_Ignore);
	//Mesh3P->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Mesh3P->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	//Mesh3P->SetCollisionResponseToChannel(COLLISION_PROJECTILE, ECR_Block);
	Mesh3P->SetupAttachment(Mesh1P);

	bLoopedMuzzleFX = false;
	bLoopedFireAnim = false;
	bPlayingFireAnim = false;
	bIsEquipped = false;
	bWantsToFire = false;
	bPendingReload = false;
	bPendingEquip = false;
	CurrentState = EGunState::Idle;

	CurrentAmmo = 0;
	CurrentAmmoInClip = 0;
	BurstCounter = 0;
	LastFireTime = 0.0f;

	
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	SetRemoteRoleForBackwardsCompat(ROLE_SimulatedProxy);
	bReplicates = true;
	bNetUseOwnerRelevancy = true;
}

void AGun::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (GunConfig.InitialClips > 0)
	{
		CurrentAmmoInClip = GunConfig.AmmoPerClip;
		CurrentAmmo = GunConfig.AmmoPerClip * GunConfig.InitialClips;
	}

	DetachMeshFromPawn();
}

void AGun::Destroyed()
{
	Super::Destroyed();

	StopSimulatingGunFire();
}

//////////////////////////////////////////////////////////////////////////
// Inventory

void AGun::OnEquip(const AGun* LastGun)
{
	AttachMeshToPawn();

	bPendingEquip = true;
	DetermineGunState();

	// Only play animation if last Gun is valid
	if (LastGun)
	{
		float Duration = PlayGunAnimation(EquipAnim);
		if (Duration <= 0.0f)
		{
			// failsafe
			Duration = 0.5f;
		}
		EquipStartedTime = GetWorld()->GetTimeSeconds();
		EquipDuration = Duration;

		GetWorldTimerManager().SetTimer(TimerHandle_OnEquipFinished, this, &AGun::OnEquipFinished, Duration, false);
	}
	else
	{
		OnEquipFinished();
	}

	if (MyPawn && MyPawn->IsLocallyControlled())
	{
		PlayGunSound(EquipSound);
	}

	//AALSBaseCharacter::NotifyEquipGun.Broadcast(MyPawn, this);
}

void AGun::OnEquipFinished()
{
	AttachMeshToPawn();

	bIsEquipped = true;
	bPendingEquip = false;

	// Determine the state so that the can reload checks will work
	DetermineGunState();

	if (MyPawn)
	{
		// try to reload empty clip
		if (MyPawn->IsLocallyControlled() &&
			CurrentAmmoInClip <= 0 &&
			CanReload())
		{
			StartReload();
		}
	}


}

void AGun::OnUnEquip()
{
	DetachMeshFromPawn();
	bIsEquipped = false;
	StopFire();

	if (bPendingReload)
	{
		StopGunAnimation(ReloadAnim);
		bPendingReload = false;

		GetWorldTimerManager().ClearTimer(TimerHandle_StopReload);
		GetWorldTimerManager().ClearTimer(TimerHandle_ReloadGun);
	}

	if (bPendingEquip)
	{
		StopGunAnimation(EquipAnim);
		bPendingEquip = false;

		GetWorldTimerManager().ClearTimer(TimerHandle_OnEquipFinished);
	}

	//AALSBaseCharacter::NotifyUnEquipGun.Broadcast(MyPawn, this);

	//DetermineGunState();
}

void AGun::OnEnterInventory(AALSBaseCharacter* NewOwner)
{
	SetOwningPawn(NewOwner);
}

void AGun::OnLeaveInventory()
{
	if (IsAttachedToPawn())
	{
		OnUnEquip();
	}

	if (GetLocalRole() == ROLE_Authority)
	{
		SetOwningPawn(NULL);
	}
}

void AGun::AttachMeshToPawn()
{
/**
	if (MyPawn)
	{
		// Remove and hide both first and third person meshes
		DetachMeshFromPawn();

		// For locally controller players we attach both Guns and let the bOnlyOwnerSee, bOwnerNoSee flags deal with visibility.
		FName AttachPoint = MyPawn->GetGunAttachPoint();
		if (MyPawn->IsLocallyControlled() == true)
		{
			USkeletalMeshComponent* PawnMesh1p = MyPawn->GetSpecifcPawnMesh(true);
			USkeletalMeshComponent* PawnMesh3p = MyPawn->GetSpecifcPawnMesh(false);
			Mesh1P->SetHiddenInGame(false);
			Mesh3P->SetHiddenInGame(false);
			Mesh1P->AttachToComponent(PawnMesh1p, FAttachmentTransformRules::KeepRelativeTransform, AttachPoint);
			Mesh3P->AttachToComponent(PawnMesh3p, FAttachmentTransformRules::KeepRelativeTransform, AttachPoint);
		}
		else
		{
			USkeletalMeshComponent* UseGunMesh = GetGunMesh();
			USkeletalMeshComponent* UsePawnMesh = MyPawn->GetPawnMesh();
			UseGunMesh->AttachToComponent(UsePawnMesh, FAttachmentTransformRules::KeepRelativeTransform, AttachPoint);
			UseGunMesh->SetHiddenInGame(false);
		}
	}
	**/
}

void AGun::DetachMeshFromPawn()
{
/**
	Mesh1P->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	Mesh1P->SetHiddenInGame(true);

	Mesh3P->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	Mesh3P->SetHiddenInGame(true);
	**/
}


//////////////////////////////////////////////////////////////////////////
// Input

void AGun::StartFire()
{
	if (GetLocalRole() < ROLE_Authority)
	{
		ServerStartFire();
	}

	if (!bWantsToFire)
	{
		bWantsToFire = true;
		DetermineGunState();
	}
}

void AGun::StopFire()
{
	if ((GetLocalRole() < ROLE_Authority) && MyPawn && MyPawn->IsLocallyControlled())
	{
		ServerStopFire();
	}

	if (bWantsToFire)
	{
		bWantsToFire = false;
		DetermineGunState();
	}
}

void AGun::StartReload(bool bFromReplication)
{
	if (!bFromReplication && GetLocalRole() < ROLE_Authority)
	{
		ServerStartReload();
	}

	if (bFromReplication || CanReload())
	{
		bPendingReload = true;
		DetermineGunState();

		float AnimDuration = PlayGunAnimation(ReloadAnim);
		if (AnimDuration <= 0.0f)
		{
			AnimDuration = GunConfig.NoAnimReloadDuration;
		}

		GetWorldTimerManager().SetTimer(TimerHandle_StopReload, this, &AGun::StopReload, AnimDuration, false);
		if (GetLocalRole() == ROLE_Authority)
		{
			GetWorldTimerManager().SetTimer(TimerHandle_ReloadGun, this, &AGun::ReloadGun, FMath::Max(0.1f, AnimDuration - 0.1f), false);
		}

		if (MyPawn && MyPawn->IsLocallyControlled())
		{
			PlayGunSound(ReloadSound);
		}
	}
}

void AGun::StopReload()
{
	if (CurrentState == EGunState::Reloading)
	{
		bPendingReload = false;
		DetermineGunState();
		StopGunAnimation(ReloadAnim);
	}
}

bool AGun::ServerStartFire_Validate()
{
	return true;
}

void AGun::ServerStartFire_Implementation()
{
	StartFire();
}

bool AGun::ServerStopFire_Validate()
{
	return true;
}

void AGun::ServerStopFire_Implementation()
{
	StopFire();
}

bool AGun::ServerStartReload_Validate()
{
	return true;
}

void AGun::ServerStartReload_Implementation()
{
	StartReload();
}

bool AGun::ServerStopReload_Validate()
{
	return true;
}

void AGun::ServerStopReload_Implementation()
{
	StopReload();
}

void AGun::ClientStartReload_Implementation()
{
	StartReload();
}

//////////////////////////////////////////////////////////////////////////
// Control

bool AGun::CanFire() const
{	
	
	bool bCanFire = MyPawn && MyPawn->CanFire();
	bool bStateOKToFire = ((CurrentState == EGunState::Idle) || (CurrentState == EGunState::Firing));
	return ((bCanFire == true) && (bStateOKToFire == true) && (bPendingReload == false));
}

bool AGun::CanReload() const
{
	bool bCanReload = false;
	//bool bCanReload = (!MyPawn || MyPawn->CanReload());
	bool bGotAmmo = (CurrentAmmoInClip < GunConfig.AmmoPerClip) && (CurrentAmmo - CurrentAmmoInClip > 0 || HasInfiniteClip());
	bool bStateOKToReload = ((CurrentState == EGunState::Idle) || (CurrentState == EGunState::Firing));
	return ((bCanReload == true) && (bGotAmmo == true) && (bStateOKToReload == true));
}


//////////////////////////////////////////////////////////////////////////
// Gun usage

void AGun::GiveAmmo(int AddAmount)
{
	const int32 MissingAmmo = FMath::Max(0, GunConfig.MaxAmmo - CurrentAmmo);
	AddAmount = FMath::Min(AddAmount, MissingAmmo);
	CurrentAmmo += AddAmount;

	/**
	AShooterAIController* BotAI = MyPawn ? Cast<AShooterAIController>(MyPawn->GetController()) : NULL;
	if (BotAI)
	{
		BotAI->CheckAmmo(this);
	}
	**/
	// start reload if clip was empty
	if (GetCurrentAmmoInClip() <= 0 &&
		CanReload() &&
		MyPawn && (MyPawn->GetGun() == this))
	{
		ClientStartReload();
	}
}

void AGun::UseAmmo()
{
	if (!HasInfiniteAmmo())
	{
		CurrentAmmoInClip--;
	}

	if (!HasInfiniteAmmo() && !HasInfiniteClip())
	{
		CurrentAmmo--;
	}

	
	AALSPlayerController* PlayerController = MyPawn ? Cast<AALSPlayerController>(MyPawn->GetController()) : NULL;
	
	 if (PlayerController)
	{
		//AAAADPlayerState* PlayerState = Cast<AAAADPlayerState>(PlayerController->PlayerState);
		switch (GetAmmoType())
		{
		case EAmmoType::ERocket:
			//PlayerState->AddRocketsFired(1);
			break;
		case EAmmoType::EBullet:
		default:
			//PlayerState->AddBulletsFired(1);
			break;
		}
	}
}

void AGun::HandleReFiring()
{
	// Update TimerIntervalAdjustment
	UWorld* MyWorld = GetWorld();

	float SlackTimeThisFrame = FMath::Max(0.0f, (MyWorld->TimeSeconds - LastFireTime) - GunConfig.TimeBetweenShots);

	if (bAllowAutomaticGunCatchup)
	{
		TimerIntervalAdjustment -= SlackTimeThisFrame;
	}

	HandleFiring();
}

void AGun::HandleFiring()
{
	if ((CurrentAmmoInClip > 0 || HasInfiniteClip() || HasInfiniteAmmo()) && CanFire())
	{
		if (GetNetMode() != NM_DedicatedServer)
		{
			SimulateGunFire();
		}

		if (MyPawn && MyPawn->IsLocallyControlled())
		{
			FireGun();

			UseAmmo();

			// update firing FX on remote clients if function was called on server
			BurstCounter++;
		}
	}
	else if (CanReload())
	{
		StartReload();
	}
	else if (MyPawn && MyPawn->IsLocallyControlled())
	{
		if (GetCurrentAmmo() == 0 && !bRefiring)
		{
			PlayGunSound(OutOfAmmoSound);
			//AALSPlayerController* MyPC = Cast<AALSPlayerController>(MyPawn->Controller);
			//AShooterHUD* MyHUD = MyPC ? Cast<AShooterHUD>(MyPC->GetHUD()) : NULL;
			//if (MyHUD)
			//{
			//	MyHUD->NotifyOutOfAmmo();
			//}
		}

		// stop Gun fire FX, but stay in Firing state
		if (BurstCounter > 0)
		{
			OnBurstFinished();
		}
	}
	else
	{
		OnBurstFinished();
	}

	if (MyPawn && MyPawn->IsLocallyControlled())
	{
		// local client will notify server
		if (GetLocalRole() < ROLE_Authority)
		{
			ServerHandleFiring();
		}

		// reload after firing last round
		if (CurrentAmmoInClip <= 0 && CanReload())
		{
			StartReload();
		}

		// setup refire timer
		bRefiring = (CurrentState == EGunState::Firing && GunConfig.TimeBetweenShots > 0.0f);
		if (bRefiring)
		{
			GetWorldTimerManager().SetTimer(TimerHandle_HandleFiring, this, &AGun::HandleReFiring, FMath::Max<float>(GunConfig.TimeBetweenShots + TimerIntervalAdjustment, SMALL_NUMBER), false);
			TimerIntervalAdjustment = 0.f;
		}
	}

	LastFireTime = GetWorld()->GetTimeSeconds();
}

bool AGun::ServerHandleFiring_Validate()
{
	return true;
}

void AGun::ServerHandleFiring_Implementation()
{
	const bool bShouldUpdateAmmo = (CurrentAmmoInClip > 0 && CanFire());

	HandleFiring();

	if (bShouldUpdateAmmo)
	{
		// update ammo
		UseAmmo();

		// update firing FX on remote clients
		BurstCounter++;
	}
}

void AGun::ReloadGun()
{
	int32 ClipDelta = FMath::Min(GunConfig.AmmoPerClip - CurrentAmmoInClip, CurrentAmmo - CurrentAmmoInClip);

	if (HasInfiniteClip())
	{
		ClipDelta = GunConfig.AmmoPerClip - CurrentAmmoInClip;
	}

	if (ClipDelta > 0)
	{
		CurrentAmmoInClip += ClipDelta;
	}

	if (HasInfiniteClip())
	{
		CurrentAmmo = FMath::Max(CurrentAmmoInClip, CurrentAmmo);
	}
}

void AGun::SetGunState(EGunState::Type NewState)
{
	const EGunState::Type PrevState = CurrentState;

	if (PrevState == EGunState::Firing && NewState != EGunState::Firing)
	{
		OnBurstFinished();
	}

	CurrentState = NewState;

	if (PrevState != EGunState::Firing && NewState == EGunState::Firing)
	{
		OnBurstStarted();
	}
}

void AGun::DetermineGunState()
{
	EGunState::Type NewState = EGunState::Idle;

	if (bIsEquipped)
	{
		if (bPendingReload)
		{
			if (CanReload() == false)
			{
				NewState = CurrentState;
			}
			else
			{
				NewState = EGunState::Reloading;
			}
		}
		else if ((bPendingReload == false) && (bWantsToFire == true) && (CanFire() == true))
		{
			NewState = EGunState::Firing;
		}
	}
	else if (bPendingEquip)
	{
		NewState = EGunState::Equipping;
	}

	SetGunState(NewState);
}

void AGun::OnBurstStarted()
{
	// start firing, can be delayed to satisfy TimeBetweenShots
	const float GameTime = GetWorld()->GetTimeSeconds();
	if (LastFireTime > 0 && GunConfig.TimeBetweenShots > 0.0f &&
		LastFireTime + GunConfig.TimeBetweenShots > GameTime)
	{
		GetWorldTimerManager().SetTimer(TimerHandle_HandleFiring, this, &AGun::HandleFiring, LastFireTime + GunConfig.TimeBetweenShots - GameTime, false);
	}
	else
	{
		HandleFiring();
	}
}

void AGun::OnBurstFinished()
{
	// stop firing FX on remote clients
	BurstCounter = 0;

	// stop firing FX locally, unless it's a dedicated server
	//if (GetNetMode() != NM_DedicatedServer)
	//{
	StopSimulatingGunFire();
	//}

	GetWorldTimerManager().ClearTimer(TimerHandle_HandleFiring);
	bRefiring = false;

	// reset firing interval adjustment
	TimerIntervalAdjustment = 0.0f;
}


//////////////////////////////////////////////////////////////////////////
// Gun usage helpers

UAudioComponent* AGun::PlayGunSound(USoundCue* Sound)
{
	UAudioComponent* AC = NULL;
	if (Sound && MyPawn)
	{
		AC = UGameplayStatics::SpawnSoundAttached(Sound, MyPawn->GetRootComponent());
		
	}

	return AC;
}

float AGun::PlayGunAnimation(const FGunAnim& Animation)
{
	float Duration = 0.0f;
	if (MyPawn)
	{
		//UAnimMontage* UseAnim = MyPawn->IsFirstPerson() ? Animation.Pawn1P : Animation.Pawn3P;
		//if (UseAnim)
		//{
		//	Duration = MyPawn->PlayAnimMontage(UseAnim);
		//}
	}

	return Duration;
}

void AGun::StopGunAnimation(const FGunAnim& Animation)
{
	if (MyPawn)
	{
		//UAnimMontage* UseAnim = MyPawn->IsFirstPerson() ? Animation.Pawn1P : Animation.Pawn3P;
	//	if (UseAnim)
		//{
		//	MyPawn->StopAnimMontage(UseAnim);
		//}
	}
}

FVector AGun::GetCameraAim() const
{
	AALSPlayerController* const PlayerController = GetInstigatorController<AALSPlayerController>();
	FVector FinalAim = FVector::ZeroVector;

	if (PlayerController)
	{
		FVector CamLoc;
		FRotator CamRot;
		PlayerController->GetPlayerViewPoint(CamLoc, CamRot);
		FinalAim = CamRot.Vector();
	}
	else if (GetInstigator())
	{
		FinalAim = GetInstigator()->GetBaseAimRotation().Vector();
	}

	return FinalAim;
}

FVector AGun::GetAdjustedAim() const
{
	AALSPlayerController* const PlayerController = GetInstigatorController<AALSPlayerController>();
	FVector FinalAim = FVector::ZeroVector;
	// If we have a player controller use it for the aim
	if (PlayerController)
	{
		FVector CamLoc;
		FRotator CamRot;
		PlayerController->GetPlayerViewPoint(CamLoc, CamRot);
		FinalAim = CamRot.Vector();
	}
	else if (GetInstigator())
	{
		//GetAim Here
	}

	return FinalAim;
}

FVector AGun::GetCameraDamageStartLocation(const FVector& AimDir) const
{
	AALSPlayerController* PC = MyPawn ? Cast<AALSPlayerController>(MyPawn->Controller) : NULL;
	
	FVector OutStartTrace = FVector::ZeroVector;

	if (PC)
	{
		// use player's camera
		FRotator UnusedRot;
		PC->GetPlayerViewPoint(OutStartTrace, UnusedRot);

		// Adjust trace so there is nothing blocking the ray between the camera and the pawn, and calculate distance from adjusted start
		OutStartTrace = OutStartTrace + AimDir * ((GetInstigator()->GetActorLocation() - OutStartTrace) | AimDir);
	}
	//else if (AIPC)
	//{
	//	OutStartTrace = GetMuzzleLocation();
	//}

	return OutStartTrace;
}

FVector AGun::GetMuzzleLocation() const
{
	USkeletalMeshComponent* UseMesh = GetGunMesh();
	return UseMesh->GetSocketLocation(MuzzleAttachPoint);
}

FVector AGun::GetMuzzleDirection() const
{
	USkeletalMeshComponent* UseMesh = GetGunMesh();
	return UseMesh->GetSocketRotation(MuzzleAttachPoint).Vector();
}

FHitResult AGun::GunTrace(const FVector& StartTrace, const FVector& EndTrace) const
{

	// Perform trace to retrieve hit info
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(GunTrace), true, GetInstigator());
	TraceParams.bReturnPhysicalMaterial = true;

	FHitResult Hit(ForceInit);
	GetWorld()->LineTraceSingleByChannel(Hit, StartTrace, EndTrace, ECC_Visibility, TraceParams);

	return Hit;
}

void AGun::SetOwningPawn(AALSBaseCharacter* NewOwner)
{
	if (MyPawn != NewOwner)
	{
		SetInstigator(NewOwner);
		MyPawn = NewOwner;
		// net owner for RPC calls
		SetOwner(NewOwner);
	}
}

//////////////////////////////////////////////////////////////////////////
// Replication & effects

void AGun::OnRep_MyPawn()
{
	if (MyPawn)
	{
		OnEnterInventory(MyPawn);
	}
	else
	{
		OnLeaveInventory();
	}
}

void AGun::OnRep_BurstCounter()
{
	if (BurstCounter > 0)
	{
		SimulateGunFire();
	}
	else
	{
		StopSimulatingGunFire();
	}
}

void AGun::OnRep_Reload()
{
	if (bPendingReload)
	{
		StartReload(true);
	}
	else
	{
		StopReload();
	}
}

void AGun::SimulateGunFire()
{
	if (GetLocalRole() == ROLE_Authority && CurrentState != EGunState::Firing)
	{
		return;
	}

	if (MuzzleFX)
	{
		USkeletalMeshComponent* UseGunMesh = GetGunMesh();
		if (!bLoopedMuzzleFX || MuzzlePSC == NULL)
		{
			// Split screen requires we create 2 effects. One that we see and one that the other player sees.
			if ((MyPawn != NULL) && (MyPawn->IsLocallyControlled() == true))
			{
				AController* PlayerCon = MyPawn->GetController();
				if (PlayerCon != NULL)
				{
					Mesh1P->GetSocketLocation(MuzzleAttachPoint);
					MuzzlePSC = UGameplayStatics::SpawnEmitterAttached(MuzzleFX, Mesh1P, MuzzleAttachPoint);
					MuzzlePSC->bOwnerNoSee = false;
					MuzzlePSC->bOnlyOwnerSee = true;

					Mesh3P->GetSocketLocation(MuzzleAttachPoint);
					MuzzlePSCSecondary = UGameplayStatics::SpawnEmitterAttached(MuzzleFX, Mesh3P, MuzzleAttachPoint);
					MuzzlePSCSecondary->bOwnerNoSee = true;
					MuzzlePSCSecondary->bOnlyOwnerSee = false;
				}
			}
			else
			{
				MuzzlePSC = UGameplayStatics::SpawnEmitterAttached(MuzzleFX, UseGunMesh, MuzzleAttachPoint);
			}
		}
	}

	if (!bLoopedFireAnim || !bPlayingFireAnim)
	{
		PlayGunAnimation(FireAnim);
		bPlayingFireAnim = true;
	}

	if (bLoopedFireSound)
	{
		if (FireAC == NULL)
		{
			FireAC = PlayGunSound(FireLoopSound);
		}
	}
	else
	{
		PlayGunSound(FireSound);
	}

	/**
	AALSPlayerController* PC = (MyPawn != NULL) ? Cast<AALSPlayerController>(MyPawn->Controller) : NULL;
	if (PC != NULL && PC->IsLocalController())
	{
		if (FireCameraShake != NULL)
		{
			PC->ClientPlayCameraShake(FireCameraShake, 1);
		}
		if (FireForceFeedback != NULL && PC->IsVibrationEnabled())
		{
			FForceFeedbackParameters FFParams;
			FFParams.Tag = "Gun";
			PC->ClientPlayForceFeedback(FireForceFeedback, FFParams);
		}
	}
	**/
}

void AGun::StopSimulatingGunFire()
{
	if (bLoopedMuzzleFX)
	{
		if (MuzzlePSC != NULL)
		{
			MuzzlePSC->DeactivateSystem();
			MuzzlePSC = NULL;
		}
		if (MuzzlePSCSecondary != NULL)
		{
			MuzzlePSCSecondary->DeactivateSystem();
			MuzzlePSCSecondary = NULL;
		}
	}

	if (bLoopedFireAnim && bPlayingFireAnim)
	{
		StopGunAnimation(FireAnim);
		bPlayingFireAnim = false;
	}

	if (FireAC)
	{
		FireAC->FadeOut(0.1f, 0.0f);
		FireAC = NULL;

		PlayGunSound(FireFinishSound);
	}
}

void AGun::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGun, MyPawn);

	DOREPLIFETIME_CONDITION(AGun, CurrentAmmo, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AGun, CurrentAmmoInClip, COND_OwnerOnly);

	DOREPLIFETIME_CONDITION(AGun, BurstCounter, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AGun, bPendingReload, COND_SkipOwner);
}

USkeletalMeshComponent* AGun::GetGunMesh() const
{
	//return (MyPawn != NULL && MyPawn->IsFirstPerson()) ? Mesh1P : Mesh3P;
	return Mesh1P;
}

class AALSBaseCharacter* AGun::GetPawnOwner() const
{
	return MyPawn;
}

bool AGun::IsEquipped() const
{
	return bIsEquipped;
}

bool AGun::IsAttachedToPawn() const
{
	return bIsEquipped || bPendingEquip;
}

EGunState::Type AGun::GetCurrentState() const
{
	return CurrentState;
}

int32 AGun::GetCurrentAmmo() const
{
	return CurrentAmmo;
}

int32 AGun::GetCurrentAmmoInClip() const
{
	return CurrentAmmoInClip;
}

int32 AGun::GetAmmoPerClip() const
{
	return GunConfig.AmmoPerClip;
}

int32 AGun::GetMaxAmmo() const
{
	return GunConfig.MaxAmmo;
}

bool AGun::HasInfiniteAmmo() const
{
	//const AALSPlayerController* MyPC = (MyPawn != NULL) ? Cast<const AALSPlayerController>(MyPawn->Controller) : NULL;
	//return GunConfig.bInfiniteAmmo || (MyPC && MyPC->HasInfiniteAmmo());
	return false;
}

bool AGun::HasInfiniteClip() const
{
	//const AALSPlayerController* MyPC = (MyPawn != NULL) ? Cast<const AALSPlayerController>(MyPawn->Controller) : NULL;
	//return GunConfig.bInfiniteClip || (MyPC && MyPC->HasInfiniteClip());
	return false;
}

float AGun::GetEquipStartedTime() const
{
	return EquipStartedTime;
}

float AGun::GetEquipDuration() const
{
	return EquipDuration;
}
