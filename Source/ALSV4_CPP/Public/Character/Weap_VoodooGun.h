// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Weapon.h"
#include "Weap_VoodooGun.generated.h"


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
	Hl2,
	Entangle,
};



class APhysicsObject;
UCLASS()
class ALSV4_CPP_API AWeap_VoodooGun : public AWeapon
{
	GENERATED_BODY()
	
		
public:
	virtual void Tick(float DeltaTime) override;

	void ToggleEntanglement();
	uint8 LastEntangledIndex = 1;
	//int8* LastEntangledIndexPointer = &LastEntangledIndex;
	
	
	uint8 DominantTangleIndex = 0;

		AActor* LastEntangledActor = nullptr;

		AActor* OtherActor = nullptr;

	UPROPERTY(Transient, Replicated)
	TArray<AActor*> EntangledActors =
	{ LastEntangledActor, OtherActor,

	}; 



	UPROPERTY(EditDefaultsOnly, Category = Config)
	FVoodooWeaponData VoodooConfig;

	UPROPERTY(EditAnywhere, Replicated)
		EVoodooMode VoodooMode = EVoodooMode::Entangle;
	
protected:
	/** [local] weapon specific fire implementation */
	virtual void FireWeapon() override;
	void SuckySucky();
	void HandleEntanglement();
	void EntangleObject(AActor* HitObject);
	void FlipDominantTangleBuddy();
	void ClearEntanglements();
	bool HasValidEntanglement();
	virtual void SwitchWeaponMode() override;
	UPROPERTY(Replicated)
	bool bEntanglementEnabled = true;
};
