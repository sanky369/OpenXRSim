// OpenXRSim: Thread-safe simulator state API used by runtime systems.
// Easy guide: read this file first when you need this behavior.

#pragma once

#include "CoreMinimal.h"
#include "OpenXRSimTypes.h"

class OPENXRSIM_API FOpenXRSimState : public TSharedFromThis<FOpenXRSimState, ESPMode::ThreadSafe>
{
public:
	FOpenXRSimState();

	// Called once per game frame by the XR system.
	void Tick(class UWorld* World, float DeltaSeconds);

	// Pose getters (thread-safe snapshot)
	FOpenXRSimPose GetHMDPose() const;
	FOpenXRSimControllerState GetLeftController() const;
	FOpenXRSimControllerState GetRightController() const;

	// Apply pose deltas from input simulation (game thread)
	void ApplyPoseDelta(EOpenXRSimDevice Device, const FVector& DeltaPosCm, const FQuat& DeltaRot);
	void SetDevicePose(EOpenXRSimDevice Device, const FOpenXRSimPose& Pose);

	// Apply action state deltas for selected controller (game thread)
	void SetControllerActions(EOpenXRSimDevice WhichController, float Trigger, float Grip, const FVector2D& Stick,
		bool bPrimary, bool bSecondary, bool bStickClick);
	void SetControllerState(EOpenXRSimDevice WhichController, const FOpenXRSimControllerState& InState);

	// Record/Replay
	bool StartRecording(const FString& AbsolutePath);
	bool StopRecording();
	bool StartReplay(const FString& AbsolutePath);
	bool StopReplay();
	bool IsRecording() const;
	bool IsReplaying() const;

	// Room selection (MVP: just a string stored in capture)
	void SetActiveRoomName(const FString& InRoomName);
	FString GetActiveRoomName() const;

	// Determinism
	void SetReplayFixedDt(float InFixedDt);
	float GetReplayFixedDt() const;

	// Diagnostics
	float GetSimTimeSeconds() const;
	FString GetInstanceLabel() const;

private:
	mutable FCriticalSection Mutex;

	// Current state
	FOpenXRSimPose HMDPose;
	FOpenXRSimControllerState Left;
	FOpenXRSimControllerState Right;

	FString ActiveRoomName;

	// Record/replay
	bool bRecording = false;
	bool bReplaying = false;

	float ReplayFixedDt = 1.0f / 60.0f;
	float ReplayTime = 0.0f;
	float SimTimeSeconds = 0.0f;
	int32 ReplayFrameIndex = 0;
	float RecordAccumulator = 0.0f;
	FString InstanceLabel;

	FString RecordPath;
	FString ReplayPath;

	TArray<FOpenXRSimFrame> Frames;

	// Helpers
	void CaptureFrame(float TimeSeconds);
	void ApplyFrame(const FOpenXRSimFrame& Frame);
	bool LoadFramesFromFile(const FString& AbsolutePath);
	bool SaveFramesToFile(const FString& AbsolutePath) const;
};
