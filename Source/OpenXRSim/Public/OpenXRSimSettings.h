// OpenXRSim: Project settings exposed in Unreal for simulator behavior.
// Easy guide: read this file first when you need this behavior.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OpenXRSimSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="OpenXR Simulator"))
class OPENXRSIM_API UOpenXRSimSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// Toggle set by editor toolbar.
	UPROPERTY(Config, EditAnywhere, Category="General")
	bool bEnableSimulator = false;

	// Input capture mode: require Alt to drive poses.
	UPROPERTY(Config, EditAnywhere, Category="Input")
	bool bRequireAltForPoseControl = true;

	// Allow Xbox/XInput to drive device poses and controller actions.
	UPROPERTY(Config, EditAnywhere, Category="Input|Gamepad")
	bool bEnableXInputGamepad = true;

	// If true, gamepad thumbsticks also move/rotate currently selected device.
	UPROPERTY(Config, EditAnywhere, Category="Input|Gamepad")
	bool bGamepadDrivesActiveDevicePose = true;

	UPROPERTY(Config, EditAnywhere, Category="Input", meta=(ClampMin="1.0", ClampMax="1000.0"))
	float TranslateSpeedCmPerSec = 200.0f;

	UPROPERTY(Config, EditAnywhere, Category="Input", meta=(ClampMin="1.0", ClampMax="720.0"))
	float RotateSpeedDegPerSec = 120.0f;

	// Opt-in fixed timestep for deterministic replay.
	UPROPERTY(Config, EditAnywhere, Category="Determinism")
	bool bReplayUseFixedTimestep = true;

	UPROPERTY(Config, EditAnywhere, Category="Determinism", meta=(ClampMin="0.001", ClampMax="0.1"))
	float ReplayFixedDt = 1.0f / 60.0f;

	// Stereo characteristics (simple side-by-side).
	UPROPERTY(Config, EditAnywhere, Category="Stereo", meta=(ClampMin="0.03", ClampMax="0.09"))
	float InterpupillaryDistanceMeters = 0.064f;

	UPROPERTY(Config, EditAnywhere, Category="Stereo", meta=(ClampMin="30.0", ClampMax="140.0"))
	float HFOVDegrees = 100.0f;

	UPROPERTY(Config, EditAnywhere, Category="Stereo", meta=(ClampMin="30.0", ClampMax="140.0"))
	float VFOVDegrees = 90.0f;

	UPROPERTY(Config, EditAnywhere, Category="Stereo")
	FIntPoint IdealRenderTargetSize = FIntPoint(2000, 2000);

	// Content paths
	UPROPERTY(Config, EditAnywhere, Category="Rooms")
	FString DefaultRoomJsonRelative = TEXT("Content/Rooms/Hero_SmallRoom.json");

	UPROPERTY(Config, EditAnywhere, Category="RecordReplay")
	FString DefaultReplayDirRelative = TEXT("Saved/OpenXRSim/Replays");

	// If set, this replay file is started once when a game world first ticks.
	// Relative paths are resolved against ProjectDir.
	UPROPERTY(Config, EditAnywhere, Category="RecordReplay")
	FString AutoReplayPath = TEXT("");

	UPROPERTY(Config, EditAnywhere, Category="RecordReplay")
	bool bAutoReplayOnWorldStart = false;

	// Room startup policy
	UPROPERTY(Config, EditAnywhere, Category="Rooms")
	bool bLoadRoomOnWorldStart = true;

	// Use built-in hero room when no JSON room is available.
	UPROPERTY(Config, EditAnywhere, Category="Rooms")
	bool bPreferBuiltInRoomOnWorldStart = true;

	// Forward external tracking/actions into the sim using UDP JSON packets.
	UPROPERTY(Config, EditAnywhere, Category="Input|Forwarding")
	bool bEnableDataForwarding = false;

	// Bind address for forwarding receiver (for example "127.0.0.1" or "0.0.0.0").
	UPROPERTY(Config, EditAnywhere, Category="Input|Forwarding")
	FString DataForwardBindAddress = TEXT("127.0.0.1");

	UPROPERTY(Config, EditAnywhere, Category="Input|Forwarding", meta=(ClampMin="1024", ClampMax="65535"))
	int32 DataForwardPort = 7779;

	// Selected device for keyboard/mouse control
	UPROPERTY(Config, EditAnywhere, Category="Input")
	int32 ActiveDeviceIndex = 0; // 0=HMD, 1=Left, 2=Right
};
