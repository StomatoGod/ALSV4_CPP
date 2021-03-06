// Project:         Advanced Locomotion System V4 on C++
// Copyright:       Copyright (C) 2020 Doğa Can Yanıkoğlu
// License:         MIT License (http://www.opensource.org/licenses/mit-license.php)
// Source Code:     https://github.com/dyanikoglu/ALSV4_CPP
// Original Author: Doğa Can Yanıkoğlu
// Contributors:    Haziq Fadhil


#include "Character/ALSBaseCharacter.h"

#include "Character/Weapon.h"
#include "Character/SingleShotTestGun.h"
#include "Character/Weap_VoodooGun.h"
#include "Character/ALSPlayerController.h"
#include "Character/Animation/ALSCharacterAnimInstance.h"
#include "Library/ALSMathLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Components/TimelineComponent.h"
#include "Camera/CameraComponent.h"
#include "Curves/CurveVector.h"
#include "Curves/CurveFloat.h"
#include "Character/ALSCharacterMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "DependencyFix/Public/PhysicsItem.h"
#include "DependencyFix/Public/Library/ItemEnumLibrary.h"
#include "Character/GravHud.h"
#include "DependencyFix/Public/UI/AAADHUD.h"
#include "DependencyFix/Public/RoomDataHelper.h"
#include "DrawDebugHelpers.h"


FOnEquipWeapon AALSBaseCharacter::NotifyEquipWeapon;
FOnUnEquipWeapon AALSBaseCharacter::NotifyUnEquipWeapon;



AALSBaseCharacter::AALSBaseCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UALSCharacterMovementComponent>(CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	MantleTimeline = CreateDefaultSubobject<UTimelineComponent>(FName(TEXT("MantleTimeline")));
	bUseControllerRotationYaw = 0;
	bReplicates = true;
	SetReplicatingMovement(true);

	CameraPoll = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Slerper"));
	CapsuleComponent = GetCapsuleComponent();
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(CameraPoll);
	//FirstPersonCameraComponent->SetRelativeLocation(FVector(-39.56f, 1.75f, 64.f)); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = false;
	
	Health = 100.f;
	bAlwaysRelevant = true;
}

void AALSBaseCharacter::UpdateCameraRotation(FRotator& RotationOffset, FRotator& OldOffsetAndOutControlRot, float DeltaTime)
{
	FVector PreCameraForward = FirstPersonCameraComponent->GetForwardVector();
	
	FVector CapsuleUp = CapsuleComponent->GetUpVector();
	FVector OldPollUp = CapsuleComponent->GetUpVector(); 
	CameraPoll->SetWorldLocation(CapsuleComponent->GetComponentLocation());
	FRotator DeltaRotation = RotationOffset - OldOffsetAndOutControlRot;
	float NewCameraPitch = RotationOffset.Pitch;
	FQuat DeltaQuatYaw = FRotator(0.f, DeltaRotation.Yaw, 0.f).Quaternion();

	if ((CapsuleUp | CameraPoll->GetUpVector()) >= THRESH_NORMALS_ARE_PARALLEL)
	{
		FirstPersonCameraComponent->SetRelativeRotation(FRotator(NewCameraPitch, 0.f, 0.f));
		CameraPoll->AddLocalRotation(DeltaQuatYaw);
	}
	else if (IsTargeting() )
	{
		const FMatrix RotationMatrix = FRotationMatrix::MakeFromZX(CapsuleUp, CameraPoll->GetForwardVector());
		CameraPoll->SetWorldRotation(FQuat::Slerp(CameraPoll->GetComponentRotation().Quaternion(), RotationMatrix.Rotator().Quaternion(), RotationLerpRate * DeltaTime));

		//compensate for changes in camera aim direction when traversing different gravity directions
		//correct the yaw orientation before sampling the change in pitch. This will allow the angle measurement between the two camera 
		// unit vectors to be be purely constructed of pitch difference.
		const FMatrix RotationMatrixYawCorrection = FRotationMatrix::MakeFromZX(CameraPoll->GetUpVector(), PreCameraForward);
		CameraPoll->SetWorldRotation(RotationMatrixYawCorrection.Rotator());

		//now we sample and correct pitch
		FVector AfterCameraForward = FirstPersonCameraComponent->GetForwardVector();
		float Dot = AfterCameraForward | PreCameraForward;
		FVector CharacterUp = GetActorUpVector();
		float DeltaQuatAcos = FMath::Acos(Dot);
		float DeltaAngle = DeltaQuatAcos * 57.2958 * -1.f;

		float OldPollDot = OldPollUp | PreCameraForward;
		float NewPollDot = OldPollUp | AfterCameraForward;
		if (NewPollDot < OldPollDot)
		{
			DeltaAngle *= -1.f;
		}

		//we save the pitch difference back into RotationOffset and then recieve the change next tick. 
		//this is to avoid exceeding pitch limits or having to enforce them twice (they are already enforced by PlayerCameraManager->ProcessViewRotation
		//on RotationOffset 
		RotationOffset.Pitch += DeltaAngle;

		// add the offset last to ensure gravity changes are the only reason for aim being thrown off
		
		
	}
	else
	{
		const FMatrix RotationMatrix = FRotationMatrix::MakeFromZX(CapsuleUp, CameraPoll->GetForwardVector());
		CameraPoll->SetWorldRotation(FQuat::Slerp(CameraPoll->GetComponentRotation().Quaternion(), RotationMatrix.Rotator().Quaternion(), RotationLerpRate * DeltaTime));
	}
	FirstPersonCameraComponent->SetRelativeRotation(FRotator(NewCameraPitch, 0.f, 0.f));
	CameraPoll->AddLocalRotation(DeltaQuatYaw);
	OldOffsetAndOutControlRot = FirstPersonCameraComponent->GetComponentRotation();
}


void AALSBaseCharacter::Gravitate(FVector SourceLocation, FVector HitLocation, float Direction, float Strength)
{
	FVector thisLocation = GetActorLocation();
	float Distance = FMath::Abs((thisLocation - SourceLocation).Size());
	float ForceScale = FMath::GetMappedRangeValueClamped({ 0.0f, 3000.0f }, { 1.0f, 0.f }, Distance);
	FVector DirectionTo = UKismetMathLibrary::GetDirectionUnitVector(HitLocation, SourceLocation);
	FVector Force = Strength * ForceScale * Direction * DirectionTo;
	GetMyMovementComponent()->AddForce(Force * .05f);
	UE_LOG(LogTemp, Log, TEXT(" AALSBaseCharacter::Gravitate"));
}

	///Weapons Weapons etc
AWeapon* AALSBaseCharacter::GetWeapon()
{
	if (CurrentWeapon != nullptr)
	{
	 return CurrentWeapon;
	}
	return nullptr;
}


void AALSBaseCharacter::SetCurrentWeapon(AWeapon* NewWeapon, AWeapon* LastWeapon)
{
	AWeapon* LocalLastWeapon = nullptr;

	if (LastWeapon != NULL)
	{
		LocalLastWeapon = LastWeapon;
	}
	else if (NewWeapon != CurrentWeapon)
	{
		LocalLastWeapon = CurrentWeapon;
	}

	// unequip previous
	if (LocalLastWeapon)
	{
		//LocalLastWeapon->OnUnEquip();
	}

	CurrentWeapon = NewWeapon;

	if (CurrentWeapon == NewWeapon)
	{
		UE_LOG(LogClass, Warning, TEXT("New WEapon Equip! "));
	}

	if (CurrentWeapon->IsA<ASingleShotTestGun>())
	{
		MainAnimInstance->OverlayState = EALSOverlayState::Rifle;
		
	}
	// equip new one
	if (NewWeapon)
	{
		NewWeapon->SetOwningPawn(this);	// Make sure Weapon's MyPawn is pointing back to us. During replication, we can't guarantee APawn::CurrentWeapon will rep after AWeapon::MyPawn!

		NewWeapon->OnEquip(LastWeapon);
	}
}

bool AALSBaseCharacter::CanFire()
{
	if (MovementState != EALSMovementState::Ragdoll)
	{
		return true;
	}

	return false;
}

void AALSBaseCharacter::Restart()
{
	Super::Restart();

	AALSPlayerController* NewController = Cast<AALSPlayerController>(GetController());
	if (NewController)
	{
		NewController->OnRestartPawn(this);
	}
}

void AALSBaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward/Backwards", this, &AALSBaseCharacter::PlayerForwardMovementInput);
	PlayerInputComponent->BindAxis("MoveRight/Left", this, &AALSBaseCharacter::PlayerRightMovementInput);
	PlayerInputComponent->BindAxis("LookUp/Down", this, &AALSBaseCharacter::PlayerCameraUpInput);
	PlayerInputComponent->BindAxis("LookLeft/Right", this, &AALSBaseCharacter::PlayerCameraRightInput);
	PlayerInputComponent->BindAction("JumpAction", IE_Pressed, this, &AALSBaseCharacter::JumpPressedAction);
	PlayerInputComponent->BindAction("JumpAction", IE_Released, this, &AALSBaseCharacter::JumpReleasedAction);
	PlayerInputComponent->BindAction("StanceAction", IE_Pressed, this, &AALSBaseCharacter::StancePressedAction);
	PlayerInputComponent->BindAction("WalkAction", IE_Pressed, this, &AALSBaseCharacter::WalkPressedAction);
	PlayerInputComponent->BindAction("RagdollAction", IE_Pressed, this, &AALSBaseCharacter::RagdollPressedAction);
	PlayerInputComponent->BindAction("SelectRotationMode_1", IE_Pressed, this,
	                                 &AALSBaseCharacter::VelocityDirectionPressedAction);
	PlayerInputComponent->BindAction("SelectRotationMode_2", IE_Pressed, this,
	                                 &AALSBaseCharacter::LookingDirectionPressedAction);
	PlayerInputComponent->BindAction("SprintAction", IE_Pressed, this, &AALSBaseCharacter::SprintPressedAction);
	PlayerInputComponent->BindAction("SprintAction", IE_Released, this, &AALSBaseCharacter::SprintReleasedAction);
	PlayerInputComponent->BindAction("AimAction", IE_Pressed, this, &AALSBaseCharacter::AimPressedAction);
	PlayerInputComponent->BindAction("AimAction", IE_Released, this, &AALSBaseCharacter::AimReleasedAction);
	PlayerInputComponent->BindAction("CameraAction", IE_Pressed, this, &AALSBaseCharacter::CameraPressedAction);
	PlayerInputComponent->BindAction("CameraAction", IE_Released, this, &AALSBaseCharacter::CameraReleasedAction);
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &AALSBaseCharacter::OnFire);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &AALSBaseCharacter::OnStopFire);
	PlayerInputComponent->BindAction("GravityTestAction", IE_Pressed, this, &AALSBaseCharacter::ZeroGravTest);
	PlayerInputComponent->BindAction("UseAction", IE_Pressed, this, &AALSBaseCharacter::UsePressedAction);
	PlayerInputComponent->BindAction("Reload", IE_Pressed, this, &AALSBaseCharacter::ReloadPressedAction);
	PlayerInputComponent->BindAction("TestAction", IE_Pressed, this, &AALSBaseCharacter::TestActionRep);
	PlayerInputComponent->BindAction("EscMenu", IE_Pressed, this, &AALSBaseCharacter::EscMenuPressedAction);
	//test
	
}

void AALSBaseCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (GetLocalRole() == ROLE_Authority)
	{
		Health = GetMaxHealth();

		// Needs to happen after character is added to repgraph
		//GetWorldTimerManager().SetTimerForNextTick(this, &AALSBaseCharacter::SpawnDefaultInventory);
	}

	MyCharacterMovementComponent = Cast<UALSCharacterMovementComponent>(Super::GetMovementComponent());
}
void AALSBaseCharacter::OnRep_LastTakeHitInfo()
{
	if (LastTakeHitInfo.bKilled)
	{
		OnDeath(LastTakeHitInfo.ActualDamage, LastTakeHitInfo.GetDamageEvent(), LastTakeHitInfo.PawnInstigator.Get(), LastTakeHitInfo.DamageCauser.Get());
	}
	else
	{
		PlayHit(LastTakeHitInfo.ActualDamage, LastTakeHitInfo.GetDamageEvent(), LastTakeHitInfo.PawnInstigator.Get(), LastTakeHitInfo.DamageCauser.Get());
	}
}

void AALSBaseCharacter::ReplicateHit(float Damage, struct FDamageEvent const& DamageEvent, class APawn* PawnInstigator, class AActor* DamageCauser, bool bKilled)
{
	const float TimeoutTime = GetWorld()->GetTimeSeconds() + 0.5f;

	FDamageEvent const& LastDamageEvent = LastTakeHitInfo.GetDamageEvent();
	if ((PawnInstigator == LastTakeHitInfo.PawnInstigator.Get()) && (LastDamageEvent.DamageTypeClass == LastTakeHitInfo.DamageTypeClass) && (LastTakeHitTimeTimeout == TimeoutTime))
	{
		// same frame damage
		if (bKilled && LastTakeHitInfo.bKilled)
		{
			// Redundant death take hit, just ignore it
			return;
		}

		// otherwise, accumulate damage done this frame
		Damage += LastTakeHitInfo.ActualDamage;
	}

	LastTakeHitInfo.ActualDamage = Damage;
	LastTakeHitInfo.PawnInstigator = Cast<AALSBaseCharacter>(PawnInstigator);
	LastTakeHitInfo.DamageCauser = DamageCauser;
	LastTakeHitInfo.SetDamageEvent(DamageEvent);
	LastTakeHitInfo.bKilled = bKilled;
	LastTakeHitInfo.EnsureReplication();

	LastTakeHitTimeTimeout = TimeoutTime;
}
void AALSBaseCharacter::ReloadPressedAction()
{
	//AShooterPlayerController* MyPC = Cast<AShooterPlayerController>(Controller);
	//if (MyPC && MyPC->IsGameInputAllowed())
//	{
		if (CurrentWeapon)
		{
			CurrentWeapon->StartReload();
		}
	//}
}
void AALSBaseCharacter::OnDeath(float KillingDamage, struct FDamageEvent const& DamageEvent, class APawn* PawnInstigator, class AActor* DamageCauser)
{
	if (bIsDying)
	{
		return;
	}

	//SetReplicatingMovement(false);
	//TearOff();
	bIsDying = true;

	if (GetLocalRole() == ROLE_Authority)
	{
		ReplicateHit(KillingDamage, DamageEvent, PawnInstigator, DamageCauser, true);

		// play the force feedback effect on the client player controller
		AALSPlayerController* PC = Cast<AALSPlayerController>(Controller);
		if (PC && DamageEvent.DamageTypeClass)
		{
			//UShooterDamageType* DamageType = Cast<UShooterDamageType>(DamageEvent.DamageTypeClass->GetDefaultObject());
			//if (DamageType && DamageType->KilledForceFeedback && PC->IsVibrationEnabled())
			//{
			//	FForceFeedbackParameters FFParams;
			//	FFParams.Tag = "Damage";
			//	PC->ClientPlayForceFeedback(DamageType->KilledForceFeedback, FFParams);
			//}
		}
	}

	// cannot use IsLocallyControlled here, because even local client's controller may be NULL here
	//if (GetNetMode() != NM_DedicatedServer && DeathSound && Mesh1P && Mesh1P->IsVisible())
	//{
	//	UGameplayStatics::PlaySoundAtLocation(this, DeathSound, GetActorLocation());
	//}

	// remove all weapons
	//DestroyInventory();

	// switch back to 3rd person view
	//UpdatePawnMeshes();

	//DetachFromControllerPendingDestroy();
//	StopAllAnimMontages();

	//if (LowHealthWarningPlayer && LowHealthWarningPlayer->IsPlaying())
	//{
	//	LowHealthWarningPlayer->Stop();
	//}

	//if (RunLoopAC)
	//{
	//	RunLoopAC->Stop();
	//}
/**
	if (GetMesh())
	{
		static FName CollisionProfileName(TEXT("Ragdoll"));
		GetMesh()->SetCollisionProfileName(CollisionProfileName);
	}
	SetActorEnableCollision(true);
	**/
	// Death anim
	//float DeathAnimDuration = PlayAnimMontage(DeathAnim);

	// Ragdoll
	CurrentWeapon->OnCharacterDeath();
	ReplicatedRagdollStart();
	
	
}

