// OpenXRSim: Shared simulator data types: poses, controller state, frame snapshots.
// Easy guide: read this file first when you need this behavior.

#pragma once

#include "CoreMinimal.h"
#include "OpenXRSimTypes.generated.h"

UENUM(BlueprintType)
enum class EOpenXRSimDevice : uint8
{
	HMD     UMETA(DisplayName="HMD"),
	Left    UMETA(DisplayName="Left Controller"),
	Right   UMETA(DisplayName="Right Controller"),
};

USTRUCT(BlueprintType)
struct FOpenXRSimPose
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector PositionCm = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FQuat Orientation = FQuat::Identity;
};

USTRUCT(BlueprintType)
struct FOpenXRSimControllerState
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FOpenXRSimPose GripPose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FOpenXRSimPose AimPose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Trigger = 0.0f; // 0..1

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Grip = 0.0f; // 0..1

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2D Thumbstick = FVector2D::ZeroVector; // -1..1

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bPrimaryButton = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSecondaryButton = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bThumbstickClick = false;
};

USTRUCT()
struct FOpenXRSimFrame
{
	GENERATED_BODY();

	UPROPERTY()
	float TimeSeconds = 0.0f;

	UPROPERTY()
	FOpenXRSimPose HMD;

	UPROPERTY()
	FOpenXRSimControllerState Left;

	UPROPERTY()
	FOpenXRSimControllerState Right;
};
