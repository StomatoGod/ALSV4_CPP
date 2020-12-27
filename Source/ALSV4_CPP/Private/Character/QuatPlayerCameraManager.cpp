// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/QuatPlayerCameraManager.h"
#include "Character/ALSBaseCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"



bool AQuatPlayerCameraManager::IsGravityWalker()
{
 return true;
}

UCameraComponent* AQuatPlayerCameraManager::GetGravityPlayerCameraComponent()
{
	AALSBaseCharacter* Character = Cast<AALSBaseCharacter>(GetViewTargetPawn());
	
		return Character->GetFirstPersonCamera();
	
}

void AQuatPlayerCameraManager::RotateComponents(FRotator DeltaRotation)
{
	FQuat DeltaQuatPitch = FRotator(DeltaRotation.Pitch, 0.f, 0.f).Quaternion();
	FQuat DeltaQuatYaw = FRotator(0.f, DeltaRotation.Yaw, 0.f).Quaternion();


	
	AALSBaseCharacter* Character = Cast<AALSBaseCharacter>(GetViewTargetPawn());
	Character->GetCameraPoll()->AddLocalRotation(DeltaQuatYaw);
	Character->GetFirstPersonCamera()->AddLocalRotation(DeltaQuatPitch);
}