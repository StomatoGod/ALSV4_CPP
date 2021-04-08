// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Weapon.h"
#include "Kismet/KismetMathLibrary.h"
#include "DependencyFix/Public/PhysicsObject.h"
#include "Weap_VoodooGun.generated.h"


UENUM(BlueprintType)
enum class ETanglePair : uint8
{
	//Make sure the Prop(PhysicsObject) root isnt massless!
	PropX2,
	CharacterX2,
	//Make sure the Prop(PhysicsObject) root isnt massless!
	PropSubCharacter,
	CharacterSubProp
};

UENUM(BlueprintType)
enum class EEntanglementMode : uint8
{
	DirectionalEnergyCopy_VelocityThreshold,
	DirectionalEnergyTransfer_VelocityThreshold,
	DirectionalEnergy_Copy,
	DirectionalEnergy_Transfer,
	PotentialEnergyTransfer,
	DirectEntangle,

};




USTRUCT()
struct FVoodooCastSkip
{
	GENERATED_USTRUCT_BODY()

	
		UPROPERTY(EditAnywhere)
		AALSBaseCharacter* Character = nullptr;
	
		UPROPERTY(EditAnywhere)
		APhysicsObject* PhysicsObject = nullptr;
		 
		 public:
		 //EnergyThreshold
			// void HandleEntanglement(FVoodooCastSkip Dom, ETanglePair Pairing, EEntanglementMode EntangleMentMode, int8 EntanglementRelationship, float DeltaTime);
			 FORCEINLINE void HandleDirectEntanglement(FVoodooCastSkip Dom, ETanglePair Pairing, int8 EntanglementRelationship, float DeltaTime);
			// void HandleDirectionalEnergy_Transfer_VelocityThreshold(FVoodooCastSkip Dom, ETanglePair Pairing, int8 EntanglementRelationship, float DeltaTime);
			 FORCEINLINE void HandleDirectionalEnergy_Copy_VelocityThreshold(FVoodooCastSkip Dom, ETanglePair Pairing, int8 EntanglementRelationship, float DeltaTime);
			 //Transfers the kinetic energy directly from the Dom to the sub causing lower mass subs to move faster than the corresponding doms etc...
			 //this can be used to accelerate small objects to dangerous speeds by entangling them with slow moving yet massive objects.
			 FORCEINLINE void HandleDirectionalEnergy_Copy(FVoodooCastSkip Dom, ETanglePair Pairing, int8 EntanglementRelationship, float DeltaTime);
			 FORCEINLINE void HandleDirectionalVelocity_Copy(FVoodooCastSkip Dom, ETanglePair Pairing, int8 EntanglementRelationship, float DeltaTime);
		FORCEINLINE FVector GetPhysicsObjectVelocity();
		FORCEINLINE float GetPhysicsObjectMass();
};
FORCEINLINE FVector FVoodooCastSkip::GetPhysicsObjectVelocity()
{
	return PhysicsObject->Root->GetComponentVelocity();
}
FORCEINLINE float FVoodooCastSkip::GetPhysicsObjectMass()
{
	
	return PhysicsObject->Root->GetMass();
}


