// OpenXRSim: Keyboard/mouse/gamepad input processing and XR key injection.
// Easy guide: read this file first when you need this behavior.

#include "OpenXRSimInputDevice.h"
#include "OpenXRSimForwardReceiver.h"
#include "OpenXRSimState.h"
#include "OpenXRSimSettings.h"
#include "OpenXRSimLog.h"

#include "InputCoreTypes.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include <Xinput.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#if WITH_SLATE
#include "Framework/Application/SlateApplication.h"
#endif

FOpenXRSimInputDevice::FOpenXRSimInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler,
                                             TSharedPtr<FOpenXRSimState, ESPMode::ThreadSafe> InState)
	: MessageHandler(InMessageHandler)
	, SimState(InState)
{
	ForwardReceiver = MakeUnique<FOpenXRSimForwardReceiver>();
}

FOpenXRSimInputDevice::~FOpenXRSimInputDevice()
{
}

void FOpenXRSimInputDevice::SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
{
	MessageHandler = InMessageHandler;
}

void FOpenXRSimInputDevice::Tick(float InDeltaTime)
{
	DeltaSeconds = InDeltaTime;
}

void FOpenXRSimInputDevice::SendControllerEvents()
{
	if (!SimState.IsValid())
	{
		return;
	}

	// Update sim state from live input (unless replay is driving it)
	if (!SimState->IsReplaying())
	{
		UpdateFromKeyboardMouse();
		UpdateFromGamepad();
		if (ForwardReceiver)
		{
			ForwardReceiver->Tick(SimState, GetDefault<UOpenXRSimSettings>());
		}
	}

	// Always push current sim action state into UE input (works for replay too).
	PushXRKeysToEngine();
}

static int32 GetActiveDeviceIndex()
{
	return GetDefault<UOpenXRSimSettings>()->ActiveDeviceIndex;
}

static EOpenXRSimDevice ActiveDeviceFromIndex(int32 Idx)
{
	switch (Idx)
	{
	case 1: return EOpenXRSimDevice::Left;
	case 2: return EOpenXRSimDevice::Right;
	default: return EOpenXRSimDevice::HMD;
	}
}

bool FOpenXRSimInputDevice::IsKeyDown_AnyPlatform(const FKey& Key) const
{
#if PLATFORM_WINDOWS
	// Minimal mapping for the keys we use.
	// For MVP we hardcode the VK codes for common keys rather than trying to map arbitrary FKeys.
	const FName Name = Key.GetFName();

	auto VkDown = [](int Vk) -> bool
	{
		return (GetAsyncKeyState(Vk) & 0x8000) != 0;
	};

	if (Name == EKeys::LeftAlt.GetFName()) return VkDown(VK_LMENU);
	if (Name == EKeys::W.GetFName()) return VkDown('W');
	if (Name == EKeys::A.GetFName()) return VkDown('A');
	if (Name == EKeys::S.GetFName()) return VkDown('S');
	if (Name == EKeys::D.GetFName()) return VkDown('D');
	if (Name == EKeys::Q.GetFName()) return VkDown('Q');
	if (Name == EKeys::E.GetFName()) return VkDown('E');

	if (Name == EKeys::One.GetFName()) return VkDown('1');
	if (Name == EKeys::Two.GetFName()) return VkDown('2');
	if (Name == EKeys::Three.GetFName()) return VkDown('3');

	if (Name == EKeys::Up.GetFName()) return VkDown(VK_UP);
	if (Name == EKeys::Down.GetFName()) return VkDown(VK_DOWN);
	if (Name == EKeys::Left.GetFName()) return VkDown(VK_LEFT);
	if (Name == EKeys::Right.GetFName()) return VkDown(VK_RIGHT);

	// Mouse buttons
	if (Name == EKeys::LeftMouseButton.GetFName()) return VkDown(VK_LBUTTON);
	if (Name == EKeys::RightMouseButton.GetFName()) return VkDown(VK_RBUTTON);

	// Primary/secondary buttons
	if (Name == EKeys::X.GetFName()) return VkDown('X');
	if (Name == EKeys::C.GetFName()) return VkDown('C');
	if (Name == EKeys::V.GetFName()) return VkDown('V');

	return false;
#else
	// Non-windows MVP: no polling (replay still works)
	return false;
#endif
}

