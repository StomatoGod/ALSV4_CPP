// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Weap_VoodooGun.h"
#include "PhysicsObject.h"


void AWeap_VoodooGun::ToggleEntanglement()
{
	bEntanglementEnabled = !bEntanglementEnabled;
}

void AWeap_VoodooGun::FireWeapon()
{

	bool bIsTargetting = MyPawn->IsTargeting();

	
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

void AWeap_VoodooGun::HandleEntanglement()
{
	if (HasValidEntanglement())
	{
		Cast<APhysicsObject>(EntangledActors[!DominantTangleIndex])->Root->SetAllPhysicsLinearVelocity(EntangledActors[DominantTangleIndex]->GetRootComponent()->GetComponentVelocity(), true);
		//EntangledActors[!DominantTangleIndex]->GetRootComponent()->Velocity += EntangledActors[DominantTangleIndex]->GetRootComponent()->GetComponentVelocity();
		//EntangledActors[!DominantTangleIndex]->GetRootComponent()->Velocity = FVector(5000.f, 0.f, 0.f);
		UE_LOG(LogTemp, Log, TEXT("HandleEntanglement"));
	}
	else
	{
		if (EntangledActors[0] == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("EntangledActors 0 null"));
		}
		if (EntangledActors[1] == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("EntangledActors 1 null"));
		}
	}

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

	if (bEntanglementEnabled)
	{	
		HandleEntanglement();
	}



}

void AWeap_VoodooGun::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AWeap_VoodooGun, bEntanglementEnabled, COND_SkipOwner);
	//DOREPLIFETIME_CONDITION(AWeap_VoodooGun, HitNotify, COND_SkipOwner);
}