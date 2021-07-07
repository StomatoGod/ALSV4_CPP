// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Weap_VoodooGun.h"



void AWeap_VoodooGun::ToggleEntanglement()
{
	bEntanglementEnabled = !bEntanglementEnabled;
}

void AWeap_VoodooGun::FireWeapon()
{

	bool bIsTargetting = MyPawn->IsTargeting();
VoodooMode = EVoodooMode(uint8(bIsTargetting));
	
		const FVector StartTrace = MyPawn->GetFirstPersonCamera()->GetComponentLocation();
		const FVector ShootDir = MyPawn->GetFirstPersonCamera()->GetForwardVector();
		const FVector EndTrace = StartTrace + (ShootDir * VoodooConfig.WeaponRange);
		const FHitResult Hit = WeaponTrace(StartTrace, EndTrace);

		if (Hit.GetActor())
		{	
			if (Hit.GetActor()->IsA<APhysicsObject>())
			{
				APhysicsObject* HitObject = Cast<APhysicsObject>(Hit.GetActor());
				if (VoodooMode == EVoodooMode::Entangle)
				{
					EntangleObject(HitObject);
				}
				
			}
			
		}
	

}

void AWeap_VoodooGun::SuckySucky()
{

}

FHitResult AWeap_VoodooGun::GravityTrace(const FVector& StartTrace, const FVector& EndTrace) const
{
	// Perform trace to retrieve hit info
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(WeaponTrace), true, GetInstigator());
	TraceParams.bReturnPhysicalMaterial = false;

	FHitResult Hit(ForceInit);
	GetWorld()->LineTraceSingleByChannel(Hit, StartTrace, EndTrace, ECC_GameTraceChannel12, TraceParams);

	return Hit;
	
}

void AWeap_VoodooGun::HandleGravityGun(float DeltaTime)
{
	const FVector StartTrace = MyPawn->GetFirstPersonCamera()->GetComponentLocation();
	const FVector ShootDir = MyPawn->GetFirstPersonCamera()->GetForwardVector();
	const FVector EndTrace = StartTrace + (ShootDir * VoodooConfig.WeaponRange);
	const FHitResult Hit = GravityTrace(StartTrace, EndTrace);
	
	
	if (!Hit.GetActor())
	{
		bDetectingPhysObject = false;
		return;
	}
	else
	{
		AActor* HitActor = Hit.GetActor();
		bDetectingPhysObject = true;

		if (HitActor == CachedPhysicsActor)
		{

			if (MyPawn->IsTargeting())
			{
				//CachedPhysicsInterface->Gravitate(StartTrace + (ShootDir * 100.f), Hit.Location, 1.f, GravityGunStrength, false);
			}

		}
		else
		{
			CachedPhysicsActor = HitActor;
			CachedPhysicsInterface = Cast<IPhysicsInterface>(HitActor);
			if (MyPawn->IsTargeting())
			{
			
				//CachedPhysicsInterface->Gravitate(StartTrace + (ShootDir * 100.f), Hit.Location, 1.f, GravityGunStrength, false);
			}
		}

	}
	

	
}
bool AWeap_VoodooGun::IsTraceValid(FHitResult InHit, FVector Start, FVector End)
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

// We do not need to do this through an rpc because character movement is already replicated and Smoothsync handles replication of physics objects
//Do on server only
void AWeap_VoodooGun::HandleGravityGunOnServer(float DeltaTime)
{
	
	const FVector StartTrace = MyPawn->GetFirstPersonCamera()->GetComponentLocation();
	const FVector ShootDir = MyPawn->GetReplicatedForward();
	const FVector EndTrace = StartTrace + (ShootDir * VoodooConfig.WeaponRange);
	const FHitResult Hit = GravityTrace(StartTrace, EndTrace);

	
	//UE_LOG(LogClass, Log, TEXT(" HandleGravityGunOnServer Called"));
	//DrawDebugLine(this->GetWorld(), StartTrace, EndTrace, FColor::Red, false, 2.f, 0, 10.f);
	if (!IsTraceValid(Hit, StartTrace, EndTrace))
	{
		//UE_LOG(LogClass, Warning, TEXT(" IsTraceValid false"));
		bDetectingPhysObject = false;
		return;
	}
	
		AActor* HitActor = Hit.GetActor();
		bDetectingPhysObject = true;
		
		// ArrayDataA = IfAThenAElseB(ArrayA.GetData(), &EmptyArrayData);
		if (HitActor == CachedPhysicsActor)
		{
				CachedPhysicsInterface->Gravitate(StartTrace + (ShootDir * 100.f), Hit.Location, 1.f, GravityGunStrength);

		}
		else
		{
			CachedPhysicsActor = HitActor;
			CachedPhysicsInterface = Cast<IPhysicsInterface>(HitActor);
				CachedPhysicsInterface->Gravitate(StartTrace + (ShootDir * 100.f), Hit.Location, 1.f, GravityGunStrength);
		}

	
	/**
	if (CachedPhysicsActor && MyPawn->IsTargeting() && bDetectingPhysObject)
	{
		const FVector StartTrace = MyPawn->GetFirstPersonCamera()->GetComponentLocation();
		const FVector ShootDir = MyPawn->GetFirstPersonCamera()->GetForwardVector();
		const FVector EndTrace = StartTrace + (ShootDir * VoodooConfig.WeaponRange);
		const FHitResult Hit = GravityTrace(StartTrace, EndTrace);

		CachedPhysicsInterface = Cast<IPhysicsInterface>(CachedPhysicsActor);
		CachedPhysicsInterface->Gravitate(StartTrace + (ShootDir * 100.f), Hit.Location, 1.f, GravityGunStrength, true);

		
	}
	**/

}

