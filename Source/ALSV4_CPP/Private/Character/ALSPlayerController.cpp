// Project:         Advanced Locomotion System V4 on C++
// Copyright:       Copyright (C) 2020 Doğa Can Yanıkoğlu
// License:         MIT License (http://www.opensource.org/licenses/mit-license.php)
// Source Code:     https://github.com/dyanikoglu/ALSV4_CPP
// Original Author: Doğa Can Yanıkoğlu
// Contributors:    


#include "Character/ALSPlayerController.h"
#include "Character/ALSCharacter.h"
#include "Character/ALSPlayerCameraManager.h"
#include "DependencyFix/Public/UI/AAADHUD.h"
#include "Character/QuatPlayerCameraManager.h"

void AALSPlayerController::OnRestartPawn(APawn* NewPawn)
{
	PossessedCharacter = Cast<AALSBaseCharacter>(NewPawn);
	check(PossessedCharacter);

	

	// Call "OnPossess" in Player Camera Manager when possessing a pawn

	/**
	AALSPlayerCameraManager* CastedMgr = Cast<AALSPlayerCameraManager>(PlayerCameraManager);
	if (CastedMgr)
	{
		CastedMgr->OnPossess(PossessedCharacter);
	}
	**/
	AQuatPlayerCameraManager* CastedMgr = Cast<AQuatPlayerCameraManager>(PlayerCameraManager);
	if (CastedMgr)
	{
		CastedMgr->SetViewTarget(GetPawn());

	}
}
void AALSPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	// [Client] Update Rotation

	AALSCharacter* const Dude = Cast<AALSCharacter>(GetPawn());
	if (Dude != nullptr)
	{
		//UE_LOG(LogClass, Error, TEXT(" Player Controller: RotationInput.YAW: %f"), RotationInput.Yaw)
		//UpdateMouseLook(DeltaTime);

		//	UE_LOG(LogClass, Error, TEXT(" RotationInput.YAW: %f"), RotationInput.Yaw);
		//UpdateRotation(DeltaTime);
	}
	//PlayerCameraManager->bUseClientSideCameraUpdates = true;
	//bUseClientSideCameraUpdates = true;
}



bool AALSPlayerController::HasGodMode() const
{
	return bGodMode;
}

void AALSPlayerController::UpdateRotation(float DeltaTime)
{
	// Calculate Delta to be applied on ViewRotation
	FRotator DeltaRot(RotationInput);

	FRotator OldOffsetAndOutControlRot = RotationOffset;

	if (PlayerCameraManager)
	{
		PlayerCameraManager->ProcessViewRotation(DeltaTime, RotationOffset, DeltaRot);
	}
	
	PossessedCharacter->UpdateCameraRotation(RotationOffset, OldOffsetAndOutControlRot, DeltaTime);
	
	SetControlRotation(OldOffsetAndOutControlRot);
	
	


	
}

