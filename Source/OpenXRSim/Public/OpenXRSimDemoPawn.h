// OpenXRSim: Simple pawn used to validate simulated motion-controller input.
// Easy guide: read this file first when you need this behavior.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "OpenXRSimDemoPawn.generated.h"

class UCameraComponent;
class UMotionControllerComponent;
class UStaticMeshComponent;

UCLASS()
class OPENXRSIM_API AOpenXRSimDemoPawn : public APawn
{
	GENERATED_BODY()

public:
	AOpenXRSimDemoPawn();

	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY()
	TObjectPtr<USceneComponent> Root;

	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY()
	TObjectPtr<UMotionControllerComponent> LeftMC;

	UPROPERTY()
	TObjectPtr<UMotionControllerComponent> RightMC;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> LeftVis;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> RightVis;
};