bool AALSBaseCharacter::Die(float KillingDamage, FDamageEvent const& DamageEvent, AController* Killer, AActor* DamageCauser)
{
	if (!CanDie(KillingDamage, DamageEvent, Killer, DamageCauser))
	{
		return false;
	}

	Health = FMath::Min(0.0f, Health);

	// if this is an environmental death then refer to the previous killer so that they receive credit (knocked into lava pits, etc)
	UDamageType const* const DamageType = DamageEvent.DamageTypeClass ? DamageEvent.DamageTypeClass->GetDefaultObject<UDamageType>() : GetDefault<UDamageType>();
	Killer = GetDamageInstigator(Killer, *DamageType);

	AController* const KilledPlayer = (Controller != NULL) ? Controller : Cast<AController>(GetOwner());
	//GetWorld()->GetAuthGameMode<AShooterGameMode>()->Killed(Killer, KilledPlayer, this, DamageType);

	//NetUpdateFrequency = GetDefault<AShooterCharacter>()->NetUpdateFrequency;
	GetCharacterMovement()->ForceReplicationUpdate();

	OnDeath(KillingDamage, DamageEvent, Killer ? Killer->GetPawn() : NULL, DamageCauser);
	return true;
}

bool AALSBaseCharacter::CanDie(float KillingDamage, FDamageEvent const& DamageEvent, AController* Killer, AActor* DamageCauser) const
{
	if (bIsDying										// already dying
		|| IsPendingKill()								// already destroyed
		|| GetLocalRole() != ROLE_Authority				// not authority
		//|| GetWorld()->GetAuthGameMode<AShooterGameMode>() == NULL
		//|| GetWorld()->GetAuthGameMode<AShooterGameMode>()->GetMatchState() == MatchState::LeavingMap)
		)// level transition occurring
	{
		return false;
	}

	return true;
}


float AALSBaseCharacter::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, class AActor* DamageCauser)
{
	UE_LOG(LogTemp, Error, TEXT("BaseCharacter TakeDamage %f"), Damage);
	UE_LOG(LogTemp, Error, TEXT("BaseCharacter Health %f"), Health);
	AALSPlayerController* PC = Cast<AALSPlayerController>(Controller);
	if (PC && PC->HasGodMode())
	{
		UE_LOG(LogTemp, Log, TEXT("HasGodMode no damage"));
		return 0.f;
	}

	if (Health <= 0.f)
	{
		return 0.f;
	}

	// Modify based on game rules.
	//AShooterGameMode* const Game = GetWorld()->GetAuthGameMode<AShooterGameMode>();
	//Damage = Game ? Game->ModifyDamage(Damage, this, DamageEvent, EventInstigator, DamageCauser) : 0.f;

	const float ActualDamage = Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);
	if (ActualDamage > 0.f)
	{
		UE_LOG(LogTemp, Error, TEXT("BaseCharacter ActualDamage > 0.f"));
		Health -= ActualDamage;
		UE_LOG(LogTemp, Log, TEXT("Health: %f"), Health);
		if (Health <= 0)
		{
			Die(ActualDamage, DamageEvent, EventInstigator, DamageCauser);
		}
		else
		{
			PlayHit(ActualDamage, DamageEvent, EventInstigator ? EventInstigator->GetPawn() : NULL, DamageCauser);
		}

		MakeNoise(1.0f, EventInstigator ? EventInstigator->GetPawn() : this);
	}

	return ActualDamage;
}

void AALSBaseCharacter::PlayHit(float DamageTaken, struct FDamageEvent const& DamageEvent, class APawn* PawnInstigator, class AActor* DamageCauser)
{
	if (GetLocalRole() == ROLE_Authority)
	{
		ReplicateHit(DamageTaken, DamageEvent, PawnInstigator, DamageCauser, false);

		// play the force feedback effect on the client player controller
		AALSPlayerController* PC = Cast<AALSPlayerController>(Controller);
		if (PC && DamageEvent.DamageTypeClass)
		{
			//UShooterDamageType* DamageType = Cast<UShooterDamageType>(DamageEvent.DamageTypeClass->GetDefaultObject());
			//if (DamageType && DamageType->HitForceFeedback && PC->IsVibrationEnabled())
			//{
			//	FForceFeedbackParameters FFParams;
			//	FFParams.Tag = "Damage";
			//	PC->ClientPlayForceFeedback(DamageType->HitForceFeedback, FFParams);
			//}
		}
	}

	if (DamageTaken > 0.f)
	{
		ApplyDamageMomentum(DamageTaken, DamageEvent, PawnInstigator, DamageCauser);
	}

	AALSPlayerController* MyPC = Cast<AALSPlayerController>(Controller);
//	AShooterHUD* MyHUD = MyPC ? Cast<AShooterHUD>(MyPC->GetHUD()) : NULL;
	//if (MyHUD)
	//{
	//	MyHUD->NotifyWeaponHit(DamageTaken, DamageEvent, PawnInstigator);
	//}

	if (PawnInstigator && PawnInstigator != this && PawnInstigator->IsLocallyControlled())
	{
		AALSPlayerController* InstigatorPC = Cast<AALSPlayerController>(PawnInstigator->Controller);
		AGravHud* InstigatorHUD = InstigatorPC ? Cast<AGravHud>(InstigatorPC->GetHUD()) : NULL;
		if (InstigatorHUD)
		{
			//InstigatorHUD->NotifyEnemyHit();
		}
	}
}

void AALSBaseCharacter::SetPredictedRoomID(int32 NewRoomID)
{
PredictedNextRoomID = NewRoomID;
}

void AALSBaseCharacter::SetCurrentRoomID(int32 NewRoomID)
{
CurrentRoomID = NewRoomID;
}

void AALSBaseCharacter::SetCurrentRoomIDToPredicted()
{
	CurrentRoomID = GridSample.PredictedNextRoomID;
}





int32 AALSBaseCharacter::GetCurrentRoomID()
{
	return CurrentRoomID;
}
int32 AALSBaseCharacter::GetPredictedNextRoomID()
{

return GridSample.PredictedNextRoomID;
}
void AALSBaseCharacter::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);

	// Only replicate this property for a short duration after it changes so join in progress players don't get spammed with fx when joining late
	DOREPLIFETIME_ACTIVE_OVERRIDE(AALSBaseCharacter, LastTakeHitInfo, GetWorld() && GetWorld()->GetTimeSeconds() < LastTakeHitTimeTimeout);
}


void AALSBaseCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AALSBaseCharacter, PlayerID);
	DOREPLIFETIME(AALSBaseCharacter, Health);
	DOREPLIFETIME(AALSBaseCharacter, CurrentWeapon);
	DOREPLIFETIME(AALSBaseCharacter, TargetRagdollLocation);

	DOREPLIFETIME_CONDITION(AALSBaseCharacter, Arsenal, COND_OwnerOnly);

	DOREPLIFETIME_CONDITION(AALSBaseCharacter, ReplicatedQuatYawRotation, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AALSBaseCharacter, CameraRotation, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AALSBaseCharacter, DeltaPitch, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AALSBaseCharacter, DeltaYaw, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AALSBaseCharacter, ReplicatedCurrentAcceleration, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AALSBaseCharacter, ReplicatedControlRotation, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AALSBaseCharacter, ReplicatedGravityDirection, COND_SkipOwner);
	

	DOREPLIFETIME(AALSBaseCharacter, DesiredGait);
	DOREPLIFETIME_CONDITION(AALSBaseCharacter, DesiredStance, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AALSBaseCharacter, DesiredRotationMode, COND_SkipOwner);

	DOREPLIFETIME_CONDITION(AALSBaseCharacter, RotationMode, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AALSBaseCharacter, OverlayState, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AALSBaseCharacter, ViewMode, COND_SkipOwner);
}

int32 AALSBaseCharacter::GetMaxHealth() const
{
	return GetClass()->GetDefaultObject<AALSBaseCharacter>()->Health;
}


void AALSBaseCharacter::OnBreakfall_Implementation()
{
	Replicated_PlayMontage(GetRollAnimation(), 1.35);
}

void AALSBaseCharacter::Replicated_PlayMontage_Implementation(UAnimMontage* montage, float track)
{
	// Roll: Simply play a Root Motion Montage.
	MainAnimInstance->Montage_Play(montage, track);
	Server_PlayMontage(montage, track);
}

void AALSBaseCharacter::ClientCalculateFlow_Implementation(ARoomDataHelper* DataHelper, const TArray<FFloatBool>& AirCurrentData) const
{

	//UE_LOG(LogTemp, Error, TEXT("ClientCalculateFlow_Implementation on %s "), *GetFName().ToString());
	DataHelper->CaclulateFlowBlendAsync(AirCurrentData);
	//DataHelper->ReturnAsyncCompressedForces.BindUObject(this, &AALSBaseCharacter::ServerUpdateFlow); //see above in wiki
	//DataHelper->AsyncCompressedForcesDelegate.AddDynamic(this, &AALSBaseCharacter::ServerUpdateFlow);
	
}


void AALSBaseCharacter::ServerUpdateFlow_Implementation(const FCompressedForceArray& OutCompressedForceArray, ARoomDataHelper* BroadcastingDataHelper)
{
	//const FVector2D Scale = FVector2D(10.f, 10.f);
	//GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::White, TEXT("Bigger???"), true, Scale);
	GEngine->AddOnScreenDebugMessage(-10, 20.f, FColor::Green, FString::Printf(TEXT("ServerUpdateFlow_Implementation CALLED!!!!!!!")));
	UE_LOG(LogTemp, Log, TEXT("ServerUpdateFlow_Implementation CALLED!!!!!!!"));
	BroadcastingDataHelper->CompressedForceArray = OutCompressedForceArray;
}

bool AALSBaseCharacter::ServerUpdateFlow_Validate(const FCompressedForceArray& OutCompressedForceArray, ARoomDataHelper* BroadcastingDataHelper)
{
	return true;
}

void AALSBaseCharacter::BeginPlay()
{
	Super::BeginPlay();

	FPostProcessSettings VariableName;
	VariableName.bOverride_MotionBlurAmount = true;
	FirstPersonCameraComponent->PostProcessSettings = VariableName;

	StunTimer = MaxStunTimerValue;
	// If we're in networked game, disable curved movement
	bDisableCurvedMovement = !IsNetMode(ENetMode::NM_Standalone);

	FOnTimelineFloat TimelineUpdated;
	FOnTimelineEvent TimelineFinished;
	TimelineUpdated.BindUFunction(this, FName(TEXT("MantleUpdate")));
	TimelineFinished.BindUFunction(this, FName(TEXT("MantleEnd")));
	MantleTimeline->SetTimelineFinishedFunc(TimelineFinished);
	MantleTimeline->SetLooping(false);
	MantleTimeline->SetTimelineLengthMode(TL_TimelineLength);
	MantleTimeline->AddInterpFloat(MantleTimelineCurve, TimelineUpdated);

	// Make sure the mesh and animbp update after the CharacterBP to ensure it gets the most recent values.
	GetMesh()->AddTickPrerequisiteActor(this);

	// Set the Movement Model
	SetMovementModel();

	// Once, force set variables in anim bp. This ensures anim instance & character starts synchronized
	FALSAnimCharacterInformation& AnimData = MainAnimInstance->GetCharacterInformationMutable();
	MainAnimInstance->Gait = DesiredGait;
	MainAnimInstance->Stance = DesiredStance;
	MainAnimInstance->RotationMode = DesiredRotationMode;
	AnimData.ViewMode = ViewMode;
	MainAnimInstance->OverlayState = OverlayState;
	AnimData.PrevMovementState = PrevMovementState;
	MainAnimInstance->MovementState = MovementState;

	// Update states to use the initial desired values.
	SetGait(DesiredGait);
	SetStance(DesiredStance);
	SetRotationMode(DesiredRotationMode);
	SetViewMode(ViewMode);
	SetOverlayState(OverlayState);

	if (Stance == EALSStance::Standing)
	{
		UnCrouch();
	}
	else if (Stance == EALSStance::Crouching)
	{
		Crouch();
	}

	// Set default rotation values.
	TargetRotation = GetActorRotation();
	LastVelocityRotation = TargetRotation;
	LastVelocityDirection = GetActorForwardVector();
	LastMovementInputRotation = TargetRotation;
	

	
	CameraPoll->SetWorldLocation(CapsuleComponent->GetComponentLocation());
	CameraPoll->SetWorldRotation(CapsuleComponent->GetComponentRotation().Quaternion());

	RotationMode = EALSRotationMode::LookingDirection;

	GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	
	//SpawnWeapon(EWeaponType::SingleShotTestGun);
	
}
void AALSBaseCharacter::PassGasToHud()
{
	
	if (IsLocallyControlled())
	{
		AALSPlayerController* PC = Cast<AALSPlayerController>(Controller);
		AAAADHUD* HUD = PC->GetHUD<AAAADHUD>();
		HUD->CurrentRoomAirPressure = &ClientCurrentRoomPressurePointer;
		//HUD->GasSamplePointer = &
		UE_LOG(LogClass, Error, TEXT("PassPressureToHud"));
	}
}
void AALSBaseCharacter::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	MainAnimInstance = Cast<UALSCharacterAnimInstance>(GetMesh()->GetAnimInstance());
	if (!MainAnimInstance)
	{
		// Animation instance should be assigned if we're not in editor preview
		checkf(GetWorld()->WorldType == EWorldType::EditorPreview,
		       TEXT("%s doesn't have a valid animation instance assigned. That's not allowed"),
		       *GetName());
	}
}