void AWeap_VoodooGun::HandleEntanglement(float DeltaTime)
{
		CastSkips[!DominantTangleIndex].HandleDirectionalEnergy_Copy(CastSkips[DominantTangleIndex], TanglePairMode, EntanglementRelationship, DeltaTime);
}

void AWeap_VoodooGun::EntangleObject(AActor* HitObject)
{
	if (EntangledActors.Contains(HitObject))
	{
		if (HitObject->IsA<APhysicsObject>())
		{
			APhysicsObject* PhysObject = Cast<APhysicsObject>(HitObject);
			int8 NullIndex = EntangledActors.Find(PhysObject);
			EntangledActors[NullIndex] = nullptr;
		}
	}
	else if (HitObject->IsA<APhysicsObject>())
	{
		int8 NewEntangleIndex = !LastEntangledIndex;
		APhysicsObject* PhysObject = Cast<APhysicsObject>(HitObject);
		EntangledActors[NewEntangleIndex] = PhysObject;
		LastEntangledIndex = NewEntangleIndex;

		UE_LOG(LogTemp, Log, TEXT("EntangleObject"));
	}

	if (!HasValidEntanglement())
	{
		return;
	}
	float PhysicsObjectCount = 0.f;
	float CharacterCount = 0.f;
	float DomValue = 0.f;
	for (int32 CurrentIndex = 0; CurrentIndex < EntangledActors.Num(); CurrentIndex++)
	{
		
		if (EntangledActors[CurrentIndex]->IsA<APhysicsObject>())
		{
			CastSkips[CurrentIndex].PhysicsObject = Cast<APhysicsObject>(EntangledActors[CurrentIndex]);
			PhysicsObjectCount += .6f;
			if (DominantTangleIndex == CurrentIndex)
			{
				DomValue += 2.f;
			}
		}
		if (EntangledActors[CurrentIndex]->IsA<AALSBaseCharacter>())
		{
			CastSkips[CurrentIndex].Character = Cast<AALSBaseCharacter>(EntangledActors[CurrentIndex]);
			CharacterCount += 1.f;
			if (DominantTangleIndex == CurrentIndex)
			{
				DomValue += .4f;
			}
		}
	}

	if (CharacterCount + PhysicsObjectCount <= 1.f)
	{
		return;
	}


	float PairValue = 0.f;
	if (CharacterCount > 0.f && PhysicsObjectCount > 0.f)
	{
		PairValue += PhysicsObjectCount + CharacterCount + DomValue;
	}
	else
	{
		PairValue += PhysicsObjectCount + CharacterCount;
	}
	uint8 PairEnumValue = FGenericPlatformMath::RoundToInt(PairValue) - 1;
	
	TanglePairMode = ETanglePair(PairEnumValue);
	bEntanglementEnabled = true;
	
}

void AWeap_VoodooGun::FlipDominantTangleBuddy()
{
	DominantTangleIndex = !DominantTangleIndex;
}

void AWeap_VoodooGun::ClearEntanglements()
{
	EntangledActors[0] = nullptr;
	EntangledActors[1] = nullptr;

}

bool AWeap_VoodooGun::HasValidEntanglement()
{	
	
	return (EntangledActors[0] != nullptr && EntangledActors[1] != nullptr);
}

void AWeap_VoodooGun::SwitchWeaponMode()
{
	VoodooMode = EVoodooMode(!uint8(VoodooMode));	
}

void AWeap_VoodooGun::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	
	
	
	if (HasAuthority())
	{
		if (MyPawn->IsTargeting())
		{
			HandleGravityGunOnServer(DeltaTime);
		}
		if (bEntanglementEnabled && HasValidEntanglement())
		{
			HandleEntanglement(DeltaTime);
		}
		
	}
	//else if (((MyPawn != NULL) && (MyPawn->IsLocallyControlled() == true)))
	//{
	//	HandleGravityGun(DeltaTime);
	//}



}

void AWeap_VoodooGun::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AWeap_VoodooGun, bEntanglementEnabled, COND_SkipOwner);
	//DOREPLIFETIME_CONDITION(AWeap_VoodooGun, CachedPhysicsActor, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AWeap_VoodooGun, bDetectingPhysObject, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AWeap_VoodooGun, EntangledActors, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AWeap_VoodooGun, VoodooMode, COND_SkipOwner);
	//DOREPLIFETIME_CONDITION(AWeap_VoodooGun, HitNotify, COND_SkipOwner);
}


ALSV4_CPP_API const float AWeap_VoodooGun::BaseBatteryPower(200.f);