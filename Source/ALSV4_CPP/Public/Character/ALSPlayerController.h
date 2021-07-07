// Project:         Advanced Locomotion System V4 on C++
// Copyright:       Copyright (C) 2020 Doğa Can Yanıkoğlu
// License:         MIT License (http://www.opensource.org/licenses/mit-license.php)
// Source Code:     https://github.com/dyanikoglu/ALSV4_CPP
// Original Author: Doğa Can Yanıkoğlu
// Contributors:    


#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ALSPlayerController.generated.h"

class AALSBaseCharacter;
//class UCapsuleComponent;

/**
 * Player controller class
 */
UCLASS(Blueprintable, BlueprintType)
class ALSV4_CPP_API AALSPlayerController : public APlayerController
{
	GENERATED_BODY()

		virtual void PlayerTick(float Deltatime) override;

public:
	void OnRestartPawn(APawn* NewPawn);
	FRotator RotationOffset;
	FRotator OldRotationOffset;
	/** get gode mode cheat */
	bool HasGodMode() const;
	virtual void UpdateRotation(float DeltaTime) override;
	float PitchOffsetMax; 
	float PitchOffsetMin; 
	//virtual void UpdateCameraManager(float DeltaSeconds) override;

private:
	/** Main character reference */
	UPROPERTY()
	AALSBaseCharacter* PossessedCharacter = nullptr;

	/** god mode cheat */
	UPROPERTY(Transient)
		uint8 bGodMode : 1;
};
