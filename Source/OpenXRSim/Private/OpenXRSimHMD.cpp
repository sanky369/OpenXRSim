// OpenXRSim: Headset simulation: tracking poses, stereo setup, startup actions.
// Easy guide: read this file first when you need this behavior.

#include "OpenXRSimHMD.h"
#include "OpenXRSimState.h"
#include "OpenXRSimSettings.h"
#include "OpenXRSimRoomSubsystem.h"
#include "OpenXRSimLog.h"

#include "Engine/Engine.h"
#include "IXRTrackingSystem.h"
#include "HeadMountedDisplayTypes.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

static FString ResolveConfiguredPath(const FString& ConfigPath)
{
	if (ConfigPath.IsEmpty())
	{
		return FString();
	}

	if (FPaths::IsRelative(ConfigPath))
	{
		const FString ProjectResolved = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), ConfigPath));
		if (FPaths::FileExists(ProjectResolved))
		{
			return ProjectResolved;
		}

		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("OpenXRSim"));
		if (Plugin.IsValid())
		{
			const FString PluginResolved = FPaths::ConvertRelativePathToFull(FPaths::Combine(Plugin->GetBaseDir(), ConfigPath));
			if (FPaths::FileExists(PluginResolved))
			{
				return PluginResolved;
			}
		}

		return ProjectResolved;
	}

	return ConfigPath;
}

FOpenXRSimHMD::FOpenXRSimHMD(const FAutoRegister& AutoRegister, TSharedPtr<FOpenXRSimState, ESPMode::ThreadSafe> InState)
	: FSceneViewExtensionBase(AutoRegister)
	, SimState(InState)
{
}

FOpenXRSimHMD::~FOpenXRSimHMD()
{
}

FName FOpenXRSimHMD::GetSystemName() const
{
	return FName(TEXT("OpenXRSim"));
}

bool FOpenXRSimHMD::EnumerateTrackedDevices(TArray<int32>& OutDevices, EXRTrackedDeviceType Type)
{
	OutDevices.Reset();

	// Minimal device IDs (MVP):
	// 0 = HMD, 1 = Left controller, 2 = Right controller
	if (Type == EXRTrackedDeviceType::Any || Type == EXRTrackedDeviceType::HeadMountedDisplay)
	{
		OutDevices.Add(0);
	}
	if (Type == EXRTrackedDeviceType::Any || Type == EXRTrackedDeviceType::Controller)
	{
		OutDevices.Add(1);
		OutDevices.Add(2);
	}
	return true;
}

float FOpenXRSimHMD::GetWorldToMetersScale() const
{
	if (CachedWorld.IsValid())
	{
		return CachedWorld->GetWorldSettings()->WorldToMeters;
	}
	return 100.0f;
}

bool FOpenXRSimHMD::GetCurrentPose(int32 DeviceId, FQuat& CurrentOrientation, FVector& CurrentPosition)
{
	if (!SimState.IsValid())
	{
		CurrentOrientation = FQuat::Identity;
		CurrentPosition = FVector::ZeroVector;
		return false;
	}

	const float WorldToMeters = GetWorldToMetersScale();

	if (DeviceId == 0)
	{
		const FOpenXRSimPose Pose = SimState->GetHMDPose();
		CurrentOrientation = (BaseOrientation.Inverse() * Pose.Orientation).GetNormalized();
		CurrentPosition = (Pose.PositionCm - BasePosition) * (WorldToMeters / 100.0f);
		return true;
	}

	if (DeviceId == 1)
	{
		const FOpenXRSimControllerState C = SimState->GetLeftController();
		CurrentOrientation = (BaseOrientation.Inverse() * C.GripPose.Orientation).GetNormalized();
		CurrentPosition = (C.GripPose.PositionCm - BasePosition) * (WorldToMeters / 100.0f);
		return true;
	}

	if (DeviceId == 2)
	{
		const FOpenXRSimControllerState C = SimState->GetRightController();
		CurrentOrientation = (BaseOrientation.Inverse() * C.GripPose.Orientation).GetNormalized();
		CurrentPosition = (C.GripPose.PositionCm - BasePosition) * (WorldToMeters / 100.0f);
		return true;
	}

	CurrentOrientation = FQuat::Identity;
	CurrentPosition = FVector::ZeroVector;
	return false;
}

