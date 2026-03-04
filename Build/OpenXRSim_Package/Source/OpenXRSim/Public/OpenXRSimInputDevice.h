// OpenXRSim: Input bridge that injects simulated XR controls into Unreal input.
// Easy guide: read this file first when you need this behavior.

#pragma once

#include "CoreMinimal.h"
#include "IInputDevice.h"
#include "InputCoreTypes.h"

class FOpenXRSimState;
class FOpenXRSimForwardReceiver;

class OPENXRSIM_API FOpenXRSimInputDevice : public IInputDevice
{
public:
	FOpenXRSimInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler,
	                      TSharedPtr<FOpenXRSimState, ESPMode::ThreadSafe> InState);

	virtual ~FOpenXRSimInputDevice() override;

	// IInputDevice
	virtual void Tick(float DeltaTime) override;
	virtual void SendControllerEvents() override;
	virtual void SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) override;
	virtual void SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override;
	virtual void SetChannelValues(int32 ControllerId, const FForceFeedbackValues& Values) override;
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override { return false; }

private:
	TSharedRef<FGenericApplicationMessageHandler> MessageHandler;
	TSharedPtr<FOpenXRSimState, ESPMode::ThreadSafe> SimState;

	float DeltaSeconds = 0.0f;

	// Cached for edge detection
	bool bPrevTriggerPressedL = false;
	bool bPrevGripPressedL = false;
	bool bPrevPrimaryPressedL = false;
	bool bPrevSecondaryPressedL = false;
	bool bPrevThumbstickPressedL = false;
	bool bPrevTriggerPressedR = false;
	bool bPrevGripPressedR = false;
	bool bPrevPrimaryPressedR = false;
	bool bPrevSecondaryPressedR = false;
	bool bPrevThumbstickPressedR = false;

	// Helpers
	void UpdateFromKeyboardMouse();
	void UpdateFromGamepad();
	void PushXRKeysToEngine();

	// Key polling helpers (Windows-first)
	bool IsKeyDown_AnyPlatform(const FKey& Key) const;
	FVector2D GetMouseDelta_AnyPlatform();

	FVector2D PrevMousePos = FVector2D::ZeroVector;
	bool bPrevMousePosValid = false;

	TUniquePtr<FOpenXRSimForwardReceiver> ForwardReceiver;
};