void AALSBaseCharacter::SetAimYawRate(float NewAimYawRate)
{
	AimYawRate = NewAimYawRate;
	MainAnimInstance->GetCharacterInformationMutable().AimYawRate = AimYawRate;
}

void AALSBaseCharacter::SetGravityDirection(FVector Direction)
{
		MyCharacterMovementComponent->SetGravityDirection(Direction);
		GravityDirection = Direction;
}

void AALSBaseCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	
	

	if (HasAuthority())
	{
		AALSPlayerController* OwnerController = Cast<AALSPlayerController>(this);
		if (OwnerController != NULL)
		{
			
			//APlayerState* PlayerState = OwnerController->PlayerState;
			//PlayerID = PlayerState->GetPlayerId();
		}

	}
	//UE_LOG(LogClass, Warning, TEXT(" GravityDirection: %s"), *GravityDirection.ToString());
	//DrawDebugLine(this->GetWorld(), GetActorLocation(), GetActorLocation() + GravityDirection * -500.f, FColor::Green, false, .01f, 0, 10.f);
	/**
	if (CurrentWeapon != nullptr)
	{
		if (CurrentWeapon->CorrespondingPhysicsItem != nullptr)
		{
			CurrentWeapon->OnCharacterDeath();
			FVector ItemLocation = CurrentWeapon->CorrespondingPhysicsItem->GetActorLocation();
			DrawDebugSphere
			(
				this->GetWorld(),
				ItemLocation,
				10.f,
				5,
				FColor::Yellow,
				false,
				.02f,
				0,
				20.f
			);
			DrawDebugLine(this->GetWorld(), GetActorLocation(), ItemLocation, FColor::Green, false, .01f, 0, 10.f);
		}
		else
		{
			UE_LOG(LogClass, Error, TEXT("CorrespondingPhysicsItem NULL"));
		}
	}
	else
	{
		UE_LOG(LogClass, Error, TEXT("CurrentWeapon NULL"));
	}
	**/
	//UE_LOG(LogClass, Warning, TEXT(" ALSBaseCharacter Tick: WindForce: %s"), *WindForce.ToString());
	WindForce = FMath::Lerp(WindForce, GridSample.Force, DeltaTime * 2.f);

	WindForce.X = FMath::Clamp(WindForce.X, -3000.f, 3000.f);
	WindForce.Y = FMath::Clamp(WindForce.Y, -3000.f, 3000.f);
	WindForce.Z = FMath::Clamp(WindForce.Z, -3000.f, 3000.f);
	FVector WindDirection = UKismetMathLibrary::GetDirectionUnitVector(FVector::ZeroVector, FVector::ZeroVector + WindForce);
	if (MovementState == EALSMovementState::Ragdoll)
	{	
		float VelocityDot = WindDirection | UKismetMathLibrary::GetDirectionUnitVector(FVector::ZeroVector, FVector::ZeroVector + LastRagdollVelocity);
		FVector RidingWindForce =   WindForce - (LastRagdollVelocity.Size() * WindDirection * FMath::Clamp(VelocityDot, 0.f, 1.f));
		GetMesh()->AddForceToAllBodiesBelow(RidingWindForce, FName(TEXT("Pelvis")), true, true);
		//GetMesh()->AddForceToAllBodiesBelow(ConstantForce - (RagdollVelocity * FMath::Clamp(VelocityDot, 0.f, 1.f)), FName(TEXT("Pelvis")), true, true);
			//UE_LOG(LogClass, Warning, TEXT("basecharacter ragdoll velocity = %f"), GetMesh()->GetPhysicsLinearVelocity().Size());
			//UE_LOG(LogClass, Warning, TEXT("basecharacter ragdoll VelocityDot = %f"), VelocityDot);
			//UE_LOG(LogClass, Warning, TEXT("basecharacter ragdoll WindForce = %f"), WindForce);
		//UE_LOG(LogClass, Warning, TEXT("basecharacter WindForce = %f"), WindForce.Size());	
	}
	else
	{
		FVector PlayerVelocity = GetMyMovementComponent()->Velocity;
		float VelocityDot = WindDirection | UKismetMathLibrary::GetDirectionUnitVector(FVector::ZeroVector, FVector::ZeroVector + PlayerVelocity);
		FVector RidingWindForce = WindForce - (PlayerVelocity.Size() * WindDirection * FMath::Clamp(VelocityDot, 0.f, 1.f));
		//DrawDebugLine(this->GetWorld(), GetActorLocation(), GridSample.Location, FColor::Red, false, .01f, 0, 4.f);
		
		GetMyMovementComponent()->AddForce(RidingWindForce);

		DrawDebugDirectionalArrow(GetWorld(), GridSample.Location, GridSample.Location + (RidingWindForce.Normalize() * 100.f), 50.f, FColor::Purple, false, .25f, 0, 5.f);
		//UGameplayStatics::GetViewProjectionMatrix(CameraView, UnusedViewMatrix, UnusedProjectionMatrix, ViewProjectionMatrix);
		//DrawDebugFrustum(GetWorld(), FirstPersonCameraComponent->matrix)
	}
	if (DrawDebugStuff)
	{
		DrawDebugSphere
		(
			this->GetWorld(),
			VecGravSample.SampleLocation,
			10.f,
			5,
			FColor::Yellow,
			false,
			.01f,
			0,
			20.f
		);
		DrawDebugLine(this->GetWorld(), GetActorLocation(), GetActorLocation() + WindForce, FColor::Red, false, .01f, 0, 4.f);
	}
	
	

	// Set required values
	SetEssentialValues(DeltaTime);

	if (MovementState == EALSMovementState::Grounded)
	{
		UpdateCharacterMovement();
		UpdateGroundedRotation(DeltaTime);
	}
	else if (MovementState == EALSMovementState::InAir)
	{
		UpdateInAirRotation(DeltaTime);

		// Perform a mantle check if falling while movement input is pressed.
		if (bHasMovementInput)
		{
			MantleCheck(FallingTraceSettings);
		}
	}
	else if (MovementState == EALSMovementState::Ragdoll)
	{
		RagdollUpdate(DeltaTime);
	}

	


	// Cache values
	PreviousVelocity = GetVelocity();
	PreviousAimYaw = AimingRotation.Yaw;

	DrawDebugSpheres();

	
	
}

void AALSBaseCharacter::RagdollStart()
{
	/** When Networked, disables replicate movement reset TargetRagdollLocation and ServerRagdollPull variable
	and if the host is a dedicated server, change character mesh optimisation option to avoid z-location bug*/
	MyCharacterMovementComponent->bIgnoreClientMovementErrorChecksAndCorrection = 1;

	if (UKismetSystemLibrary::IsDedicatedServer(GetWorld()))
	{
		DefVisBasedTickOp = GetMesh()->VisibilityBasedAnimTickOption;
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
	TargetRagdollLocation = GetMesh()->GetSocketLocation(FName(TEXT("Pelvis")));
	
	ServerRagdollPull = 0;

	// Step 1: Clear the Character Movement Mode and set the Movement State to Ragdoll
	GetCharacterMovement()->SetMovementMode(MOVE_None);
	SetMovementState(EALSMovementState::Ragdoll);
	

	// Step 2: Disable capsule collision and enable mesh physics simulation starting from the pelvis.
	CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionObjectType(ECC_PhysicsBody);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetMesh()->SetAllBodiesBelowSimulatePhysics(FName(TEXT("Pelvis")), true, true);

	GetMesh()->SetEnableGravity(false);
	// Step 3: Stop any active montages.
	MainAnimInstance->Montage_Stop(0.2f);

	SetReplicateMovement(false);
	
}

void AALSBaseCharacter::RagdollEnd()
{
	/** Re-enable Replicate Movement and if the host is a dedicated server set mesh visibility based anim
	tick option back to default*/

	if (UKismetSystemLibrary::IsDedicatedServer(GetWorld()))
	{
		GetMesh()->VisibilityBasedAnimTickOption = DefVisBasedTickOp;
	}

	MyCharacterMovementComponent->bIgnoreClientMovementErrorChecksAndCorrection = 0;
	SetReplicateMovement(true);

	if (!MainAnimInstance)
	{
		return;
	}

	// Step 1: Save a snapshot of the current Ragdoll Pose for use in AnimGraph to blend out of the ragdoll
	MainAnimInstance->SavePoseSnapshot(FName(TEXT("RagdollPose")));

	// Step 2: If the ragdoll is on the ground, set the movement mode to walking and play a Get Up animation.
	// If not, set the movement mode to falling and update the character movement velocity to match the last ragdoll velocity.
	if (bRagdollOnGround)
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		MainAnimInstance->Montage_Play(GetGetUpAnimation(bRagdollFaceUp),
		                               1.0f, EMontagePlayReturnType::MontageLength, 0.0f, true);
	}
	else
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		GetCharacterMovement()->Velocity = LastRagdollVelocity;
	}

	// Step 3: Re-Enable capsule collision, and disable physics simulation on the mesh.
	CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetMesh()->SetCollisionObjectType(ECC_Pawn);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GetMesh()->SetAllBodiesSimulatePhysics(false);
}

void AALSBaseCharacter::Server_SetMeshLocationDuringRagdoll_Implementation(FVector MeshLocation)
{
	TargetRagdollLocation = MeshLocation;
}

void AALSBaseCharacter::Server_SetPitchAndYaw_Implementation(float Yaw, float Pitch, FRotator SlerperRotation)
{
	ReplicatedQuatYawRotation = SlerperRotation;
	DeltaYaw = Yaw;
	DeltaPitch = Pitch;
}
void AALSBaseCharacter::Server_SetCameraRotation_Implementation(FRotator Rot)
{
	CameraRotation = Rot;
	MyCharacterMovementComponent->GravityControlRotation(Rot);
}

void AALSBaseCharacter::SetMovementState(const EALSMovementState NewState)
{
	if (MovementState != NewState)
	{
		PrevMovementState = MovementState;
		MovementState = NewState;
		FALSAnimCharacterInformation& AnimData = MainAnimInstance->GetCharacterInformationMutable();
		AnimData.PrevMovementState = PrevMovementState;
		MainAnimInstance->MovementState = MovementState;
		OnMovementStateChanged(PrevMovementState);
	}
}

bool AALSBaseCharacter::CanReload()
{

	if (MovementState != EALSMovementState::Ragdoll)
	{
		return true;
	}
	return false;
}

void AALSBaseCharacter::SetMovementAction(const EALSMovementAction NewAction)
{
	if (MovementAction != NewAction)
	{
		const EALSMovementAction Prev = MovementAction;
		MovementAction = NewAction;
		MainAnimInstance->MovementAction = MovementAction;
		OnMovementActionChanged(Prev);
	}
}

void AALSBaseCharacter::SetStance(const EALSStance NewStance)
{
	if (Stance != NewStance)
	{
		const EALSStance Prev = Stance;
		Stance = NewStance;
		MainAnimInstance->Stance = Stance;
		OnStanceChanged(Prev);
	}
}

void AALSBaseCharacter::SetGait(const EALSGait NewGait)
{
	if (Gait != NewGait)
	{
		Gait = NewGait;
		MainAnimInstance->Gait = Gait;
	}
}


void AALSBaseCharacter::SetDesiredStance(EALSStance NewStance)
{
	DesiredStance = NewStance;
	if (GetLocalRole() == ROLE_AutonomousProxy)
	{
		Server_SetDesiredStance(NewStance);
	}
}

void AALSBaseCharacter::Server_SetDesiredStance_Implementation(EALSStance NewStance)
{
	SetDesiredStance(NewStance);
}

void AALSBaseCharacter::SetDesiredGait(const EALSGait NewGait)
{
	DesiredGait = NewGait;
	if (GetLocalRole() == ROLE_AutonomousProxy)
	{
		Server_SetDesiredGait(NewGait);
	}
}

void AALSBaseCharacter::Server_SetDesiredGait_Implementation(EALSGait NewGait)
{
	SetDesiredGait(NewGait);
}

void AALSBaseCharacter::SetDesiredRotationMode(EALSRotationMode NewRotMode)
{
	DesiredRotationMode = NewRotMode;

	if (GetLocalRole() == ROLE_AutonomousProxy)
	{
		Server_SetDesiredRotationMode(NewRotMode);
	}
}

void AALSBaseCharacter::Server_SetDesiredRotationMode_Implementation(EALSRotationMode NewRotMode)
{
	SetDesiredRotationMode(NewRotMode);
}

void AALSBaseCharacter::SetRotationMode(const EALSRotationMode NewRotationMode)
{
	if (RotationMode != NewRotationMode)
	{
		const EALSRotationMode Prev = RotationMode;
		RotationMode = NewRotationMode;
		OnRotationModeChanged(Prev);

		if (GetLocalRole() == ROLE_AutonomousProxy)
		{
			Server_SetRotationMode(NewRotationMode);
		}
	}
}

void AALSBaseCharacter::Server_SetRotationMode_Implementation(EALSRotationMode NewRotationMode)
{
	SetRotationMode(NewRotationMode);
}

void AALSBaseCharacter::SetViewMode(const EALSViewMode NewViewMode)
{
	if (ViewMode != NewViewMode)
	{
		const EALSViewMode Prev = ViewMode;
		ViewMode = NewViewMode;
		OnViewModeChanged(Prev);

		if (GetLocalRole() == ROLE_AutonomousProxy)
		{
			Server_SetViewMode(NewViewMode);
		}
	}
}

void AALSBaseCharacter::Server_SetViewMode_Implementation(EALSViewMode NewViewMode)
{
	SetViewMode(NewViewMode);
}

void AALSBaseCharacter::SetOverlayState(const EALSOverlayState NewState)
{
	if (OverlayState != NewState)
	{
		const EALSOverlayState Prev = OverlayState;
		OverlayState = NewState;
		OnOverlayStateChanged(Prev);

		if (GetLocalRole() == ROLE_AutonomousProxy)
		{
			Server_SetOverlayState(NewState);
		}
	}
}


