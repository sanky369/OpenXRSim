// OpenXRSim: Actor that renders room primitives and debug visuals.
// Easy guide: read this file first when you need this behavior.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OpenXRSimRoomActor.generated.h"

class UInstancedStaticMeshComponent;

UCLASS()
class OPENXRSIM_API AOpenXRSimRoomActor : public AActor
{
	GENERATED_BODY()

public:
	AOpenXRSimRoomActor();

	void SetDebugDraw(bool bEnable);

	// Built-in room (procedural)
	void BuildHeroSmallRoom();

	// JSON build
	bool BuildFromJson(const TSharedPtr<class FJsonObject>& Root);

protected:
	UPROPERTY()
	TObjectPtr<USceneComponent> Root;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> InstancedCubes;

	bool bDebugDraw = true;

	void ClearInstances();
	void AddBoxCm(const FVector& CenterCm, const FVector& SizeCm);

	void DebugDrawBounds(const FVector& MinCm, const FVector& MaxCm);
	void DebugDrawAnchor(const FString& Id, const FVector& PositionCm);
};