FORCEINLINE void FVoodooCastSkip::HandleDirectionalEnergy_Copy(FVoodooCastSkip Dom, ETanglePair Pairing, int8 EntanglementRelationship, float DeltaTime)
{
	switch (Pairing)
	{
	case ETanglePair::PropX2:
		
		float Scalar = .5f;
		FVector SubVelocity = GetPhysicsObjectVelocity();
		FVector DomVelocity = Dom.GetPhysicsObjectVelocity();
		float DomSpeed = DomVelocity.Size() * Scalar;
		float SubSpeed = SubVelocity.Size() * Scalar;
		//float RelativeSpeed = FMath::Abs((SubDirection - DomDirection).Size());
		float DirectionDot = SubVelocity.GetSafeNormal() | DomVelocity.GetSafeNormal();
		float DomKineticEnergy = .5f * (Dom.GetPhysicsObjectMass() * Scalar)  * (DomSpeed * DomSpeed);
		float SubKineticEnergy = .5f * (GetPhysicsObjectMass() * Scalar) * (SubSpeed * SubSpeed);
		float EnergyDifferential = FMath::Clamp(SubKineticEnergy / DomKineticEnergy, 0.f, 1.f);
		float MassMultiplier = Dom.GetPhysicsObjectMass() / GetPhysicsObjectMass();
		FVector ScaledForce = DomVelocity * MassMultiplier;
		FVector Force = ScaledForce - (ScaledForce * EnergyDifferential * FMath::Clamp(DirectionDot, 0.f, 1.f));
		
		UE_LOG(LogTemp, Log, TEXT("HandleDirectionalEnergy_Copy Force: %s, SafeNormalForce: %s"), *Force.ToString(), *Force.GetSafeNormal().ToString());
		UE_LOG(LogTemp, Log, TEXT("HandleDirectionalEnergy_Copy DomMass: %f, Submass: %f, MassMultiplier: %f, EnergyDifferential: %f"), Dom.GetPhysicsObjectMass(), GetPhysicsObjectMass(), MassMultiplier, EnergyDifferential);
		PhysicsObject->Root->AddForce(Force * float(EntanglementRelationship), NAME_None, true);

		break;

//	case ETanglePair::CharacterX2:



	//	break;

//	case ETanglePair::PropSubCharacter:



	//	break;

	//case ETanglePair::CharacterSubPRop:



	//	break;
	}
}

FORCEINLINE void FVoodooCastSkip::HandleDirectionalVelocity_Copy(FVoodooCastSkip Dom, ETanglePair Pairing, int8 EntanglementRelationship, float DeltaTime)
{
	switch (Pairing)
	{
	case ETanglePair::PropX2:

		float Scalar = .01f;
		FVector SubVelocity = GetPhysicsObjectVelocity();
		FVector DomVelocity = Dom.GetPhysicsObjectVelocity();
		float DomSpeed = DomVelocity.Size() * Scalar;
		float SubSpeed = SubVelocity.Size() * Scalar;
		FVector SubDirection = UKismetMathLibrary::GetDirectionUnitVector(FVector::ZeroVector, FVector::ZeroVector + SubVelocity);
		FVector DomDirection = UKismetMathLibrary::GetDirectionUnitVector(FVector::ZeroVector, FVector::ZeroVector + DomVelocity);
		float RelativeSpeed = FMath::Abs((SubDirection - DomDirection).Size());
		FVector SubVelocityRelativeToDom = UKismetMathLibrary::GetDirectionUnitVector(SubVelocity, DomVelocity) * RelativeSpeed;
		float DirectionDot = SubDirection | DomDirection;
		float DomKineticEnergy = .5f * (Dom.GetPhysicsObjectMass() * Scalar) * (DomSpeed * DomSpeed);
		float SubKineticEnergy = .5f * (GetPhysicsObjectMass() * Scalar) * (SubSpeed * SubSpeed);
		SubKineticEnergy = FMath::Clamp(SubKineticEnergy, 1.f, DomKineticEnergy + 1.f);
		DomKineticEnergy = FMath::Clamp(DomKineticEnergy, 1.f, DomKineticEnergy + 1.f);
		float EnergyDifferential = FMath::Clamp(DomKineticEnergy / SubKineticEnergy, 0.f, 1.f);
		//FVector Force =  DomVelocity - (DomVelocity * (DomKineticEnergy / SubKineticEnergy) * DirectionDot).GetSafeNormal();
		float MassMultiplier = Dom.GetPhysicsObjectMass() / GetPhysicsObjectMass();
		FVector Force = DomVelocity;
		//Force = Force.GetSafeNormal();
		//Force *=   (Dom.GetPhysicsObjectMass() /  GetPhysicsObjectMass()) * ((DomKineticEnergy / SubKineticEnergy));
		UE_LOG(LogTemp, Log, TEXT("HandleDirectionalEnergy_Copy Force: %s, SafeNormalForce: %s"), *Force.ToString(), *Force.GetSafeNormal().ToString());


		//UE_LOG(LogTemp, Log, TEXT("HandleDirectionalEnergy_Copy DomKineticEnergy: %f"), DomKineticEnergy);
		//UE_LOG(LogTemp, Log, TEXT("HandleDirectionalEnergy_Copy SubKineticEnergy: %f"), SubKineticEnergy);
		//PhysicsObject->Root->SetAllPhysicsLinearVelocity(SubVelocity + VelocityDifferential, false);
		UE_LOG(LogTemp, Log, TEXT("HandleDirectionalEnergy_Copy DomMass: %f, Submass: %f, MassMultiplier: %f, EnergyDifferential: %f"), Dom.GetPhysicsObjectMass(), GetPhysicsObjectMass(), MassMultiplier, EnergyDifferential);
		PhysicsObject->Root->AddForce(Force, NAME_None, true);

		break;

		//	case ETanglePair::CharacterX2:



			//	break;

		//	case ETanglePair::PropSubCharacter:



			//	break;

			//case ETanglePair::CharacterSubPRop:



			//	break;
	}
}


