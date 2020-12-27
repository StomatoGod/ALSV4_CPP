// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "QuatPlayerCameraManager.generated.h"

/**
 * 
 */
UCLASS()
class ALSV4_CPP_API AQuatPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

	public:

		virtual bool IsGravityWalker() override;
		virtual UCameraComponent* GetGravityPlayerCameraComponent() override;
		virtual void RotateComponents(FRotator DeltaRotation) override;
};