void AALSBaseCharacter::Server_SetOverlayState_Implementation(EALSOverlayState NewState)
{
	SetOverlayState(NewState);
}

void AALSBaseCharacter::EventOnLanded()
{
	const float VelZ = FMath::Abs(GetCharacterMovement()->Velocity.Z);

	if (bRagdollOnLand && VelZ > RagdollOnLandVelocity)
	{
		//ReplicatedRagdollStart();
		//Adding breakfall here instead of ragdoll for now
		OnBreakfall();
	}
	else if (bBreakfallOnLand && bHasMovementInput && VelZ >= BreakfallOnLandVelocity)
	{
		OnBreakfall();
	}
	else
	{
		GetCharacterMovement()->BrakingFrictionFactor = bHasMovementInput ? 0.5f : 3.0f;

		// After 0.5 secs, reset braking friction factor to zero
		GetWorldTimerManager().SetTimer(OnLandedFrictionResetTimer, this,
		                                &AALSBaseCharacter::OnLandFrictionReset, 0.5f, false);
	}
}

void AALSBaseCharacter::Multicast_OnLanded_Implementation()
{
	if (!IsLocallyControlled())
	{
		EventOnLanded();
	}
}

void AALSBaseCharacter::EventOnJumped()
{
	// Set the new In Air Rotation to the velocity rotation if speed is greater than 100.
	InAirRotation = Speed > 100.0f ? LastVelocityRotation : GetActorRotation();
	MainAnimInstance->OnJumped();
}

void AALSBaseCharacter::Server_MantleStart_Implementation(float MantleHeight,
                                                          const FALSComponentAndTransform& MantleLedgeWS,
                                                          EALSMantleType MantleType)
{
	Multicast_MantleStart(MantleHeight, MantleLedgeWS, MantleType);
}

void AALSBaseCharacter::Multicast_MantleStart_Implementation(float MantleHeight,
                                                             const FALSComponentAndTransform& MantleLedgeWS,
                                                             EALSMantleType MantleType)
{
	if (!IsLocallyControlled())
	{
		MantleStart(MantleHeight, MantleLedgeWS, MantleType);
	}
}

void AALSBaseCharacter::Server_PlayMontage_Implementation(UAnimMontage* montage, float track)
{
	Multicast_PlayMontage(montage, track);
}

void AALSBaseCharacter::Multicast_PlayMontage_Implementation(UAnimMontage* montage, float track)
{
	if (!IsLocallyControlled())
	{
		// Roll: Simply play a Root Motion Montage.
		MainAnimInstance->Montage_Play(montage, track);
	}
}

void AALSBaseCharacter::Multicast_OnJumped_Implementation()
{
	if (!IsLocallyControlled())
	{
		EventOnJumped();
	}
}

void AALSBaseCharacter::Server_RagdollStart_Implementation()
{
	Multicast_RagdollStart();
}

void AALSBaseCharacter::Multicast_RagdollStart_Implementation()
{
	RagdollStart();
}

void AALSBaseCharacter::Server_RagdollEnd_Implementation(FVector CharacterLocation)
{
	Multicast_RagdollEnd(CharacterLocation);
}

void AALSBaseCharacter::Multicast_RagdollEnd_Implementation(FVector CharacterLocation)
{
	RagdollEnd();
}

void AALSBaseCharacter::SetActorLocationAndTargetRotation(FVector NewLocation, FRotator NewRotation)
{
	SetActorLocationAndRotation(NewLocation, NewRotation);
	TargetRotation = NewRotation;
}

bool AALSBaseCharacter::MantleCheckGrounded()
{
	return MantleCheck(GroundedTraceSettings);
}

bool AALSBaseCharacter::MantleCheckFalling()
{
	return MantleCheck(FallingTraceSettings);
}

void AALSBaseCharacter::SetMovementModel()
{
	const FString ContextString = GetFullName();
	FALSMovementStateSettings* OutRow =
		MovementModel.DataTable->FindRow<FALSMovementStateSettings>(MovementModel.RowName, ContextString);
	check(OutRow);
	MovementData = *OutRow;
}

void AALSBaseCharacter::SetHasMovementInput(bool bNewHasMovementInput)
{
	bHasMovementInput = bNewHasMovementInput;
	MainAnimInstance->GetCharacterInformationMutable().bHasMovementInput = bHasMovementInput;
}

FALSMovementSettings AALSBaseCharacter::GetTargetMovementSettings() const
{
	if (RotationMode == EALSRotationMode::VelocityDirection)
	{
		if (Stance == EALSStance::Standing)
		{
			return MovementData.VelocityDirection.Standing;
		}
		if (Stance == EALSStance::Crouching)
		{
			return MovementData.VelocityDirection.Crouching;
		}
	}
	else if (RotationMode == EALSRotationMode::LookingDirection)
	{
		if (Stance == EALSStance::Standing)
		{
			return MovementData.LookingDirection.Standing;
		}
		if (Stance == EALSStance::Crouching)
		{
			return MovementData.LookingDirection.Crouching;
		}
	}
	else if (RotationMode == EALSRotationMode::Aiming)
	{
		if (Stance == EALSStance::Standing)
		{
			return MovementData.Aiming.Standing;
		}
		if (Stance == EALSStance::Crouching)
		{
			return MovementData.Aiming.Crouching;
		}
	}

	// Default to velocity dir standing
	return MovementData.VelocityDirection.Standing;
}

bool AALSBaseCharacter::CanSprint() const
{
	// Determine if the character is currently able to sprint based on the Rotation mode and current acceleration
	// (input) rotation. If the character is in the Looking Rotation mode, only allow sprinting if there is full
	// movement input and it is faced forward relative to the camera + or - 50 degrees.

	if (!bHasMovementInput || RotationMode == EALSRotationMode::Aiming)
	{
		return false;
	}

	const bool bValidInputAmount = MovementInputAmount > 0.9f;

	if (RotationMode == EALSRotationMode::VelocityDirection)
	{
		return bValidInputAmount;
	}

	if (RotationMode == EALSRotationMode::LookingDirection)
	{
		const FRotator AccRot = ReplicatedCurrentAcceleration.ToOrientationRotator();
		FRotator Delta = AccRot - AimingRotation;
		Delta.Normalize();

		return bValidInputAmount && FMath::Abs(Delta.Yaw) < 50.0f;
	}

	return false;
}

void AALSBaseCharacter::SetIsMoving(bool bNewIsMoving)
{
	bIsMoving = bNewIsMoving;
	MainAnimInstance->GetCharacterInformationMutable().bIsMoving = bIsMoving;
}

FVector AALSBaseCharacter::GetMovementInput() const
{
	return ReplicatedCurrentAcceleration;
}

void AALSBaseCharacter::SetMovementInputAmount(float NewMovementInputAmount)
{
	MovementInputAmount = NewMovementInputAmount;
	MainAnimInstance->GetCharacterInformationMutable().MovementInputAmount = MovementInputAmount;
}

void AALSBaseCharacter::SetSpeed(float NewSpeed)
{
	Speed = NewSpeed;
	MainAnimInstance->GetCharacterInformationMutable().Speed = Speed;
}

float AALSBaseCharacter::GetAnimCurveValue(FName CurveName) const
{
	if (MainAnimInstance)
	{
		return MainAnimInstance->GetCurveValue(CurveName);
	}

	return 0.0f;
}

ECollisionChannel AALSBaseCharacter::GetThirdPersonTraceParams(FVector& TraceOrigin, float& TraceRadius)
{
	TraceOrigin = GetActorLocation();
	TraceRadius = 10.0f;
	return ECC_Visibility;
}

FTransform AALSBaseCharacter::GetThirdPersonPivotTarget()
{
	return GetActorTransform();
}

FVector AALSBaseCharacter::GetFirstPersonCameraTarget()
{
	//return GetMesh()->GetSocketLocation(FName(TEXT("FP_Camera")));
	return FirstPersonCameraComponent->GetComponentLocation();
}

UCameraComponent* AALSBaseCharacter::GetFirstPersonCamera()
{
	return FirstPersonCameraComponent;
}

UStaticMeshComponent* AALSBaseCharacter::GetCameraPoll()
{
	return CameraPoll;
}

FRotator AALSBaseCharacter::GetFirstPersonCameraRotation()
{
	//return GetMesh()->GetSocketLocation(FName(TEXT("FP_Camera")));
	return FirstPersonCameraComponent->GetComponentRotation();
}

void AALSBaseCharacter::GetCameraParameters(float& TPFOVOut, float& FPFOVOut, bool& bRightShoulderOut) const
{
	TPFOVOut = ThirdPersonFOV;
	FPFOVOut = FirstPersonFOV;
	bRightShoulderOut = bRightShoulder;
}

void AALSBaseCharacter::SetAcceleration(const FVector& NewAcceleration)
{
	Acceleration = (NewAcceleration != FVector::ZeroVector || IsLocallyControlled())
		               ? NewAcceleration
		               : Acceleration / 2;
	MainAnimInstance->GetCharacterInformationMutable().Acceleration = Acceleration;
}

void AALSBaseCharacter::RagdollUpdate(float DeltaTime)
{
	// Set the Last Ragdoll Velocity.
	const FVector NewRagdollVel = GetMesh()->GetPhysicsLinearVelocity(FName(TEXT("root")));
	float RagdollVelChange = FMath::Abs(LastRagdollVelocity.Size() - NewRagdollVel.Size());
	if (RagdollVelChange > 1200.f)
	{
		//StunTimer = 0.f;
	}
	/**
	LastRagdollVelocity = (NewRagdollVel != FVector::ZeroVector || IsLocallyControlled())
		                      ? NewRagdollVel
		                      : LastRagdollVelocity / 2;
**/
	// Use the Ragdoll Velocity to scale the ragdoll's joint strength for physical animation.
	
	float SpringValue = FMath::GetMappedRangeValueClamped({0.0f, 1000.0f}, {0.0f, 2300.f}, LastRagdollVelocity.Size());
	
	float Scalar = FMath::Clamp(1.f - (MaxStunTimerValue - StunTimer), 0.f, 1.f);
	SpringValue = SpringValue * Scalar;
	
	GetMesh()->SetAllMotorsAngularDriveParams(SpringValue, 0.0f, 0.0f, false);

	if (StunTimer < MaxStunTimerValue)
	{
		StunTimer += DeltaTime;
		StunTimer = FMath::Clamp(StunTimer, 0.f, MaxStunTimerValue);
	}

	//UE_LOG(LogClass, Log, TEXT("RagdollUpdate SpringValue: %f"), SpringValue);
	//UE_LOG(LogClass, Warning, TEXT("RagdollUpdate RagdollVelChange: %f"), RagdollVelChange);
	//UE_LOG(LogClass, Warning, TEXT("RagdollUpdate StunTimer: %f , SpringValue: %f, Scalar: %f"),StunTimer, SpringValue, Scalar);
	//UE_LOG(LogClass, Log, TEXT("RagdollUpdate Scalar: %f"), Scalar);

	// Disable Gravity if falling faster than -4000 to prevent continual acceleration.
	// This also prevents the ragdoll from going through the floor.
	const bool bEnableGrav = LastRagdollVelocity.Z > -4000.0f;
	GetMesh()->SetEnableGravity(bEnableGrav);

	LastRagdollVelocity = NewRagdollVel;
	// Update the Actor location to follow the ragdoll.
	SetActorLocationDuringRagdoll(DeltaTime);
	
	
}

void AALSBaseCharacter::SetActorLocationDuringRagdoll(float DeltaTime)
{
	if (IsLocallyControlled())
	{
		// Set the pelvis as the target location.
		TargetRagdollLocation = GetMesh()->GetSocketLocation(FName(TEXT("Pelvis")));
		if (!HasAuthority())
		{
			Server_SetMeshLocationDuringRagdoll(TargetRagdollLocation);
		}
	}

	// Determine wether the ragdoll is facing up or down and set the target rotation accordingly.
	const FRotator PelvisRot = GetMesh()->GetSocketRotation(FName(TEXT("Pelvis")));

	bRagdollFaceUp = PelvisRot.Roll < 0.0f;

	const FRotator TargetRagdollRotation(0.0f, bRagdollFaceUp ? PelvisRot.Yaw - 180.0f : PelvisRot.Yaw, 0.0f);

	// Trace downward from the target location to offset the target location,
	// preventing the lower half of the capsule from going through the floor when the ragdoll is laying on the ground.
	const FVector TraceVect(TargetRagdollLocation.X, TargetRagdollLocation.Y,
	                        TargetRagdollLocation.Z - CapsuleComponent->GetScaledCapsuleHalfHeight());

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	FHitResult HitResult;
	GetWorld()->LineTraceSingleByChannel(HitResult, TargetRagdollLocation, TraceVect,
	                                     ECC_Visibility, Params);

	bRagdollOnGround = HitResult.IsValidBlockingHit();
	FVector NewRagdollLoc = TargetRagdollLocation;

	if (bRagdollOnGround)
	{
		const float ImpactDistZ = FMath::Abs(HitResult.ImpactPoint.Z - HitResult.TraceStart.Z);
		NewRagdollLoc.Z += CapsuleComponent->GetScaledCapsuleHalfHeight() - ImpactDistZ + 2.0f;
	}
	if (!IsLocallyControlled())
	{
		ServerRagdollPull = FMath::FInterpTo(ServerRagdollPull, 750, DeltaTime, 0.6);
		float RagdollSpeed = FVector(LastRagdollVelocity.X, LastRagdollVelocity.Y, 0).Size();
		FName RagdollSocketPullName = RagdollSpeed > 300 ? FName(TEXT("spine_03")) : FName(TEXT("pelvis"));
		GetMesh()->AddForce(
			(TargetRagdollLocation - GetMesh()->GetSocketLocation(RagdollSocketPullName)) * ServerRagdollPull,
			RagdollSocketPullName, true);
	}
	SetActorLocationAndTargetRotation(bRagdollOnGround ? NewRagdollLoc : TargetRagdollLocation, TargetRagdollRotation);
}

void AALSBaseCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	// Use the Character Movement Mode changes to set the Movement States to the right values. This allows you to have
	// a custom set of movement states but still use the functionality of the default character movement component.

	if (GetCharacterMovement()->MovementMode == MOVE_Walking ||
		GetCharacterMovement()->MovementMode == MOVE_NavWalking)
	{
		SetMovementState(EALSMovementState::Grounded);
	}
	else if (GetCharacterMovement()->MovementMode == MOVE_Falling)
	{
		SetMovementState(EALSMovementState::InAir);
	}
}

