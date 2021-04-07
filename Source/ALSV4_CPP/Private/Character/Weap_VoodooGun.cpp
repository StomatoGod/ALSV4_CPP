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
				else
				{
					GravityGunActor.PhysicsObject = Cast<APhysicsObject>(Hit.GetActor());
				}
			}
			
		}
	

}

void AWeap_VoodooGun::SuckySucky()
{

}

void AWeap_VoodooGun::HandleGravityGun(float DeltaTime)
{
	
	if (MyPawn->IsTargeting() && GravityGunActor.PhysicsObject->IsValidLowLevelFast())
	{
		FVector DirectionToMe = UKismetMathLibrary::GetDirectionUnitVector(GravityGunActor.PhysicsObject->GetActorLocation(), MyPawn->GetActorLocation());
		float Distance = GravityGunActor.PhysicsObject->GetDistanceTo(MyPawn);
		FVector Force = DirectionToMe * 200.f;
		//GravityGunActor.PhysicsObject->Root->SetAllPhysicsLinearVelocity(SubVelocity + VelocityDifferential, false);
		GravityGunActor.PhysicsObject->Root->AddForce(Force, NAME_None, true);
	}
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

	if (bEntanglementEnabled && HasValidEntanglement())
	{	
		HandleEntanglement(DeltaTime);
	}
	
	HandleGravityGun(DeltaTime);


}

void AWeap_VoodooGun::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AWeap_VoodooGun, bEntanglementEnabled, COND_SkipOwner);
	//DOREPLIFETIME_CONDITION(AWeap_VoodooGun, HitNotify, COND_SkipOwner);
}


ALSV4_CPP_API const float AWeap_VoodooGun::BaseBatteryPower(200.f);