// OpenXRSim: World subsystem API for loading and managing simulated rooms.
// Easy guide: read this file first when you need this behavior.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OpenXRSimRoomSubsystem.generated.h"

class AOpenXRSimRoomActor;

UCLASS()
class OPENXRSIM_API UOpenXRSimRoomSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Load a built-in procedural room
	UFUNCTION(BlueprintCallable, Category="OpenXRSim|Rooms")
	void LoadBuiltInRoom();

	// Load JSON room from an absolute path
	UFUNCTION(BlueprintCallable, Category="OpenXRSim|Rooms")
	bool LoadRoomFromJsonFile(const FString& AbsolutePath);

	UFUNCTION(BlueprintCallable, Category="OpenXRSim|Rooms")
	bool LoadDefaultRoomFromSettings();

	UFUNCTION(BlueprintCallable, Category="OpenXRSim|Rooms")
	void SetDebugDraw(bool bEnable);

private:
	UPROPERTY()
	TObjectPtr<AOpenXRSimRoomActor> RoomActor;

	bool bDebugDraw = true;

	void EnsureRoomActor();
};