FORCEINLINE void FVoodooCastSkip::HandleDirectionalEnergy_Copy_VelocityThreshold(FVoodooCastSkip Dom, ETanglePair Pairing, int8 EntanglementRelationship, float DeltaTime)
{
	switch (Pairing)
	{
	case ETanglePair::PropX2:

		FVector SubVelocity = GetPhysicsObjectVelocity();
		FVector DomVelocity = Dom.GetPhysicsObjectVelocity();
		float DomSpeed = DomVelocity.Size();
		float SubSpeed = SubVelocity.Size();
		FVector SubDirection = UKismetMathLibrary::GetDirectionUnitVector(FVector::ZeroVector, FVector::ZeroVector + SubVelocity);
		FVector DomDirection = UKismetMathLibrary::GetDirectionUnitVector(FVector::ZeroVector, FVector::ZeroVector + DomVelocity);
		float RelativeSpeed = FMath::Abs((SubDirection - DomDirection).Size());
		FVector SubVelocityRelativeToDom = UKismetMathLibrary::GetDirectionUnitVector(DomVelocity, SubVelocity) * RelativeSpeed;
		float DirectionDot = SubVelocity | DomVelocity;
		float DomKineticEnergy = .5f * Dom.GetPhysicsObjectMass() * DomSpeed * DomSpeed;
		FVector VelocityDifferential = Dom.GetPhysicsObjectVelocity() - PhysicsObject->Root->GetComponentVelocity();
		PhysicsObject->Root->SetAllPhysicsLinearVelocity(SubVelocity + VelocityDifferential, false);


		break;

	//case ETanglePair::CharacterX2:



		//break;
	}
}
FORCEINLINE void FVoodooCastSkip::HandleDirectEntanglement(FVoodooCastSkip Dom, ETanglePair Pairing, int8 EntanglementRelationship, float DeltaTime)
{
	
		switch (Pairing)
		{
		case ETanglePair::PropX2:

			FVector SubVelocity = GetPhysicsObjectVelocity();
			FVector DomVelocity = Dom.GetPhysicsObjectVelocity();
			FVector SubDirection = UKismetMathLibrary::GetDirectionUnitVector(FVector::ZeroVector, FVector::ZeroVector + SubVelocity);
			FVector DomDirection = UKismetMathLibrary::GetDirectionUnitVector(FVector::ZeroVector, FVector::ZeroVector + DomVelocity);
			float RelativeSpeed = FMath::Abs((SubDirection - DomDirection).Size());
			float DirectionDot = SubVelocity | DomVelocity;
			float InertialScalar = Dom.GetPhysicsObjectMass() / GetPhysicsObjectMass();
			FVector VelocityDifferential = Dom.GetPhysicsObjectVelocity() - PhysicsObject->Root->GetComponentVelocity();
			PhysicsObject->Root->SetAllPhysicsLinearVelocity(SubVelocity + VelocityDifferential, false);
			

			break;

	//	case ETanglePair::CharacterX2:

			

			//break;
		}

	
}