void AALSBaseCharacter::OnMovementStateChanged(const EALSMovementState PreviousState)
{
	if (MovementState == EALSMovementState::InAir)
	{
		if (MovementAction == EALSMovementAction::None)
		{
			// If the character enters the air, set the In Air Rotation and uncrouch if crouched.
			InAirRotation = GetActorRotation();
			if (Stance == EALSStance::Crouching)
			{
				UnCrouch();
			}
		}
		else if (MovementAction == EALSMovementAction::Rolling)
		{
			// If the character is currently rolling, enable the ragdoll.
			ReplicatedRagdollStart();
		}
	}
	else if (MovementState == EALSMovementState::Ragdoll && PreviousState == EALSMovementState::Mantling)
	{
		// Stop the Mantle Timeline if transitioning to the ragdoll state while mantling.
		MantleTimeline->Stop();
	}
}

void AALSBaseCharacter::OnMovementActionChanged(const EALSMovementAction PreviousAction)
{
	// Make the character crouch if performing a roll.
	if (MovementAction == EALSMovementAction::Rolling)
	{
		Crouch();
	}

	if (PreviousAction == EALSMovementAction::Rolling)
	{
		if (DesiredStance == EALSStance::Standing)
		{
			UnCrouch();
		}
		else if (DesiredStance == EALSStance::Crouching)
		{
			Crouch();
		}
	}
}
void AALSBaseCharacter::OnStanceChanged(const EALSStance PreviousStance)
{
}

void AALSBaseCharacter::OnRotationModeChanged(EALSRotationMode PreviousRotationMode)
{
	MainAnimInstance->RotationMode = RotationMode;
	if (RotationMode == EALSRotationMode::VelocityDirection && ViewMode == EALSViewMode::FirstPerson)
	{
		// If the new rotation mode is Velocity Direction and the character is in First Person,
		// set the viewmode to Third Person.
		SetViewMode(EALSViewMode::ThirdPerson);
	}
}

void AALSBaseCharacter::OnGaitChanged(const EALSGait PreviousGait)
{
}

void AALSBaseCharacter::OnViewModeChanged(const EALSViewMode PreviousViewMode)
{
	MainAnimInstance->GetCharacterInformationMutable().ViewMode = ViewMode;
	if (ViewMode == EALSViewMode::ThirdPerson)
	{
		if (RotationMode == EALSRotationMode::VelocityDirection || RotationMode == EALSRotationMode::LookingDirection)
		{
			// If Third Person, set the rotation mode back to the desired mode.
			SetRotationMode(DesiredRotationMode);
		}
	}
	else if (ViewMode == EALSViewMode::FirstPerson && RotationMode == EALSRotationMode::VelocityDirection)
	{
		// If First Person, set the rotation mode to looking direction if currently in the velocity direction mode.
		SetRotationMode(EALSRotationMode::LookingDirection);
	}
}

void AALSBaseCharacter::OnOverlayStateChanged(const EALSOverlayState PreviousState)
{
	MainAnimInstance->OverlayState = OverlayState;
}

void AALSBaseCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	SetStance(EALSStance::Crouching);
}

void AALSBaseCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	SetStance(EALSStance::Standing);
}

void AALSBaseCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();
	if (IsLocallyControlled())
	{
		EventOnJumped();
	}
	if (HasAuthority())
	{
		Multicast_OnJumped();
	}
}

void AALSBaseCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if (IsLocallyControlled())
	{
		EventOnLanded();
	}
	if (HasAuthority())
	{
		Multicast_OnLanded();
	}
}

void AALSBaseCharacter::OnLandFrictionReset()
{
	// Reset the braking friction
	GetCharacterMovement()->BrakingFrictionFactor = 0.0f;
}

void AALSBaseCharacter::SetEssentialValues(float DeltaTime)
{

	if (GetLocalRole() == ROLE_AutonomousProxy)
	{
		//Replicate the slerper and camera locations
		Server_SetCameraRotation(FirstPersonCameraComponent->GetComponentRotation());
		Server_SetPitchAndYaw(DeltaPitch, DeltaYaw, CameraPoll->GetComponentRotation());
	}
	if (GetLocalRole() != ROLE_SimulatedProxy)
	{
		//CameraRotation = FirstPersonCameraComponent->GetComponentRotation();
		ReplicatedCurrentAcceleration = GetCharacterMovement()->GetCurrentAcceleration();
		ReplicatedControlRotation = GetControlRotation();
		//ReplicatedQuatYawRotation = CameraPoll->GetComponentRotation();
		EasedMaxAcceleration = GetCharacterMovement()->GetMaxAcceleration();
	}

	else
	{
		EasedMaxAcceleration = GetCharacterMovement()->GetMaxAcceleration() != 0
			                       ? GetCharacterMovement()->GetMaxAcceleration()
			                       : EasedMaxAcceleration / 2;
	}
	if (IsLocallyControlled())
	{
		const FMatrix RotationMatrix = FRotationMatrix::MakeFromZX(CapsuleComponent->GetUpVector(), CameraPoll->GetForwardVector());
		//LocalCorrectedRight = RotationMatrix.ToQuat().GetRightVector();

		//ReplicatedQuatYawRotation = CameraPoll->GetComponentRotation();
		ReplicatedQuatYawRotation = RotationMatrix.Rotator();
		CameraRotation = FirstPersonCameraComponent->GetComponentRotation();
	}

	// Interp AimingRotation to current control rotation for smooth character rotation movement. Decrease InterpSpeed
	// for slower but smoother movement.
	AimingRotation = FMath::RInterpTo(AimingRotation, ReplicatedControlRotation, DeltaTime, 30);
	QuatYawRotation = FMath::RInterpTo(QuatYawRotation, ReplicatedQuatYawRotation, DeltaTime, 30);
	UpdateDeltaPitch();
	UpdateDeltaYaw();
	


	// These values represent how the capsule is moving as well as how it wants to move, and therefore are essential
	// for any data driven animation system. They are also used throughout the system for various functions,
	// so I found it is easiest to manage them all in one place.

	const FVector CurrentVel = GetVelocity();

	// Set the amount of Acceleration.
	SetAcceleration((CurrentVel - PreviousVelocity) / DeltaTime);

	// Determine if the character is moving by getting it's speed. The Speed equals the length of the horizontal (x y)
	// velocity, so it does not take vertical movement into account. If the character is moving, update the last
	// velocity rotation. This value is saved because it might be useful to know the last orientation of movement
	// even after the character has stopped.
	SetSpeed(CurrentVel.Size());
	SetIsMoving(Speed > 1.0f);
	if (bIsMoving)
	{
		LastVelocityRotation = CurrentVel.ToOrientationRotator();
		LastVelocityDirection = CurrentVel;
	}

	// Determine if the character has movement input by getting its movement input amount.
	// The Movement Input Amount is equal to the current acceleration divided by the max acceleration so that
	// it has a range of 0-1, 1 being the maximum possible amount of input, and 0 being none.
	// If the character has movement input, update the Last Movement Input Rotation.
	SetMovementInputAmount(ReplicatedCurrentAcceleration.Size() / EasedMaxAcceleration);
	SetHasMovementInput(MovementInputAmount > 0.0f);
	if (bHasMovementInput)
	{
		LastMovementInputRotation = ReplicatedCurrentAcceleration.ToOrientationRotator();
	}

	// Set the Aim Yaw rate by comparing the current and previous Aim Yaw value, divided by Delta Seconds.
	// This represents the speed the camera is rotating left to right.
	SetAimYawRate(FMath::Abs((AimingRotation.Yaw - PreviousAimYaw) / DeltaTime));
}

void AALSBaseCharacter::UpdateCharacterMovement()
{
	// Set the Allowed Gait
	const EALSGait AllowedGait = GetAllowedGait();

	// Determine the Actual Gait. If it is different from the current Gait, Set the new Gait Event.
	const EALSGait ActualGait = GetActualGait(AllowedGait);

	if (ActualGait != Gait)
	{
		SetGait(ActualGait);
	}

	// Use the allowed gait to update the movement settings.
	if (bDisableCurvedMovement)
	{
		// Don't use curves for movement
		UpdateDynamicMovementSettingsNetworked(AllowedGait);
	}
	else
	{
		// Use curves for movement
		UpdateDynamicMovementSettingsStandalone(AllowedGait);
	}
}

void AALSBaseCharacter::UpdateDynamicMovementSettingsStandalone(EALSGait AllowedGait)
{
	// Get the Current Movement Settings.
	CurrentMovementSettings = GetTargetMovementSettings();
	const float NewMaxSpeed = CurrentMovementSettings.GetSpeedForGait(AllowedGait);

	// Update the Acceleration, Deceleration, and Ground Friction using the Movement Curve.
	// This allows for fine control over movement behavior at each speed (May not be suitable for replication).
	const float MappedSpeed = GetMappedSpeed();
	const FVector CurveVec = CurrentMovementSettings.MovementCurve->GetVectorValue(MappedSpeed);

	// Update the Character Max Walk Speed to the configured speeds based on the currently Allowed Gait.
	MyCharacterMovementComponent->SetMaxWalkingSpeed(NewMaxSpeed);
	GetCharacterMovement()->MaxAcceleration = CurveVec.X;
	GetCharacterMovement()->BrakingDecelerationWalking = CurveVec.Y;
	GetCharacterMovement()->GroundFriction = CurveVec.Z;
}

void AALSBaseCharacter::UpdateDynamicMovementSettingsNetworked(EALSGait AllowedGait)
{
	// Get the Current Movement Settings.
	CurrentMovementSettings = GetTargetMovementSettings();
	const float NewMaxSpeed = CurrentMovementSettings.GetSpeedForGait(AllowedGait);

	// Update the Character Max Walk Speed to the configured speeds based on the currently Allowed Gait.
	if (IsLocallyControlled() || HasAuthority())
	{
		if (GetCharacterMovement()->MaxWalkSpeed != NewMaxSpeed)
		{
			MyCharacterMovementComponent->SetMaxWalkingSpeed(NewMaxSpeed);
		}
	}
	else
	{
		GetCharacterMovement()->MaxWalkSpeed = NewMaxSpeed;
	}
}

void AALSBaseCharacter::UpdateGroundedRotation(float DeltaTime)
{
	if (MovementAction == EALSMovementAction::None)
	{
		const bool bCanUpdateMovingRot = ((bIsMoving && bHasMovementInput) || Speed > 150.0f) && !HasAnyRootMotion();
		if (bCanUpdateMovingRot)
		{
			const float GroundedRotationRate = CalculateGroundedRotationRate();
			if (RotationMode == EALSRotationMode::VelocityDirection)
			{
				UE_LOG(LogTemp, Warning, TEXT("RotationMode == EALSRotationMode::VelocityDirection"));
				// Velocity Direction Rotation
				SmoothCharacterRotation({0.0f, LastVelocityRotation.Yaw, 0.0f}, 800.0f, GroundedRotationRate,
				                        DeltaTime);
			}
			else if (RotationMode == EALSRotationMode::LookingDirection)
			{
				//UE_LOG(LogTemp, Warning, TEXT("RotationMode == EALSRotationMode::LookingDirection"));
				// Looking Direction Rotation
				float YawValue;
				if (Gait == EALSGait::Sprinting)
				{
					FVector QuatYawForward = UKismetMathLibrary::GetForwardVector(QuatYawRotation);
					float DeltaQuatDot = FVector::DotProduct(QuatYawForward, LastVelocityDirection);
					float DeltaQuatAcos = FMath::Acos(DeltaQuatDot);
					float DeltaQuatYaw = DeltaQuatAcos * 57.2958;

					float RightDot = FVector::DotProduct(QuatYawForward, CapsuleComponent->GetRightVector());
					//Do the opposite of Limit rotation since we are subtracting this rotation from our current forward control rotation
					if (RightDot < 0)
					{
						DeltaQuatYaw *= -1.f;
					}
					YawValue = DeltaQuatYaw;
				}
				else
				{
					// Walking or Running..
					const float YawOffsetCurveVal = MainAnimInstance->GetCurveValue(FName(TEXT("YawOffset")));
					//YawValue = AimingRotation.Yaw + YawOffsetCurveVal;
					YawValue = YawOffsetCurveVal;
				}
				FQuat DeltaQuatYaw = FRotator(0.f, YawValue, 0.f).Quaternion();
				const FMatrix RotationMatrix = FRotationMatrix::MakeFromZX(CapsuleComponent->GetUpVector(), CameraPoll->GetForwardVector());
				FRotator OutRotation = (RotationMatrix.ToQuat() * DeltaQuatYaw).Rotator();
				SmoothCharacterRotationYaw(1.f, ReplicatedQuatYawRotation, 500.f, GroundedRotationRate, DeltaTime);
			}
			else if (RotationMode == EALSRotationMode::Aiming)
			{
				const float ControlYaw = AimingRotation.Yaw;
				SmoothCharacterRotation(QuatYawRotation, 1000.0f, 20.0f, DeltaTime);
			}
		}
		else
		{
			// Not Moving

			if ((ViewMode == EALSViewMode::ThirdPerson && RotationMode == EALSRotationMode::Aiming) ||
				ViewMode == EALSViewMode::FirstPerson)
			{
				LimitRotation(-100.0f, 100.0f, 20.0f, DeltaTime);
			}

			// Apply the RotationAmount curve from Turn In Place Animations.
			// The Rotation Amount curve defines how much rotation should be applied each frame,
			// and is calculated for animations that are animated at 30fps.

			const float RotAmountCurve = MainAnimInstance->GetCurveValue(FName(TEXT("RotationAmount")));
			//UE_LOG(LogTemp, Warning, TEXT("RotAmountCurve Value: %f"), MainAnimInstance->GetCurveValue(FName(TEXT("RotationAmount"))));
			if (FMath::Abs(RotAmountCurve) > 0.001f)
			{
				float YawDelta = RotAmountCurve * (DeltaTime / (1.0f / 30.0f));
				//float DeltaYaw = 5.f * (DeltaTime / (1.0f / 30.0f));
				FQuat DeltaQuatYaw = FRotator(0.f, YawDelta, 0.f).Quaternion();
				//UE_LOG(LogTemp, Warning, TEXT("RotAmountCurve Value: %f"), MainAnimInstance->GetCurveValue(FName(TEXT("RotationAmount"))));
				if (GetLocalRole() == ROLE_AutonomousProxy)
				{	
					FQuat TargetQuat = TargetRotation.Quaternion() * DeltaQuatYaw;
					SetActorRotation(TargetQuat.Rotator());
					//FQuat CurrentRotation = GetActorQuat();
					//AddActorWorldRotation({ 0, RotAmountCurve * (DeltaTime / (1.0f / 30.0f)), 0 });
					
				}
				else if (GetLocalRole() == ROLE_SimulatedProxy)
				{
					//UE_LOG(LogTemp, Log, TEXT("ROLE_SimulatedProxy: ReplicatedQuatYawRotation: %s"), *ReplicatedQuatYawRotation.ToString());
					AddActorLocalRotation(DeltaQuatYaw);
				}
				else if (IsLocallyControlled())
				{
					
					FQuat CurrentRotation = GetActorQuat();
					//AddActorWorldRotation({ 0, RotAmountCurve * (DeltaTime / (1.0f / 30.0f)), 0 });
					//AddActorWorldRotation(DeltaQuatYaw);
					AddActorLocalRotation(DeltaQuatYaw);
					
				}
				
				TargetRotation = GetActorRotation();
			}
		}
	}
	else if (MovementAction == EALSMovementAction::Rolling)
	{
		// Rolling Rotation

		if (bHasMovementInput)
		{
			SmoothCharacterRotation({0.0f, LastMovementInputRotation.Yaw, 0.0f}, 0.0f, 2.0f, DeltaTime);
		}
	}

	// Other actions are ignored...
}