FVector2D FOpenXRSimInputDevice::GetMouseDelta_AnyPlatform()
{
#if WITH_SLATE
	if (!FSlateApplication::IsInitialized())
	{
		return FVector2D::ZeroVector;
	}

	const FVector2D Now = FSlateApplication::Get().GetCursorPos();
	if (!bPrevMousePosValid)
	{
		bPrevMousePosValid = true;
		PrevMousePos = Now;
		return FVector2D::ZeroVector;
	}

	const FVector2D Delta = Now - PrevMousePos;
	PrevMousePos = Now;
	return Delta;
#else
	return FVector2D::ZeroVector;
#endif
}

void FOpenXRSimInputDevice::UpdateFromKeyboardMouse()
{
	const UOpenXRSimSettings* S = GetDefault<UOpenXRSimSettings>();

	// device select (1/2/3)
	if (IsKeyDown_AnyPlatform(EKeys::One))   GetMutableDefault<UOpenXRSimSettings>()->ActiveDeviceIndex = 0;
	if (IsKeyDown_AnyPlatform(EKeys::Two))   GetMutableDefault<UOpenXRSimSettings>()->ActiveDeviceIndex = 1;
	if (IsKeyDown_AnyPlatform(EKeys::Three)) GetMutableDefault<UOpenXRSimSettings>()->ActiveDeviceIndex = 2;

	const bool bAltDown = IsKeyDown_AnyPlatform(EKeys::LeftAlt);
	if (S->bRequireAltForPoseControl && !bAltDown)
	{
		// Still allow action buttons without Alt? (MVP: yes)
	}
	else
	{
		// Translation with WASDQE in local yaw (we simplify: world axes)
		FVector Move = FVector::ZeroVector;
		if (IsKeyDown_AnyPlatform(EKeys::W)) Move.Y += 1.0f;
		if (IsKeyDown_AnyPlatform(EKeys::S)) Move.Y -= 1.0f;
		if (IsKeyDown_AnyPlatform(EKeys::D)) Move.X += 1.0f;
		if (IsKeyDown_AnyPlatform(EKeys::A)) Move.X -= 1.0f;
		if (IsKeyDown_AnyPlatform(EKeys::E)) Move.Z += 1.0f;
		if (IsKeyDown_AnyPlatform(EKeys::Q)) Move.Z -= 1.0f;

		Move = Move.GetClampedToMaxSize(1.0f) * (S->TranslateSpeedCmPerSec * FMath::Max(DeltaSeconds, 0.0f));

		// Rotation from mouse delta (yaw/pitch)
		const FVector2D MouseDelta = GetMouseDelta_AnyPlatform();
		const float YawDeltaDeg   = MouseDelta.X * (S->RotateSpeedDegPerSec * 0.01f);
		const float PitchDeltaDeg = -MouseDelta.Y * (S->RotateSpeedDegPerSec * 0.01f);

		const FQuat RotDelta =
			FQuat(FVector::UpVector,   FMath::DegreesToRadians(YawDeltaDeg)) *
			FQuat(FVector::RightVector, FMath::DegreesToRadians(PitchDeltaDeg));

		const EOpenXRSimDevice Dev = ActiveDeviceFromIndex(GetActiveDeviceIndex());
		SimState->ApplyPoseDelta(Dev, Move, RotDelta);
	}

	// Controller actions: apply to the active controller when selected (Left/Right)
	const EOpenXRSimDevice ActiveDev = ActiveDeviceFromIndex(GetActiveDeviceIndex());
	if (ActiveDev == EOpenXRSimDevice::Left || ActiveDev == EOpenXRSimDevice::Right)
	{
		const bool bTrigger = IsKeyDown_AnyPlatform(EKeys::LeftMouseButton);
		const bool bGrip    = IsKeyDown_AnyPlatform(EKeys::RightMouseButton);

		// Thumbstick axes as digital -1/0/1 from arrow keys
		FVector2D Stick = FVector2D::ZeroVector;
		if (IsKeyDown_AnyPlatform(EKeys::Left))  Stick.X -= 1.0f;
		if (IsKeyDown_AnyPlatform(EKeys::Right)) Stick.X += 1.0f;
		if (IsKeyDown_AnyPlatform(EKeys::Up))    Stick.Y += 1.0f;
		if (IsKeyDown_AnyPlatform(EKeys::Down))  Stick.Y -= 1.0f;

		const bool bPrimary = IsKeyDown_AnyPlatform(EKeys::X);
		const bool bSecondary = IsKeyDown_AnyPlatform(EKeys::C);
		const bool bStickClick = IsKeyDown_AnyPlatform(EKeys::V);

		SimState->SetControllerActions(
			ActiveDev,
			bTrigger ? 1.0f : 0.0f,
			bGrip ? 1.0f : 0.0f,
			Stick,
			bPrimary,
			bSecondary,
			bStickClick
		);
	}
}

