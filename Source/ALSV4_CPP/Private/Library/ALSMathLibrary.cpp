// Project:         Advanced Locomotion System V4 on C++
// Copyright:       Copyright (C) 2020 Doğa Can Yanıkoğlu
// License:         MIT License (http://www.opensource.org/licenses/mit-license.php)
// Source Code:     https://github.com/dyanikoglu/ALSV4_CPP
// Original Author: Doğa Can Yanıkoğlu
// Contributors:    


#include "Library/ALSMathLibrary.h"

#include "Components/CapsuleComponent.h"
#include "Library/ALSCharacterStructLibrary.h"

FTransform UALSMathLibrary::MantleComponentLocalToWorld(const FALSComponentAndTransform& CompAndTransform)
{
	const FTransform& InverseTransform = CompAndTransform.Component->GetComponentToWorld().Inverse();
	const FVector Location = InverseTransform.InverseTransformPosition(CompAndTransform.Transform.GetLocation());
	const FQuat Quat = InverseTransform.InverseTransformRotation(CompAndTransform.Transform.GetRotation());
	const FVector Scale = InverseTransform.InverseTransformPosition(CompAndTransform.Transform.GetScale3D());
	return {Quat, Location, Scale};
}

TPair<float, float> UALSMathLibrary::FixDiagonalGamepadValues(const float Y, const float X)
{
	float ResultY = Y * FMath::GetMappedRangeValueClamped(FVector2D(0.0f, 0.6f),
	                                                      FVector2D(1.0f, 1.2f), FMath::Abs(X));
	ResultY = FMath::Clamp(ResultY, -1.0f, 1.0f);
	float ResultX = X * FMath::GetMappedRangeValueClamped(FVector2D(0.0f, 0.6f),
	                                                      FVector2D(1.0f, 1.2f), FMath::Abs(Y));
	ResultX = FMath::Clamp(ResultX, -1.0f, 1.0f);
	return TPair<float, float>(ResultY, ResultX);
}

FVector UALSMathLibrary::GetCapsuleBaseLocation(const float ZOffset, UCapsuleComponent* Capsule)
{
	return Capsule->GetComponentLocation() -
		Capsule->GetUpVector() * (Capsule->GetScaledCapsuleHalfHeight() + ZOffset);
}

FVector UALSMathLibrary::GetCapsuleLocationFromBase(FVector BaseLocation, const float ZOffset,
                                                    UCapsuleComponent* Capsule)
{
	BaseLocation.Z += Capsule->GetScaledCapsuleHalfHeight() + ZOffset;
	return BaseLocation;
}

bool UALSMathLibrary::CapsuleHasRoomCheck(UCapsuleComponent* Capsule, FVector TargetLocation, float HeightOffset,
                                          float RadiusOffset)
{
	// Perform a trace to see if the capsule has room to be at the target location.
	const float ZTarget = Capsule->GetScaledCapsuleHalfHeight_WithoutHemisphere() - RadiusOffset + HeightOffset;
	FVector TraceStart = TargetLocation;
	TraceStart.Z += ZTarget;
	FVector TraceEnd = TargetLocation;
	TraceEnd.Z -= ZTarget;
	const float Radius = Capsule->GetUnscaledCapsuleRadius() + RadiusOffset;

	const UWorld* World = Capsule->GetWorld();
	check(World);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Capsule->GetOwner());

	FHitResult HitResult;
	World->SweepSingleByProfile(HitResult, TraceStart, TraceEnd, FQuat::Identity,
	                            FName(TEXT("ALS_Character")), FCollisionShape::MakeSphere(Radius), Params);

	return !(HitResult.bBlockingHit || HitResult.bStartPenetrating);
}

bool UALSMathLibrary::AngleInRange(float Angle, float MinAngle, float MaxAngle, float Buffer, bool IncreaseBuffer)
{
	if (IncreaseBuffer)
	{
		return Angle >= MinAngle - Buffer && Angle <= MaxAngle + Buffer;
	}
	return Angle >= MinAngle + Buffer && Angle <= MaxAngle - Buffer;
}

EALSMovementDirection UALSMathLibrary::CalculateQuadrant(EALSMovementDirection Current, float FRThreshold,
                                                         float FLThreshold,
                                                         float BRThreshold, float BLThreshold, float Buffer,
                                                         float Angle)
{
	// Take the input angle and determine its quadrant (direction). Use the current Movement Direction to increase or
	// decrease the buffers on the angle ranges for each quadrant.
	if (AngleInRange(Angle, FLThreshold, FRThreshold, Buffer,
	                 Current != EALSMovementDirection::Forward || Current != EALSMovementDirection::Backward))
	{
		return EALSMovementDirection::Forward;
	}

	if (AngleInRange(Angle, FRThreshold, BRThreshold, Buffer,
	                 Current != EALSMovementDirection::Right || Current != EALSMovementDirection::Left))
	{
		return EALSMovementDirection::Right;
	}

	if (AngleInRange(Angle, BLThreshold, FLThreshold, Buffer,
	                 Current != EALSMovementDirection::Right || Current != EALSMovementDirection::Left))
	{
		return EALSMovementDirection::Left;
	}

	return EALSMovementDirection::Backward;
}

FRotator UALSMathLibrary::RInterpConstantTo(const FRotator& Current, const FRotator& Target, float DeltaTime, float InterpSpeed)
{
	// if DeltaTime is 0, do not perform any interpolation (Location was already calculated for that frame)
	if (DeltaTime == 0.f || Current == Target)
	{
		return Current;
	}

	// If no interp speed, jump to target value
	if (InterpSpeed <= 0.f)
	{
		return Target;
	}

	const float DeltaInterpSpeed = InterpSpeed * DeltaTime;
	
	FQuat Slerp = FQuat::Slerp(Current.Quaternion(), Target.Quaternion(), DeltaInterpSpeed);
	const FRotator DeltaMove = (Target - Current).GetNormalized();
	FRotator Result = Current;
	Result.Pitch += FMath::Clamp(DeltaMove.Pitch, -DeltaInterpSpeed, DeltaInterpSpeed);
	Result.Yaw += FMath::Clamp(DeltaMove.Yaw, -DeltaInterpSpeed, DeltaInterpSpeed);
	Result.Roll += FMath::Clamp(DeltaMove.Roll, -DeltaInterpSpeed, DeltaInterpSpeed);
	//return Result.GetNormalized();
	return Slerp.Rotator().GetNormalized();
}
FRotator UALSMathLibrary::RInterpTo(const FRotator& Current, const FRotator& Target, float DeltaTime, float InterpSpeed)
{
	// if DeltaTime is 0, do not perform any interpolation (Location was already calculated for that frame)
	if (DeltaTime == 0.f || Current == Target)
	{
		return Current;
	}

	// If no interp speed, jump to target value
	if (InterpSpeed <= 0.f)
	{
		return Target;
	}

	const float DeltaInterpSpeed = InterpSpeed * DeltaTime;

	const FRotator Delta = (Target - Current).GetNormalized();

	// If steps are too small, just return Target and assume we have reached our destination.
	if (Delta.IsNearlyZero())
	{
		return Target;
	}

	// Delta Move, Clamp so we do not over shoot.
	const FRotator DeltaMove = Delta * FMath::Clamp<float>(DeltaInterpSpeed, 0.f, 1.f);

	const float DeltaInterpClamped = FMath::Clamp<float>(DeltaInterpSpeed, 0.f, 1.f);
	FQuat Slerp = FQuat::Slerp(Current.Quaternion(), Target.Quaternion(), DeltaInterpClamped);

	
	return (Current + DeltaMove).GetNormalized();
}