void AALSBaseCharacter::UpdateInAirRotation(float DeltaTime)
{
	if (RotationMode == EALSRotationMode::VelocityDirection || RotationMode == EALSRotationMode::LookingDirection)
	{
		// Velocity / Looking Direction Rotation
		//SmoothCharacterRotation({0.0f, InAirRotation.Yaw, 0.0f}, 0.0f, 5.0f, DeltaTime);
	}
	else if (RotationMode == EALSRotationMode::Aiming)
	{
		// Aiming Rotation
		//SmoothCharacterRotation({0.0f, AimingRotation.Yaw, 0.0f}, 0.0f, 15.0f, DeltaTime);
		InAirRotation = GetActorRotation();
	}
}

void AALSBaseCharacter::MantleStart(float MantleHeight, const FALSComponentAndTransform& MantleLedgeWS,
                                    EALSMantleType MantleType)
{
	// Step 1: Get the Mantle Asset and use it to set the new Mantle Params.
	const FALSMantleAsset& MantleAsset = GetMantleAsset(MantleType);

	MantleParams.AnimMontage = MantleAsset.AnimMontage;
	MantleParams.PositionCorrectionCurve = MantleAsset.PositionCorrectionCurve;
	MantleParams.StartingOffset = MantleAsset.StartingOffset;
	MantleParams.StartingPosition = FMath::GetMappedRangeValueClamped({MantleAsset.LowHeight, MantleAsset.HighHeight},
	                                                                  {
		                                                                  MantleAsset.LowStartPosition,
		                                                                  MantleAsset.HighStartPosition
	                                                                  },
	                                                                  MantleHeight);
	MantleParams.PlayRate = FMath::GetMappedRangeValueClamped({MantleAsset.LowHeight, MantleAsset.HighHeight},
	                                                          {MantleAsset.LowPlayRate, MantleAsset.HighPlayRate},
	                                                          MantleHeight);

	// Step 2: Convert the world space target to the mantle component's local space for use in moving objects.
	MantleLedgeLS.Component = MantleLedgeWS.Component;
	MantleLedgeLS.Transform = MantleLedgeWS.Transform * MantleLedgeWS.Component->GetComponentToWorld().Inverse();

	// Step 3: Set the Mantle Target and calculate the Starting Offset
	// (offset amount between the actor and target transform).
	MantleTarget = MantleLedgeWS.Transform;
	MantleActualStartOffset = UALSMathLibrary::TransfromSub(GetActorTransform(), MantleTarget);

	// Step 4: Calculate the Animated Start Offset from the Target Location.
	// This would be the location the actual animation starts at relative to the Target Transform.
	FVector RotatedVector = MantleTarget.GetRotation().Vector() * MantleParams.StartingOffset.Y;
	RotatedVector.Z = MantleParams.StartingOffset.Z;
	const FTransform StartOffset(MantleTarget.Rotator(), MantleTarget.GetLocation() - RotatedVector,
	                             FVector::OneVector);
	MantleAnimatedStartOffset = UALSMathLibrary::TransfromSub(StartOffset, MantleTarget);

	// Step 5: Clear the Character Movement Mode and set the Movement State to Mantling
	GetCharacterMovement()->SetMovementMode(MOVE_None);
	SetMovementState(EALSMovementState::Mantling);

	// Step 6: Configure the Mantle Timeline so that it is the same length as the
	// Lerp/Correction curve minus the starting position, and plays at the same speed as the animation.
	// Then start the timeline.
	float MinTime = 0.0f;
	float MaxTime = 0.0f;
	MantleParams.PositionCorrectionCurve->GetTimeRange(MinTime, MaxTime);
	MantleTimeline->SetTimelineLength(MaxTime - MantleParams.StartingPosition);
	MantleTimeline->SetPlayRate(MantleParams.PlayRate);
	MantleTimeline->PlayFromStart();

	// Step 7: Play the Anim Montaget if valid.
	if (IsValid(MantleParams.AnimMontage))
	{
		MainAnimInstance->Montage_Play(MantleParams.AnimMontage, MantleParams.PlayRate,
		                               EMontagePlayReturnType::MontageLength, MantleParams.StartingPosition, false);
	}
}

bool AALSBaseCharacter::MantleCheck(const FALSMantleTraceSettings& TraceSettings, EDrawDebugTrace::Type DebugType)
{
	// Step 1: Trace forward to find a wall / object the character cannot walk on.
	const FVector& CapsuleBaseLocation = UALSMathLibrary::GetCapsuleBaseLocation(2.0f, CapsuleComponent);
	FVector TraceStart = CapsuleBaseLocation + GetPlayerMovementInput() * -30.0f;
	TraceStart.Z += (TraceSettings.MaxLedgeHeight + TraceSettings.MinLedgeHeight) / 2.0f;
	const FVector TraceEnd = TraceStart + (GetPlayerMovementInput() * TraceSettings.ReachDistance);
	const float HalfHeight = 1.0f + ((TraceSettings.MaxLedgeHeight - TraceSettings.MinLedgeHeight) / 2.0f);

	UWorld* World = GetWorld();
	check(World);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	FHitResult HitResult;
	// ECC_GameTraceChannel2 -> Climbable
	World->SweepSingleByChannel(HitResult, TraceStart, TraceEnd, FQuat::Identity, ECC_GameTraceChannel2,
	                            FCollisionShape::MakeCapsule(TraceSettings.ForwardTraceRadius, HalfHeight), Params);

	if (!HitResult.IsValidBlockingHit() || GetCharacterMovement()->IsWalkable(HitResult))
	{
		// Not a valid surface to mantle
		return false;
	}
	
	if (HitResult.GetComponent() != nullptr) 
	{
		UPrimitiveComponent* PrimitiveComponent = HitResult.GetComponent();
		if (PrimitiveComponent && PrimitiveComponent->GetComponentVelocity().Size() > AcceptableVelocityWhileMantling)
		{
			// The surface to mantle moves too fast
			return false;
		}
	}
	
	const FVector InitialTraceImpactPoint = HitResult.ImpactPoint;
	const FVector InitialTraceNormal = HitResult.ImpactNormal;

	// Step 2: Trace downward from the first trace's Impact Point and determine if the hit location is walkable.
	FVector DownwardTraceEnd = InitialTraceImpactPoint;
	DownwardTraceEnd.Z = CapsuleBaseLocation.Z;
	DownwardTraceEnd += InitialTraceNormal * -15.0f;
	FVector DownwardTraceStart = DownwardTraceEnd;
	DownwardTraceStart.Z += TraceSettings.MaxLedgeHeight + TraceSettings.DownwardTraceRadius + 1.0f;

	World->SweepSingleByChannel(HitResult, DownwardTraceStart, DownwardTraceEnd, FQuat::Identity,
	                            ECC_GameTraceChannel2, FCollisionShape::MakeSphere(TraceSettings.DownwardTraceRadius),
	                            Params);


	if (!GetCharacterMovement()->IsWalkable(HitResult))
	{
		// Not a valid surface to mantle
		return false;
	}

	const FVector DownTraceLocation(HitResult.Location.X, HitResult.Location.Y, HitResult.ImpactPoint.Z);
	UPrimitiveComponent* HitComponent = HitResult.GetComponent();

	// Step 3: Check if the capsule has room to stand at the downward trace's location.
	// If so, set that location as the Target Transform and calculate the mantle height.
	const FVector& CapsuleLocationFBase = UALSMathLibrary::GetCapsuleLocationFromBase(
		DownTraceLocation, 2.0f, CapsuleComponent);
	const bool bCapsuleHasRoom = UALSMathLibrary::CapsuleHasRoomCheck(CapsuleComponent, CapsuleLocationFBase, 0.0f,
	                                                                  0.0f);

	if (!bCapsuleHasRoom)
	{
		// Capsule doesn't have enough room to mantle
		return false;
	}

	const FTransform TargetTransform(
		(InitialTraceNormal * FVector(-1.0f, -1.0f, 0.0f)).ToOrientationRotator(),
		CapsuleLocationFBase,
		FVector::OneVector);

	const float MantleHeight = (CapsuleLocationFBase - GetActorLocation()).Z;

	// Step 4: Determine the Mantle Type by checking the movement mode and Mantle Height.
	EALSMantleType MantleType;
	if (MovementState == EALSMovementState::InAir)
	{
		MantleType = EALSMantleType::FallingCatch;
	}
	else
	{
		MantleType = MantleHeight > 125.0f ? EALSMantleType::HighMantle : EALSMantleType::LowMantle;
	}

	// Step 5: If everything checks out, start the Mantle
	FALSComponentAndTransform MantleWS;
	MantleWS.Component = HitComponent;
	MantleWS.Transform = TargetTransform;
	MantleStart(MantleHeight, MantleWS, MantleType);
	Server_MantleStart(MantleHeight, MantleWS, MantleType);

	return true;
}

// This function is called by "MantleTimeline" using BindUFunction in the AALSBaseCharacter::BeginPlay during the default settings initalization.
void AALSBaseCharacter::MantleUpdate(float BlendIn)
{
	// Step 1: Continually update the mantle target from the stored local transform to follow along with moving objects
	MantleTarget = UALSMathLibrary::MantleComponentLocalToWorld(MantleLedgeLS);

	// Step 2: Update the Position and Correction Alphas using the Position/Correction curve set for each Mantle.
	const FVector CurveVec = MantleParams.PositionCorrectionCurve
	                                     ->GetVectorValue(
		                                     MantleParams.StartingPosition + MantleTimeline->GetPlaybackPosition());
	const float PositionAlpha = CurveVec.X;
	const float XYCorrectionAlpha = CurveVec.Y;
	const float ZCorrectionAlpha = CurveVec.Z;

	// Step 3: Lerp multiple transforms together for independent control over the horizontal
	// and vertical blend to the animated start position, as well as the target position.

	// Blend into the animated horizontal and rotation offset using the Y value of the Position/Correction Curve.
	const FTransform TargetHzTransform(MantleAnimatedStartOffset.GetRotation(),
	                                   {
		                                   MantleAnimatedStartOffset.GetLocation().X,
		                                   MantleAnimatedStartOffset.GetLocation().Y,
		                                   MantleActualStartOffset.GetLocation().Z
	                                   },
	                                   FVector::OneVector);
	const FTransform& HzLerpResult =
		UKismetMathLibrary::TLerp(MantleActualStartOffset, TargetHzTransform, XYCorrectionAlpha);

	// Blend into the animated vertical offset using the Z value of the Position/Correction Curve.
	const FTransform TargetVtTransform(MantleActualStartOffset.GetRotation(),
	                                   {
		                                   MantleActualStartOffset.GetLocation().X,
		                                   MantleActualStartOffset.GetLocation().Y,
		                                   MantleAnimatedStartOffset.GetLocation().Z
	                                   },
	                                   FVector::OneVector);
	const FTransform& VtLerpResult =
		UKismetMathLibrary::TLerp(MantleActualStartOffset, TargetVtTransform, ZCorrectionAlpha);

	const FTransform ResultTransform(HzLerpResult.GetRotation(),
	                                 {
		                                 HzLerpResult.GetLocation().X, HzLerpResult.GetLocation().Y,
		                                 VtLerpResult.GetLocation().Z
	                                 },
	                                 FVector::OneVector);

	// Blend from the currently blending transforms into the final mantle target using the X
	// value of the Position/Correction Curve.
	const FTransform& ResultLerp = UKismetMathLibrary::TLerp(
		UALSMathLibrary::TransfromAdd(MantleTarget, ResultTransform), MantleTarget,
		PositionAlpha);

	// Initial Blend In (controlled in the timeline curve) to allow the actor to blend into the Position/Correction
	// curve at the midoint. This prevents pops when mantling an object lower than the animated mantle.
	const FTransform& LerpedTarget =
		UKismetMathLibrary::TLerp(UALSMathLibrary::TransfromAdd(MantleTarget, MantleActualStartOffset), ResultLerp,
		                          BlendIn);

	// Step 4: Set the actors location and rotation to the Lerped Target.
	SetActorLocationAndTargetRotation(LerpedTarget.GetLocation(), LerpedTarget.GetRotation().Rotator());
}