static float NormalizeThumb(int16 Value, float DeadZone)
{
	const float N = FMath::Clamp((float)Value / 32767.0f, -1.0f, 1.0f);
	return (FMath::Abs(N) < DeadZone) ? 0.0f : N;
}

void FOpenXRSimInputDevice::UpdateFromGamepad()
{
	const UOpenXRSimSettings* S = GetDefault<UOpenXRSimSettings>();
	if (!S->bEnableXInputGamepad)
	{
		return;
	}

#if PLATFORM_WINDOWS
	XINPUT_STATE XiState;
	FMemory::Memzero(XiState);
	if (XInputGetState(0, &XiState) != ERROR_SUCCESS)
	{
		return;
	}

	const XINPUT_GAMEPAD& Pad = XiState.Gamepad;
	const float Lx = NormalizeThumb(Pad.sThumbLX, 0.2f);
	const float Ly = NormalizeThumb(Pad.sThumbLY, 0.2f);
	const float Rx = NormalizeThumb(Pad.sThumbRX, 0.2f);
	const float Ry = NormalizeThumb(Pad.sThumbRY, 0.2f);
	const float Lt = FMath::Clamp((float)Pad.bLeftTrigger / 255.0f, 0.0f, 1.0f);
	const float Rt = FMath::Clamp((float)Pad.bRightTrigger / 255.0f, 0.0f, 1.0f);

	// Map gamepad to controller actions.
	SimState->SetControllerActions(
		EOpenXRSimDevice::Left,
		Lt,
		(Pad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) ? 1.0f : 0.0f,
		FVector2D(Lx, Ly),
		(Pad.wButtons & XINPUT_GAMEPAD_X) != 0,
		(Pad.wButtons & XINPUT_GAMEPAD_Y) != 0,
		(Pad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0
	);

	SimState->SetControllerActions(
		EOpenXRSimDevice::Right,
		Rt,
		(Pad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) ? 1.0f : 0.0f,
		FVector2D(Rx, Ry),
		(Pad.wButtons & XINPUT_GAMEPAD_A) != 0,
		(Pad.wButtons & XINPUT_GAMEPAD_B) != 0,
		(Pad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0
	);

	if (S->bGamepadDrivesActiveDevicePose)
	{
		FVector Move = FVector(Lx, Ly, 0.0f);
		if (Pad.wButtons & XINPUT_GAMEPAD_DPAD_UP) Move.Z += 1.0f;
		if (Pad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) Move.Z -= 1.0f;
		Move = Move.GetClampedToMaxSize(1.0f) * (S->TranslateSpeedCmPerSec * FMath::Max(DeltaSeconds, 0.0f));

		const float YawDeltaDeg = Rx * (S->RotateSpeedDegPerSec * FMath::Max(DeltaSeconds, 0.0f));
		const float PitchDeltaDeg = -Ry * (S->RotateSpeedDegPerSec * FMath::Max(DeltaSeconds, 0.0f));
		const FQuat RotDelta =
			FQuat(FVector::UpVector, FMath::DegreesToRadians(YawDeltaDeg)) *
			FQuat(FVector::RightVector, FMath::DegreesToRadians(PitchDeltaDeg));

		SimState->ApplyPoseDelta(ActiveDeviceFromIndex(GetActiveDeviceIndex()), Move, RotDelta);
	}
#endif
}

void FOpenXRSimInputDevice::PushXRKeysToEngine()
{
	// We inject generic MotionController keys (vendor-agnostic).
	// ControllerId = 0 for player 0.

	const int32 ControllerId = 0;

	const FOpenXRSimControllerState L = SimState->GetLeftController();
	const FOpenXRSimControllerState R = SimState->GetRightController();

	auto SendAnalog = [&](const FKey& Key, float Value)
	{
		MessageHandler->OnControllerAnalog(Key.GetFName(), ControllerId, Value);
	};

	auto SendButton = [&](const FKey& Key, bool bDown, bool& bPrevDown)
	{
		if (bDown && !bPrevDown)
		{
			MessageHandler->OnControllerButtonPressed(Key.GetFName(), ControllerId, false);
		}
		else if (!bDown && bPrevDown)
		{
			MessageHandler->OnControllerButtonReleased(Key.GetFName(), ControllerId, false);
		}
		bPrevDown = bDown;
	};

	// Left
	SendAnalog(EKeys::MotionController_Left_TriggerAxis, L.Trigger);
	SendAnalog(EKeys::MotionController_Left_Grip1Axis,    L.Grip);
	SendAnalog(EKeys::MotionController_Left_Thumbstick_X, L.Thumbstick.X);
	SendAnalog(EKeys::MotionController_Left_Thumbstick_Y, L.Thumbstick.Y);

	SendButton(EKeys::MotionController_Left_Trigger, L.Trigger > 0.5f, bPrevTriggerPressedL);
	SendButton(EKeys::MotionController_Left_Grip1,   L.Grip > 0.5f,    bPrevGripPressedL);
	SendButton(EKeys::MotionController_Left_FaceButton1, L.bPrimaryButton, bPrevPrimaryPressedL);
	SendButton(EKeys::MotionController_Left_FaceButton2, L.bSecondaryButton, bPrevSecondaryPressedL);
	SendButton(EKeys::MotionController_Left_Thumbstick,  L.bThumbstickClick, bPrevThumbstickPressedL);

	// Right
	SendAnalog(EKeys::MotionController_Right_TriggerAxis, R.Trigger);
	SendAnalog(EKeys::MotionController_Right_Grip1Axis,    R.Grip);
	SendAnalog(EKeys::MotionController_Right_Thumbstick_X, R.Thumbstick.X);
	SendAnalog(EKeys::MotionController_Right_Thumbstick_Y, R.Thumbstick.Y);

	SendButton(EKeys::MotionController_Right_Trigger, R.Trigger > 0.5f, bPrevTriggerPressedR);
	SendButton(EKeys::MotionController_Right_Grip1,   R.Grip > 0.5f,    bPrevGripPressedR);
	SendButton(EKeys::MotionController_Right_FaceButton1, R.bPrimaryButton, bPrevPrimaryPressedR);
	SendButton(EKeys::MotionController_Right_FaceButton2, R.bSecondaryButton, bPrevSecondaryPressedR);
	SendButton(EKeys::MotionController_Right_Thumbstick,  R.bThumbstickClick, bPrevThumbstickPressedR);
}