bool FOpenXRSimHMD::GetRelativeEyePose(int32 DeviceId, int32 ViewIndex, FQuat& OutOrientation, FVector& OutPosition)
{
	// XR docs define HMDDeviceId == 0.
	if (DeviceId != IXRTrackingSystem::HMDDeviceId)
	{
		OutOrientation = FQuat::Identity;
		OutPosition = FVector::ZeroVector;
		return false;
	}

	const float WorldToMeters = GetWorldToMetersScale();
	const float HalfIpdCm = GetDefault<UOpenXRSimSettings>()->InterpupillaryDistanceMeters * 50.0f;
	const float HalfIpdUU = HalfIpdCm * (WorldToMeters / 100.0f);
	const float EyeSign = (ViewIndex == 0) ? -1.0f : 1.0f;

	OutOrientation = FQuat::Identity;
	OutPosition = FVector(0.0f, EyeSign * HalfIpdUU, 0.0f);
	return true;
}

bool FOpenXRSimHMD::DoesSupportPositionalTracking() const
{
	return true;
}

bool FOpenXRSimHMD::OnStartGameFrame(FWorldContext& WorldContext)
{
	CachedWorld = WorldContext.World();
	ApplyWorldStartupActions(CachedWorld.Get());

	if (SimState.IsValid() && CachedWorld.IsValid())
	{
		SimState->SetReplayFixedDt(GetDefault<UOpenXRSimSettings>()->ReplayFixedDt);
		SimState->Tick(CachedWorld.Get(), CachedWorld->GetDeltaSeconds());
	}

	return true;
}

bool FOpenXRSimHMD::IsHMDConnected()
{
	return GetDefault<UOpenXRSimSettings>()->bEnableSimulator;
}

bool FOpenXRSimHMD::IsHMDEnabled() const
{
	return bHmdEnabled;
}

void FOpenXRSimHMD::EnableHMD(bool bEnable)
{
	bHmdEnabled = bEnable;
}

void FOpenXRSimHMD::GetFieldOfView(float& OutHFOVInDegrees, float& OutVFOVInDegrees) const
{
	const UOpenXRSimSettings* S = GetDefault<UOpenXRSimSettings>();
	OutHFOVInDegrees = S->HFOVDegrees;
	OutVFOVInDegrees = S->VFOVDegrees;
}

FIntPoint FOpenXRSimHMD::GetIdealRenderTargetSize() const
{
	return GetDefault<UOpenXRSimSettings>()->IdealRenderTargetSize;
}

void FOpenXRSimHMD::ResetOrientationAndPosition(float Yaw)
{
	if (!SimState.IsValid()) return;

	const FOpenXRSimPose Pose = SimState->GetHMDPose();
	BaseOrientation = Pose.Orientation;

	// Optional yaw override
	if (!FMath::IsNearlyZero(Yaw))
	{
		BaseOrientation = FQuat(FVector::UpVector, FMath::DegreesToRadians(Yaw)) * BaseOrientation;
		BaseOrientation.Normalize();
	}

	BasePosition = Pose.PositionCm;
}

bool FOpenXRSimHMD::IsStereoEnabled() const
{
	return bStereoEnabled && bHmdEnabled;
}

bool FOpenXRSimHMD::EnableStereo(bool bEnable)
{
	bStereoEnabled = bEnable;
	return bStereoEnabled;
}