void AALSBaseCharacter::MantleEnd()
{
	// Set the Character Movement Mode to Walking
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

float AALSBaseCharacter::GetMappedSpeed() const
{
	// Map the character's current speed to the configured movement speeds with a range of 0-3,
	// with 0 = stopped, 1 = the Walk Speed, 2 = the Run Speed, and 3 = the Sprint Speed.
	// This allows us to vary the movement speeds but still use the mapped range in calculations for consistent results

	const float LocWalkSpeed = CurrentMovementSettings.WalkSpeed;
	const float LocRunSpeed = CurrentMovementSettings.RunSpeed;
	const float LocSprintSpeed = CurrentMovementSettings.SprintSpeed;

	if (Speed > LocRunSpeed)
	{
		return FMath::GetMappedRangeValueClamped({LocRunSpeed, LocSprintSpeed}, {2.0f, 3.0f}, Speed);
	}

	if (Speed > LocWalkSpeed)
	{
		return FMath::GetMappedRangeValueClamped({LocWalkSpeed, LocRunSpeed}, {1.0f, 2.0f}, Speed);
	}

	return FMath::GetMappedRangeValueClamped({0.0f, LocWalkSpeed}, {0.0f, 1.0f}, Speed);
}

EALSGait AALSBaseCharacter::GetAllowedGait() const
{
	// Calculate the Allowed Gait. This represents the maximum Gait the character is currently allowed to be in,
	// and can be determined by the desired gait, the rotation mode, the stance, etc. For example,
	// if you wanted to force the character into a walking state while indoors, this could be done here.

	if (Stance == EALSStance::Standing)
	{
		if (RotationMode != EALSRotationMode::Aiming)
		{
			if (DesiredGait == EALSGait::Sprinting)
			{
				return CanSprint() ? EALSGait::Sprinting : EALSGait::Running;
			}
			return DesiredGait;
		}
	}

	// Crouching stance & Aiming rot mode has same behaviour

	if (DesiredGait == EALSGait::Sprinting)
	{
		return EALSGait::Running;
	}

	return DesiredGait;
}

EALSGait AALSBaseCharacter::GetActualGait(EALSGait AllowedGait) const
{
	// Get the Actual Gait. This is calculated by the actual movement of the character,  and so it can be different
	// from the desired gait or allowed gait. For instance, if the Allowed Gait becomes walking,
	// the Actual gait will still be running untill the character decelerates to the walking speed.

	const float LocWalkSpeed = CurrentMovementSettings.WalkSpeed;
	const float LocRunSpeed = CurrentMovementSettings.RunSpeed;

	if (Speed > LocRunSpeed + 10.0f)
	{
		if (AllowedGait == EALSGait::Sprinting)
		{
			return EALSGait::Sprinting;
		}
		return EALSGait::Running;
	}

	if (Speed >= LocWalkSpeed + 10.0f)
	{
		return EALSGait::Running;
	}

	return EALSGait::Walking;
}


void AALSBaseCharacter::SmoothCharacterRotationYaw(float DeltaQuatYaw, FRotator Target, float TargetInterpSpeed, float ActorInterpSpeed,
	float DeltaTime)
{
//FMath::FInterpTo()
	FQuat OutTargetQuat = FMath::QInterpConstantTo(TargetRotation.Quaternion(), Target.Quaternion(), DeltaTime, TargetInterpSpeed);
	// Interpolate the Target Rotation for extra smooth rotation behavior
	TargetRotation = OutTargetQuat.Rotator();
	SetActorRotation(
		FMath::QInterpTo(GetActorQuat(), OutTargetQuat, DeltaTime, ActorInterpSpeed));
}

void AALSBaseCharacter::SmoothCharacterRotation(FRotator Target, float TargetInterpSpeed, float ActorInterpSpeed,
                                                float DeltaTime)
{
	// Interpolate the Target Rotation for extra smooth rotation behavior
	TargetRotation =
		FMath::RInterpConstantTo(TargetRotation, Target, DeltaTime, TargetInterpSpeed);
	SetActorRotation(
		FMath::RInterpTo(GetActorRotation(), TargetRotation, DeltaTime, ActorInterpSpeed));
}
void AALSBaseCharacter::SmoothGravityCharacterRotation(FRotator Target, float TargetInterpSpeed, float ActorInterpSpeed,
	float DeltaTime)
{
	// Interpolate the Target Rotation for extra smooth rotation behavior
	TargetRotation =
		FMath::RInterpConstantTo(TargetRotation, Target, DeltaTime, TargetInterpSpeed);
	SetActorRotation(
		FMath::RInterpTo(GetActorRotation(), TargetRotation, DeltaTime, ActorInterpSpeed));
}


float AALSBaseCharacter::CalculateGroundedRotationRate() const
{
	// Calculate the rotation rate by using the current Rotation Rate Curve in the Movement Settings.
	// Using the curve in conjunction with the mapped speed gives you a high level of control over the rotation
	// rates for each speed. Increase the speed if the camera is rotating quickly for more responsive rotation.

	const float MappedSpeedVal = GetMappedSpeed();
	const float CurveVal =
		CurrentMovementSettings.RotationRateCurve->GetFloatValue(MappedSpeedVal);
	const float ClampedAimYawRate = FMath::GetMappedRangeValueClamped({0.0f, 300.0f}, {1.0f, 3.0f}, AimYawRate);
	return CurveVal * ClampedAimYawRate;
}

void AALSBaseCharacter::LimitRotation(float AimYawMin, float AimYawMax, float InterpSpeed, float DeltaTime)
{
	// Prevent the character from rotating past a certain angle.
	FVector QuatYawForward = UKismetMathLibrary::GetForwardVector(QuatYawRotation);
	float DeltaQuatDot = FVector::DotProduct(QuatYawForward, CapsuleComponent->GetForwardVector());
	float DeltaQuatAcos = FMath::Acos(DeltaQuatDot);
	float DeltaQuatYaw = DeltaQuatAcos * 57.2958;

	float RightDot = FVector::DotProduct(QuatYawForward, CapsuleComponent->GetRightVector());
	//if the  dot product between Quatyaw rotation and actor right vector is greater than 0, we rotate right. 
	if (RightDot > 0)
	{
		DeltaQuatYaw *= -1.f;
	}
	FRotator Delta = FRotator(0.0f, DeltaQuatYaw, 0.0f);
	
	Delta.Normalize();
	
	//UE_LOG(LogTemp, Warning, TEXT("DeltaQuatYaw: %f,  DeltaYaw: %f"), DeltaQuatYaw, Delta.Yaw);
	
	const float RangeVal = Delta.Yaw;

	if (RangeVal < AimYawMin || RangeVal > AimYawMax)
	{
		//FQuat TargetDelta = FRotator(0.f, (RangeVal > 0.0f ? AimYawMin : AimYawMax),0.f).Quaternion();
		FQuat DeltaQuat = Delta.Quaternion();
		FQuat TargetQuat = GetActorRotation().Quaternion() * DeltaQuat;
		SmoothCharacterRotation(TargetQuat.Rotator(), 0.0f, InterpSpeed, DeltaTime);
	}
}

void AALSBaseCharacter::GetControlForwardRightVector(FVector& Forward, FVector& Right) const
{
	Forward = GetInputAxisValue("MoveForward/Backwards") * CameraPoll->GetRightVector();
	Right = GetInputAxisValue("MoveRight/Left") * CameraPoll->GetForwardVector();
}

void AALSBaseCharacter::OnFire()
{

	//AALSPlayerController* MyPC = Cast<AALSPlayerController>(Controller);
	//if (MyPC && MyPC->IsGameInputAllowed())
//	{
	StartWeaponFire();
//	}
/**
TArray<AActor*> IgnoreArray;
	FCollisionQueryParams CollisionParams;
	CollisionParams.AddIgnoredActor(this);
	FHitResult HitResult;
	FVector HitStart = FirstPersonCameraComponent->GetComponentLocation();
	FVector HitEnd = HitStart + FirstPersonCameraComponent->GetForwardVector() * 9000.f;
	GetWorld()->LineTraceSingleByChannel(HitResult, FirstPersonCameraComponent->GetComponentLocation(), HitEnd, ECC_Visibility, CollisionParams);
	
	UE_LOG(LogTemp, Log, TEXT("Onfire Called"));
	if (HitResult.GetActor())
	{
		UE_LOG(LogTemp, Log, TEXT("Onfire hit object %s"), *HitResult.GetActor()->GetFName().ToString());
		
		DrawDebugLine(this->GetWorld(), HitStart, HitResult.Location, FColor::Green, false, .5f, 0, 4.f);
		TArray<FHitResult> OutHits;

		UKismetSystemLibrary::SphereTraceMulti(this, HitResult.Location, HitResult.Location, 600.f, UEngineTypes::ConvertToTraceType(ECC_Visibility), false, IgnoreArray, EDrawDebugTrace::None, OutHits, true);

		for (auto& Hit : OutHits)
		{	
			
			if (Hit.GetActor()->GetRootComponent()->IsA<UStaticMeshComponent>() && Hit.GetActor()->IsRootComponentMovable())
			{
				UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>((Hit.GetActor())->GetRootComponent());
				FVector AwayFromBlastDirection = UKismetMathLibrary::GetDirectionUnitVector(HitResult.Location, MeshComp->GetComponentLocation());
				MeshComp->SetAllPhysicsLinearVelocity(MeshComp->GetComponentVelocity() + (AwayFromBlastDirection * 30.f));
				MeshComp->AddRadialImpulse(HitResult.Location, 500.f, 1600.f, ERadialImpulseFalloff::RIF_Constant, true);
				
			}
		}
	}
	**/
}
void AALSBaseCharacter::OnStopFire()
{
	StopWeaponFire();
}
void AALSBaseCharacter::StopWeaponFire()
{
	if (bWantsToFire)
	{
		bWantsToFire = false;
		if (CurrentWeapon)
		{
			CurrentWeapon->StopFire();
		}
	}
}
void AALSBaseCharacter::StartWeaponFire()
{
	if (!bWantsToFire)
	{
		bWantsToFire = true;
		if (CurrentWeapon)
		{
			CurrentWeapon->StartFire();
		}
	}
}


FVector AALSBaseCharacter::GetPlayerMovementInput() const
{
	FVector Forward = FVector::ZeroVector;
	FVector Right = FVector::ZeroVector;
	GetControlForwardRightVector(Forward, Right);
	return (Forward + Right).GetSafeNormal();
}

void AALSBaseCharacter::PlayerForwardMovementInput(float Value)
{
	if (MovementState == EALSMovementState::Grounded)
	{
		// Default camera relative movement behavior
		//const float Scale = UALSMathLibrary::FixDiagonalGamepadValues(Value, GetInputAxisValue("MoveRight/Left")).Key;
		//const FRotator DirRotator(0.0f, AimingRotation.Yaw, 0.0f);
		//AddMovementInput(UKismetMathLibrary::GetForwardVector(DirRotator), Scale);
		AddMovementInput(CameraPoll->GetForwardVector(), Value);
	}
	if (MovementState == EALSMovementState::InAir)
	{
		AddMovementInput(FirstPersonCameraComponent->GetForwardVector(), Value);
	}
}

FVector AALSBaseCharacter::GetReplicatedForward()
{
	
return UKismetMathLibrary::GetForwardVector(ReplicatedControlRotation);
}

void AALSBaseCharacter::PlayerRightMovementInput(float Value)
{
	if (MovementState == EALSMovementState::Grounded || MovementState == EALSMovementState::InAir)
	{
		// Default camera relative movement behavior
		//const float Scale = UALSMathLibrary::FixDiagonalGamepadValues(GetInputAxisValue("MoveForward/Backwards"), Value)
		//	.Value;
		//const FRotator DirRotator(0.0f, AimingRotation.Yaw, 0.0f);
		//AddMovementInput(UKismetMathLibrary::GetRightVector(DirRotator), Scale);
		AddMovementInput(CameraPoll->GetRightVector(), Value);
		//AddMovementInput(GetActorRightVector(), Value);
	}
	
}

void AALSBaseCharacter::PlayerCameraUpInput(float Value)
{
	//YawThisFrame+= 10.f * Value;
	AddControllerPitchInput(LookUpDownRate * Value);
}

void AALSBaseCharacter::PlayerCameraRightInput(float Value)
{

	//PitchThisFrame+= 10.f * Value;
	AddControllerYawInput(LookLeftRightRate * Value);
}

void AALSBaseCharacter::EquipWeapon(AWeapon* Weapon)
{
	if (Weapon)
	{
		if (GetLocalRole() == ROLE_Authority)
		{
			SetCurrentWeapon(Weapon, CurrentWeapon);
		}
		else
		{
			ServerEquipWeapon(Weapon);
		}
	}
}

bool AALSBaseCharacter::ServerEquipWeapon_Validate(AWeapon* Weapon)
{
	return true;
}

void AALSBaseCharacter::ServerEquipWeapon_Implementation(AWeapon* Weapon)
{
	EquipWeapon(Weapon);
}


void AALSBaseCharacter::OnRep_CurrentWeapon(AWeapon* LastWeapon)
{
	SetCurrentWeapon(CurrentWeapon, LastWeapon);
}


bool AALSBaseCharacter::IsTargeting() const
{
	return bIsTargeting;
}

void AALSBaseCharacter::TestActionRep()
{
	if (GetLocalRole() < ROLE_Authority)
	{
		//UE_LOG(LogClass, Warning, TEXT("TestAction Local called "));
		ServerTestActionRep();
	}
	
}
void AALSBaseCharacter::ServerTestActionRep_Implementation()
{
	UE_LOG(LogClass, Warning, TEXT("TestAction Server called "));
	//GetMesh()->SetAllBodiesBelowSimulatePhysics(FName(TEXT("Pelivs")), true, true);
	//TestActionRep();
	RagdollStart();
	
}

bool AALSBaseCharacter::ServerTestActionRep_Validate()
{	
	return true;
}


void AALSBaseCharacter::SetTargeting(bool bNewTargeting)
{
	bIsTargeting = bNewTargeting;
	

	
	//if (TargetingSound)
	//{
	//	UGameplayStatics::SpawnSoundAttached(TargetingSound, GetRootComponent());
//	}

	if (GetLocalRole() < ROLE_Authority)
	{
		ServerSetTargeting(bNewTargeting);
	}
}

bool AALSBaseCharacter::ServerSetTargeting_Validate(bool bNewTargeting)
{
	return true;
}

void AALSBaseCharacter::ServerSetTargeting_Implementation(bool bNewTargeting)
{
	SetTargeting(bNewTargeting);
}

void AALSBaseCharacter::SpawnWeaponFromPickup(APhysicsItem* PickedUpItem)
{
	if (GetLocalRole() < ROLE_Authority)
	{
		ServerSpawnWeaponFromPickup(PickedUpItem);
	}
	else
	{	
		EWeaponType WeaponType = PickedUpItem->WeaponType;
		FActorSpawnParameters SpawnParams;
		FRotator Rotation = FRotator(0.f, 0.f, 0.f);
		AWeapon* SpawnWeapon = nullptr;
		switch (WeaponType)
		{
		case EWeaponType::SingleShotTestGun:

			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnWeapon = GetWorld()->SpawnActor<ASingleShotTestGun>(TestWeaponToSpawn, GetActorLocation(), FRotator::ZeroRotator, SpawnParams);
			
			break;

		case EWeaponType::VoodooGun: 

			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnWeapon = GetWorld()->SpawnActor<AWeap_VoodooGun>(VoodooGunToSpawn, GetActorLocation(), FRotator::ZeroRotator, SpawnParams);
				
			break;
		}

		if (!SpawnWeapon->IsValidLowLevel())
		{
			UE_LOG(LogClass, Error, TEXT("SpawnWeaponFromPickup weapon is null "));
			return;
		}
		SpawnWeapon->Tags.Add("AssPiss");
		SpawnWeapon->CorrespondingPhysicsItem = PickedUpItem;
		PickedUpItem->Root->SetVisibility(false, true);
		Arsenal.Add(SpawnWeapon);
		EquipWeapon(SpawnWeapon);

		
	}
	
	
	
}
void AALSBaseCharacter::ServerSpawnWeaponFromPickup_Implementation(APhysicsItem* PickedUpItem)
{
	SpawnWeaponFromPickup(PickedUpItem);
}
bool AALSBaseCharacter::ServerSpawnWeaponFromPickup_Validate(APhysicsItem* PickedUpItem)
{
	return true;
}

void AALSBaseCharacter::SpawnWeapon(EWeaponType WeaponType)
{
	if (GetLocalRole() < ROLE_Authority)
	{
		ServerSpawnWeapon(WeaponType);
	}
	
	if (WeaponType == EWeaponType::SingleShotTestGun)
	{
		FActorSpawnParameters SpawnParams;
		//Spawn param collision does not affect in editor spawning unfortunately.
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		FRotator Rotation = FRotator(0.f, 0.f, 0.f);
		ASingleShotTestGun* TestWeapon = GetWorld()->SpawnActor<ASingleShotTestGun>(TestWeaponToSpawn, GetActorLocation(), Rotation, SpawnParams);
		TestWeapon->Tags.Add("AssPiss");

		Arsenal.Add(TestWeapon);

		EquipWeapon(TestWeapon);
		
	}
}

void AALSBaseCharacter::ServerSpawnWeapon_Implementation(EWeaponType WeaponType)
{
	SpawnWeapon(WeaponType);
}
bool AALSBaseCharacter::ServerSpawnWeapon_Validate(EWeaponType WeaponType)
{
return true;
}

bool AALSBaseCharacter::IsUseTraceValid(FHitResult InHit, FVector Start, FVector End)
{

	if (!InHit.GetActor())
	{
		return false;
	}
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(WeaponTrace), true, GetInstigator());
	TraceParams.bReturnPhysicalMaterial = false;
	FHitResult CheckHit(ForceInit);
	//StaticMap trace channel. See if we are hitting the environment
	GetWorld()->LineTraceSingleByChannel(CheckHit, Start, End, ECC_GameTraceChannel8, TraceParams);



	return (CheckHit.Distance > InHit.Distance);

}