USTRUCT()
struct FVoodooWeaponData
{
	GENERATED_USTRUCT_BODY()

		/** weapon range */
		UPROPERTY(EditDefaultsOnly, Category = WeaponStat)
		float WeaponRange;

	/** damage amount */
	UPROPERTY(EditDefaultsOnly, Category = WeaponStat)
		int32 HitDamage;

	/** type of damage */
	UPROPERTY(EditDefaultsOnly, Category = WeaponStat)
		TSubclassOf<UDamageType> DamageType;

	/** hit verification: scale for bounding box of hit actor */
	UPROPERTY(EditDefaultsOnly, Category = HitVerification)
		float ClientSideHitLeeway;

	/** hit verification: threshold for dot product between view direction and hit direction */
	UPROPERTY(EditDefaultsOnly, Category = HitVerification)
		float AllowedViewDotHitDir;


	/** defaults */
	FVoodooWeaponData()
	{
		WeaponRange = 10000.0f;
		HitDamage = 10;
		DamageType = UDamageType::StaticClass();
		ClientSideHitLeeway = 200.0f;
		AllowedViewDotHitDir = 0.8f;
	}
};



UENUM(BlueprintType)
enum class EVoodooMode : uint8
{
	Entangle,
	Hl2,
};


//class AALSBaseCharacter;
class APhysicsObject;
UCLASS()
class ALSV4_CPP_API AWeap_VoodooGun : public AWeapon
{
	GENERATED_BODY()
	
		
	
public:
	virtual void Tick(float DeltaTime) override;

	void ToggleEntanglement();
	//set to 1 or -1 one. Makes sub follow dom through inverted forces or not
	static const float BaseBatteryPower;
	
	int8 EntanglementRelationship = 1;
	uint8 LastEntangledIndex = 1;
	uint8* LastEntangledIndexPointer = &LastEntangledIndex;
	uint8 TestIndex = *LastEntangledIndexPointer;
	
	uint8 DominantTangleIndex = 0;


	
		AActor* LastEntangledActor = nullptr;

		AActor* OtherActor = nullptr;

		FVoodooCastSkip NullCastSkip;
		FVoodooCastSkip GravityGunActor;
	
	TArray<FVoodooCastSkip> CastSkips =
	{ NullCastSkip, NullCastSkip,

	}; 

	UPROPERTY(Transient, Replicated)
	TArray<AActor*> EntangledActors =
	{ LastEntangledActor, OtherActor,

	};



	UPROPERTY(EditDefaultsOnly, Category = Config)
	FVoodooWeaponData VoodooConfig;

	UPROPERTY(EditAnywhere, Replicated)
		EVoodooMode VoodooMode = EVoodooMode::Entangle;

	ETanglePair TanglePairMode = ETanglePair::PropX2;
	
protected:
	/** [local] weapon specific fire implementation */
	float AvailablePower;

	virtual void FireWeapon() override;
	void SuckySucky();
	
	FHitResult GravityTrace(const FVector& StartTrace, const FVector& EndTrace) const;
	virtual bool IsTraceValid(FHitResult InHit, FVector Start, FVector End) override;
	void HandleEntanglement(float DeltaTime);
	void HandleGravityGun(float DeltaTime);
	

	void HandleGravityGunOnServer(float DeltaTime);
	void EntangleObject(AActor* HitObject);
	void FlipDominantTangleBuddy();
	void ClearEntanglements();
	bool HasValidEntanglement();
	virtual void SwitchWeaponMode() override;
	UPROPERTY(VisibleAnywhere, Replicated)
	bool bEntanglementEnabled = false;


	float GravityGunStrength = 40000.f;



	IPhysicsInterface* CachedPhysicsInterface = nullptr;
	
	UPROPERTY(VisibleAnywhere, Replicated)
	AActor* CachedPhysicsActor = nullptr;
	
	UPROPERTY(VisibleAnywhere, Replicated)
	bool bDetectingPhysObject = false;
};