void FOpenXRSimHMD::AdjustViewRect(int32 ViewIndex, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
{
	// Side-by-side: split horizontally
	const uint32 HalfX = SizeX / 2;
	if (ViewIndex == 0)
	{
		X = 0;
		SizeX = HalfX;
	}
	else
	{
		X = HalfX;
		SizeX = HalfX;
	}
}

EStereoscopicPass FOpenXRSimHMD::GetViewPassForIndex(bool bStereoRequested, int32 ViewIndex) const
{
	if (!bStereoRequested || !IsStereoEnabled())
	{
		return eSSP_FULL;
	}

	return (ViewIndex == 0) ? eSSP_LEFT_EYE : eSSP_RIGHT_EYE;
}

int32 FOpenXRSimHMD::GetDesiredNumberOfViews(bool bStereoRequested) const
{
	return (bStereoRequested && IsStereoEnabled()) ? 2 : 1;
}

FMatrix FOpenXRSimHMD::GetStereoProjectionMatrix(const int32 ViewIndex) const
{
	// Simple perspective matrix (not doing asymmetric frusta in MVP).
	const UOpenXRSimSettings* S = GetDefault<UOpenXRSimSettings>();

	const float HFovRad = FMath::DegreesToRadians(S->HFOVDegrees);
	const float VFovRad = FMath::DegreesToRadians(S->VFOVDegrees);

	const float HalfHFov = HFovRad * 0.5f;
	const float HalfVFov = VFovRad * 0.5f;

	const float XS = 1.0f / FMath::Tan(HalfHFov);
	const float YS = 1.0f / FMath::Tan(HalfVFov);

	const float NearZ = FMath::Max(1.0f, GNearClippingPlane);
	const float FarZ  = 100000.0f;

	const float Q  = FarZ / (FarZ - NearZ);
	const float Qn = -Q * NearZ;

	FMatrix M = FMatrix::Identity;
	M.M[0][0] = XS;  M.M[0][1] = 0;   M.M[0][2] = 0;   M.M[0][3] = 0;
	M.M[1][0] = 0;   M.M[1][1] = YS;  M.M[1][2] = 0;   M.M[1][3] = 0;
	M.M[2][0] = 0;   M.M[2][1] = 0;   M.M[2][2] = Q;   M.M[2][3] = 1;
	M.M[3][0] = 0;   M.M[3][1] = 0;   M.M[3][2] = Qn;  M.M[3][3] = 0;
	return M;
}

void FOpenXRSimHMD::GetMotionControllerData(UObject* WorldContext, const EControllerHand Hand, FXRMotionControllerData& MotionControllerData)
{
	MotionControllerData = FXRMotionControllerData();
	MotionControllerData.DeviceName = GetSystemName();
	MotionControllerData.ApplicationInstanceID = 0;

	const float WorldToMeters = GetWorldToMetersScale();

	if (!SimState.IsValid())
	{
		MotionControllerData.TrackingStatus = ETrackingStatus::NotTracked;
		return;
	}

	const bool bLeft = (Hand == EControllerHand::Left);
	const bool bRight = (Hand == EControllerHand::Right);

	if (!bLeft && !bRight)
	{
		MotionControllerData.TrackingStatus = ETrackingStatus::NotTracked;
		return;
	}

	const FOpenXRSimControllerState C = bLeft ? SimState->GetLeftController() : SimState->GetRightController();
	const FOpenXRSimPose Pose = C.GripPose;

	MotionControllerData.TrackingStatus = ETrackingStatus::Tracked;
	MotionControllerData.Hand = Hand;
	MotionControllerData.GripPosition = (Pose.PositionCm - BasePosition) * (WorldToMeters / 100.0f);
	MotionControllerData.GripRotation = (BaseOrientation.Inverse() * Pose.Orientation).Rotator();

	MotionControllerData.AimPosition = (C.AimPose.PositionCm - BasePosition) * (WorldToMeters / 100.0f);
	MotionControllerData.AimRotation = (BaseOrientation.Inverse() * C.AimPose.Orientation).Rotator();

	MotionControllerData.bValid = true;
}

bool FOpenXRSimHMD::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	return GetDefault<UOpenXRSimSettings>()->bEnableSimulator && bHmdEnabled;
}

void FOpenXRSimHMD::ApplyWorldStartupActions(UWorld* World)
{
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	if (!StartupWorld.IsValid() || StartupWorld.Get() != World)
	{
		StartupWorld = World;
		bStartupInitialized = false;
	}

	if (bStartupInitialized)
	{
		return;
	}

	const UOpenXRSimSettings* S = GetDefault<UOpenXRSimSettings>();
	bool bRoomInitDone = !S->bLoadRoomOnWorldStart;
	bool bReplayInitDone = !S->bAutoReplayOnWorldStart;

	if (S->bLoadRoomOnWorldStart)
	{
		if (UOpenXRSimRoomSubsystem* Rooms = World->GetSubsystem<UOpenXRSimRoomSubsystem>())
		{
			Rooms->LoadDefaultRoomFromSettings();
			bRoomInitDone = true;
		}
	}

	if (S->bAutoReplayOnWorldStart && SimState.IsValid())
	{
		const FString ReplayPath = ResolveConfiguredPath(S->AutoReplayPath);
		if (ReplayPath.IsEmpty())
		{
			bReplayInitDone = true;
		}
		else
		{
			SimState->StartReplay(ReplayPath);
			bReplayInitDone = true;
		}
	}

	bStartupInitialized = bRoomInitDone && bReplayInitDone;
}