void AALSBaseCharacter::UsePressedAction()
{
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	FHitResult Hit;
	FVector Start = FirstPersonCameraComponent->GetComponentLocation();
	FVector Forward = FirstPersonCameraComponent->GetForwardVector();
	GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + (Forward * 300.f),
		ECC_GameTraceChannel10, Params);
	
	if (Hit.GetActor() && IsUseTraceValid(Hit, Start, Forward * 300.f))
	{
		if (IUseInterface* Interface = Cast<IUseInterface>(Hit.GetActor()))
		{
			Interface->Use();
			
		}
		if (Hit.GetActor()->IsA<APhysicsItem>())
		{
			APhysicsItem* Item = Cast<APhysicsItem>(Hit.GetActor());
			EItemType ItemType = Item->ItemType;
			switch (ItemType)
			{
			case EItemType::Weapon: 
				SpawnWeaponFromPickup(Item);
				break;
			//case EStance::S_Crouching: ...
				//break;
			}
			
			
			
		}
		DrawDebugLine(GetWorld(), Start, Hit.Location, FColor::Blue, false, 1.f, 0, 8.f);
	}
}

void AALSBaseCharacter::JumpPressedAction()
{
	// Jump Action: Press "Jump Action" to end the ragdoll if ragdolling, check for a mantle if grounded or in air,
	// stand up if crouching, or jump if standing.
	

BlurBool = !BlurBool;

	if (MovementAction == EALSMovementAction::None)
	{
		if (MovementState == EALSMovementState::Grounded)
		{
			if (bHasMovementInput)
			{
				if (MantleCheckGrounded())
				{
					return;
				}
			}
			if (Stance == EALSStance::Standing)
			{
				Jump();
			}
			else if (Stance == EALSStance::Crouching)
			{
				UnCrouch();
			}
		}
		else if (MovementState == EALSMovementState::InAir)
		{
			MantleCheckFalling();
		}
		else if (MovementState == EALSMovementState::Ragdoll)
		{
			ReplicatedRagdollEnd();
		}
	}
	
}

void AALSBaseCharacter::EscMenuPressedAction()
{
	AALSPlayerController* PC = Cast<AALSPlayerController>(Controller);
	AAAADHUD* HUD = PC->GetHUD<AAAADHUD>();
	HUD->ToggleEscMenu();

}

void AALSBaseCharacter::JumpReleasedAction()
{
	StopJumping();
}

void AALSBaseCharacter::SprintPressedAction()
{
	SetDesiredGait(EALSGait::Sprinting);

	GetMesh()->SetCollisionObjectType(ECC_PhysicsBody);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	/**
	GetMesh()->SetAllBodiesBelowSimulatePhysics(FName(TEXT("Upperarm_l")), true, true);
	GetMesh()->SetAllBodiesBelowSimulatePhysics(FName(TEXT("Upperarm_r")), true, true);
	GetMesh()->SetAllBodiesBelowPhysicsBlendWeight(FName(TEXT("Upperarm_l")), .3f, true, true);
	GetMesh()->SetAllBodiesBelowPhysicsBlendWeight(FName(TEXT("Upperarm_r")), .3f, true, true);
	**/
	//GetMesh()->SetAllBodiesBelowSimulatePhysics(FName(TEXT("Clavicle_r")), true, true);
	//GetMesh()->SetAllBodiesBelowSimulatePhysics(FName(TEXT("Clavicle_l")), true, true);
	//GetMesh()->SetAllBodiesBelowPhysicsBlendWeight(FName(TEXT("Clavicle_r")), .3f, true, true);
	//GetMesh()->SetAllBodiesBelowPhysicsBlendWeight(FName(TEXT("Clavicle_l")), .3f, true, true);
	//GetMesh()->SetEnableGravity(false);


	
}

void AALSBaseCharacter::SprintReleasedAction()
{
	SetDesiredGait(EALSGait::Running);
}

void AALSBaseCharacter::AimPressedAction()
{
	// AimAction: Hold "AimAction" to enter the aiming mode, release to revert back the desired rotation mode.
	//SetRotationMode(EALSRotationMode::Aiming);

	AALSPlayerController* MyPC = Cast<AALSPlayerController>(Controller);
	if (MyPC)
	{
		if (Gait == EALSGait::Sprinting)
		{
			SetDesiredGait(EALSGait::Running);
		}
		SetTargeting(true);
	}
}

void AALSBaseCharacter::AimReleasedAction()
{
	if (ViewMode == EALSViewMode::ThirdPerson)
	{
		SetRotationMode(DesiredRotationMode);
	}
	else if (ViewMode == EALSViewMode::FirstPerson)
	{
		SetRotationMode(EALSRotationMode::LookingDirection);
	}
	SetTargeting(false);
}

void AALSBaseCharacter::CameraPressedAction()
{



	UE_LOG(LogTemp, Warning, TEXT("Camera Pressed"));
	UWorld* World = GetWorld();
	check(World);
	CameraActionPressedTime = World->GetTimeSeconds();
	GetWorldTimerManager().SetTimer(OnCameraModeSwapTimer, this,
	                                &AALSBaseCharacter::OnSwitchCameraMode, ViewModeSwitchHoldTime, false);
}

void AALSBaseCharacter::CameraReleasedAction()
{
	if (ViewMode == EALSViewMode::FirstPerson)
	{
		// Don't swap shoulders on first person mode
		return;
	}

	UWorld* World = GetWorld();
	check(World);
	if (World->GetTimeSeconds() - CameraActionPressedTime < ViewModeSwitchHoldTime)
	{
		// Switch shoulders
		SetRightShoulder(!bRightShoulder);
		GetWorldTimerManager().ClearTimer(OnCameraModeSwapTimer); // Prevent mode change
	}
}

void AALSBaseCharacter::OnSwitchCameraMode()
{
	// Switch camera mode
	if (ViewMode == EALSViewMode::FirstPerson)
	{
		SetViewMode(EALSViewMode::ThirdPerson);
	}
	else if (ViewMode == EALSViewMode::ThirdPerson)
	{
		SetViewMode(EALSViewMode::FirstPerson);
	}
}


void AALSBaseCharacter::StancePressedAction()
{
	// Stance Action: Press "Stance Action" to toggle Standing / Crouching, double tap to Roll.

	if (MovementAction != EALSMovementAction::None)
	{
		return;
	}

	UWorld* World = GetWorld();
	check(World);

	const float PrevStanceInputTime = LastStanceInputTime;
	LastStanceInputTime = World->GetTimeSeconds();

	if (LastStanceInputTime - PrevStanceInputTime <= RollDoubleTapTimeout)
	{
		// Roll
		Replicated_PlayMontage(GetRollAnimation(), 1.15f);

		if (Stance == EALSStance::Standing)
		{
			SetDesiredStance(EALSStance::Crouching);
		}
		else if (Stance == EALSStance::Crouching)
		{
			SetDesiredStance(EALSStance::Standing);
		}
		return;
	}

	if (MovementState == EALSMovementState::Grounded)
	{
		if (Stance == EALSStance::Standing)
		{
			SetDesiredStance(EALSStance::Crouching);
			Crouch();
		}
		else if (Stance == EALSStance::Crouching)
		{
			SetDesiredStance(EALSStance::Standing);
			UnCrouch();
		}
	}

	// Notice: MovementState == EALSMovementState::InAir case is removed
}

void AALSBaseCharacter::WalkPressedAction()
{
	if (DesiredGait == EALSGait::Walking)
	{
		SetDesiredGait(EALSGait::Running);
	}
	else if (DesiredGait == EALSGait::Running)
	{
		SetDesiredGait(EALSGait::Walking);
	}
}

void AALSBaseCharacter::RagdollPressedAction()
{
	// Ragdoll Action: Press "Ragdoll Action" to toggle the ragdoll state on or off.

	//MyCharacterMovementComponent->SetGravityDirection(FVector(-1.f, 0.f, 0.f));
	
	if (GetMovementState() == EALSMovementState::Ragdoll)
	{
		ReplicatedRagdollEnd();
	}
	else
	{
		ReplicatedRagdollStart();
	}
	
}

void AALSBaseCharacter::VelocityDirectionPressedAction()
{
	// Select Rotation Mode: Switch the desired (default) rotation mode to Velocity or Looking Direction.
	// This will be the mode the character reverts back to when un-aiming
	SetDesiredRotationMode(EALSRotationMode::VelocityDirection);
	SetRotationMode(EALSRotationMode::VelocityDirection);
}

void AALSBaseCharacter::LookingDirectionPressedAction()
{
	SetDesiredRotationMode(EALSRotationMode::LookingDirection);
	SetRotationMode(EALSRotationMode::LookingDirection);
}

void AALSBaseCharacter::ReplicatedRagdollStart()
{
	if (HasAuthority())
	{
		Multicast_RagdollStart();
	}
	else
	{
		Server_RagdollStart();
	}
}

void AALSBaseCharacter::ReplicatedRagdollEnd()
{
	if (HasAuthority())
	{
		Multicast_RagdollEnd(GetActorLocation());
	}
	else
	{
		Server_RagdollEnd(GetActorLocation());
	}
}

void AALSBaseCharacter::OnRep_RotationMode(EALSRotationMode PrevRotMode)
{
	OnRotationModeChanged(PrevRotMode);
}

void AALSBaseCharacter::OnRep_ViewMode(EALSViewMode PrevViewMode)
{
	OnViewModeChanged(PrevViewMode);
}

void AALSBaseCharacter::OnRep_OverlayState(EALSOverlayState PrevOverlayState)
{
	OnOverlayStateChanged(PrevOverlayState);
}

void AALSBaseCharacter::ReturnToGravity()
{
	if (GetCharacterMovement()->MovementMode == EMovementMode::MOVE_Flying)
	{
		GetMyMovementComponent()->ForceZeroG = false;
		GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		SetMovementState(EALSMovementState::Grounded);
		UE_LOG(LogTemp, Log, TEXT("ReturnToGravity"));
	}
	
}
void AALSBaseCharacter::ZeroGravTest()
{
	//UE_LOG(LogTemp, Log, TEXT("ZeroGravTest"));
	GetMyMovementComponent()->ForceZeroG = true;
	GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	SetMovementState(EALSMovementState::InAir);
	
	

	/**
	//CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionObjectType(ECC_PhysicsBody);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	
	GetMesh()->SetEnableGravity(false);
	
	GetMesh()->SetAllBodiesBelowSimulatePhysics(FName(TEXT("Thigh_l")), true, true);
	GetMesh()->SetAllBodiesBelowSimulatePhysics(FName(TEXT("Thigh_r")), true, true);
	
	GetMesh()->SetAllBodiesBelowSimulatePhysics(FName(TEXT("Clavicle_r")), true, true);
	GetMesh()->SetAllBodiesBelowSimulatePhysics(FName(TEXT("Clavicle_l")), true, true);
	
	GetMesh()->SetAllMotorsAngularDriveParams(5000, 3000.f, 0.0f, false);

	
	**/
}

void AALSBaseCharacter::UpdateDeltaPitch()
{

	
		FVector PitchForward = UKismetMathLibrary::GetForwardVector(ReplicatedControlRotation);
		FVector SlerperForward = UKismetMathLibrary::GetForwardVector(ReplicatedQuatYawRotation);
		FVector CharacterUp = GetActorUpVector();
		float DeltaQuatDot = FVector::DotProduct(PitchForward, SlerperForward);
		float DeltaQuatAcos = FMath::Acos(DeltaQuatDot);
		float DeltaQuatPitch = DeltaQuatAcos * 57.2958;

		float UpDot = FVector::DotProduct(PitchForward, CharacterUp);
		
		if (UpDot < 0)
		{
			DeltaQuatPitch *= -1.f;
		}
		DeltaPitch = DeltaQuatPitch;
	
	
}

void AALSBaseCharacter::UpdateDeltaYaw()
{

	
	FVector YawForward = UKismetMathLibrary::GetForwardVector(ReplicatedQuatYawRotation);
	FVector CharacterForward = GetActorForwardVector();
	FVector CharacterRight = GetActorRightVector();
	float DeltaQuatDot = FVector::DotProduct(YawForward, CharacterForward);
	float DeltaQuatAcos = FMath::Acos(DeltaQuatDot);
	float DeltaQuatYaw = DeltaQuatAcos * 57.2958;

	float RightDot = FVector::DotProduct(YawForward, CharacterRight);
	//if the  dot product between Quatyaw rotation and actor right vector is less than 0, we rotate right. 
	if (RightDot < 0)
	{
		DeltaQuatYaw *= -1.f;
	}

	DeltaYaw = DeltaQuatYaw;
	
}
