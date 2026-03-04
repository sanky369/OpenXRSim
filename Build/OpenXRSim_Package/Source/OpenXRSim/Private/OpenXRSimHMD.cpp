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
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/World.h"
#include "SceneView.h"
#include "EngineUtils.h"
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
	: FHeadMountedDisplayBase(nullptr)
	, FSceneViewExtensionBase(AutoRegister)
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

int32 FOpenXRSimHMD::GetXRSystemFlags() const
{
	return EXRSystemFlags::IsHeadMounted;
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
	const FVector OriginOffsetCm = GetTrackingOriginOffsetCm();

	if (DeviceId == 0)
	{
		const FOpenXRSimPose Pose = SimState->GetHMDPose();
		CurrentOrientation = (BaseOrientation.Inverse() * Pose.Orientation).GetNormalized();
		CurrentPosition = (Pose.PositionCm - BasePosition - OriginOffsetCm) * (WorldToMeters / 100.0f);
		return true;
	}

	if (DeviceId == 1)
	{
		const FOpenXRSimControllerState C = SimState->GetLeftController();
		CurrentOrientation = (BaseOrientation.Inverse() * C.GripPose.Orientation).GetNormalized();
		CurrentPosition = (C.GripPose.PositionCm - BasePosition - OriginOffsetCm) * (WorldToMeters / 100.0f);
		return true;
	}

	if (DeviceId == 2)
	{
		const FOpenXRSimControllerState C = SimState->GetRightController();
		CurrentOrientation = (BaseOrientation.Inverse() * C.GripPose.Orientation).GetNormalized();
		CurrentPosition = (C.GripPose.PositionCm - BasePosition - OriginOffsetCm) * (WorldToMeters / 100.0f);
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

void FOpenXRSimHMD::SetTrackingOrigin(EHMDTrackingOrigin::Type NewOrigin)
{
	EHMDTrackingOrigin::Type SanitizedOrigin = NewOrigin;
	// Simulator state is authored in floor space. Treat Local as LocalFloor to avoid eye-height collapse.
	if (SanitizedOrigin == EHMDTrackingOrigin::View
		|| SanitizedOrigin == EHMDTrackingOrigin::CustomOpenXR
		|| SanitizedOrigin == EHMDTrackingOrigin::Local)
	{
		SanitizedOrigin = EHMDTrackingOrigin::LocalFloor;
	}

	if (TrackingOrigin != SanitizedOrigin)
	{
		TrackingOrigin = SanitizedOrigin;
		OnTrackingOriginChanged();
	}
}

EHMDTrackingOrigin::Type FOpenXRSimHMD::GetTrackingOrigin() const
{
	return TrackingOrigin;
}

bool FOpenXRSimHMD::GetFloorToEyeTrackingTransform(FTransform& OutFloorToEye) const
{
	const UOpenXRSimSettings* Settings = GetDefault<UOpenXRSimSettings>();
	const float WorldToMeters = GetWorldToMetersScale();
	const float EyeHeightUU = Settings->DefaultStandingHeightCm * (WorldToMeters / 100.0f);
	OutFloorToEye = FTransform(FVector(0.0f, 0.0f, EyeHeightUU));
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

bool FOpenXRSimHMD::GetHMDMonitorInfo(MonitorInfo& MonitorDesc)
{
	MonitorDesc.MonitorName = TEXT("OpenXRSim Virtual HMD");
	MonitorDesc.MonitorId = 0;
	MonitorDesc.DesktopX = 0;
	MonitorDesc.DesktopY = 0;
	MonitorDesc.ResolutionX = 0;
	MonitorDesc.ResolutionY = 0;
	MonitorDesc.WindowSizeX = 0;
	MonitorDesc.WindowSizeY = 0;
	MonitorDesc.bShouldTestResolution = false;
	return false;
}

void FOpenXRSimHMD::GetFieldOfView(float& OutHFOVInDegrees, float& OutVFOVInDegrees) const
{
	const UOpenXRSimSettings* S = GetDefault<UOpenXRSimSettings>();
	OutHFOVInDegrees = S->HFOVDegrees;
	OutVFOVInDegrees = S->VFOVDegrees;
}

void FOpenXRSimHMD::SetInterpupillaryDistance(float NewInterpupillaryDistance)
{
	UOpenXRSimSettings* Settings = GetMutableDefault<UOpenXRSimSettings>();
	Settings->InterpupillaryDistanceMeters = FMath::Max(0.0f, NewInterpupillaryDistance);
}

float FOpenXRSimHMD::GetInterpupillaryDistance() const
{
	return GetDefault<UOpenXRSimSettings>()->InterpupillaryDistanceMeters;
}

FIntPoint FOpenXRSimHMD::GetIdealRenderTargetSize() const
{
	return GetDefault<UOpenXRSimSettings>()->IdealRenderTargetSize;
}

bool FOpenXRSimHMD::IsChromaAbCorrectionEnabled() const
{
	return false;
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
		return EStereoscopicPass::eSSP_FULL;
	}

	return (ViewIndex == 0) ? EStereoscopicPass::eSSP_PRIMARY : EStereoscopicPass::eSSP_SECONDARY;
}

int32 FOpenXRSimHMD::GetDesiredNumberOfViews(bool bStereoRequested) const
{
	return (bStereoRequested && IsStereoEnabled()) ? 2 : 1;
}

FMatrix FOpenXRSimHMD::GetStereoProjectionMatrix(const int32 ViewIndex) const
{
	// Use UE-compatible reversed-Z perspective form to avoid stereo culling/depth artifacts.
	const UOpenXRSimSettings* S = GetDefault<UOpenXRSimSettings>();

	const float HFovRad = FMath::DegreesToRadians(S->HFOVDegrees);
	const float VFovRad = FMath::DegreesToRadians(S->VFOVDegrees);

	const float HalfHFov = HFovRad * 0.5f;
	const float HalfVFov = VFovRad * 0.5f;

	const float XS = 1.0f / FMath::Tan(HalfHFov);
	const float YS = 1.0f / FMath::Tan(HalfVFov);

	const float NearZ = FMath::Max(1.0f, GNearClippingPlane);

	return FMatrix(
		FPlane(XS,   0.0f, 0.0f, 0.0f),
		FPlane(0.0f, YS,   0.0f, 0.0f),
		FPlane(0.0f, 0.0f, 0.0f, 1.0f),
		FPlane(0.0f, 0.0f, NearZ, 0.0f));
}

void FOpenXRSimHMD::GetMotionControllerData(UObject* WorldContext, const EControllerHand Hand, FXRMotionControllerData& MotionControllerData)
{
	MotionControllerData = FXRMotionControllerData();
	MotionControllerData.DeviceName = GetSystemName();
	MotionControllerData.ApplicationInstanceID = FGuid();

	const float WorldToMeters = GetWorldToMetersScale();
	const FVector OriginOffsetCm = GetTrackingOriginOffsetCm();

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
	MotionControllerData.HandIndex = Hand;
	MotionControllerData.GripPosition = (Pose.PositionCm - BasePosition - OriginOffsetCm) * (WorldToMeters / 100.0f);
	MotionControllerData.GripRotation = (BaseOrientation.Inverse() * Pose.Orientation).GetNormalized();

	MotionControllerData.AimPosition = (C.AimPose.PositionCm - BasePosition - OriginOffsetCm) * (WorldToMeters / 100.0f);
	MotionControllerData.AimRotation = (BaseOrientation.Inverse() * C.AimPose.Orientation).GetNormalized();

	MotionControllerData.bValid = true;
}

bool FOpenXRSimHMD::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	return GetDefault<UOpenXRSimSettings>()->bEnableSimulator && bHmdEnabled;
}

void FOpenXRSimHMD::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
	InViewFamily.EngineShowFlags.MotionBlur = 0;
	InViewFamily.EngineShowFlags.HMDDistortion = false;
	InViewFamily.EngineShowFlags.StereoRendering = IsStereoEnabled();
}

void FOpenXRSimHMD::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
}

void FOpenXRSimHMD::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
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
		bRoomStartupDone = false;
		bReplayStartupDone = false;
		bPoseStartupDone = false;
	}

	if (bStartupInitialized)
	{
		return;
	}

	const UOpenXRSimSettings* S = GetDefault<UOpenXRSimSettings>();
	if (!bRoomStartupDone)
	{
		if (!S->bLoadRoomOnWorldStart)
		{
			bRoomStartupDone = true;
		}
		else if (UOpenXRSimRoomSubsystem* Rooms = World->GetSubsystem<UOpenXRSimRoomSubsystem>())
		{
			Rooms->LoadDefaultRoomFromSettings();
			bRoomStartupDone = true;
		}
	}

	if (!bReplayStartupDone)
	{
		if (!S->bAutoReplayOnWorldStart || !SimState.IsValid())
		{
			bReplayStartupDone = true;
		}
		else
		{
			const FString ReplayPath = ResolveConfiguredPath(S->AutoReplayPath);
			if (ReplayPath.IsEmpty())
			{
				bReplayStartupDone = true;
			}
			else
			{
				SimState->StartReplay(ReplayPath);
				bReplayStartupDone = true;
			}
		}
	}

	if (!bPoseStartupDone)
	{
		if (!S->bAlignToPlayerStartOnWorldStart || !SimState.IsValid())
		{
			bPoseStartupDone = true;
		}
		else
		{
			bPoseStartupDone = TryAlignSimPoseToPlayer(World);
		}
	}

	bStartupInitialized = bRoomStartupDone && bReplayStartupDone && bPoseStartupDone;
}

FVector FOpenXRSimHMD::GetTrackingOriginOffsetCm() const
{
	// All supported origins in this simulator use floor-level authored poses.
	return FVector::ZeroVector;
}

bool FOpenXRSimHMD::TryAlignSimPoseToPlayer(UWorld* World)
{
	if (!World || !SimState.IsValid())
	{
		return false;
	}

	const UOpenXRSimSettings* S = GetDefault<UOpenXRSimSettings>();
	FVector HmdPosCm = FVector::ZeroVector;
	FQuat HmdOri = FQuat::Identity;
	bool bFoundPose = false;

	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			HmdPosCm = Pawn->GetActorLocation();
			HmdPosCm.Z += S->DefaultStandingHeightCm;

			FRotator Facing = PC->GetControlRotation();
			if (Facing.IsNearlyZero())
			{
				Facing = Pawn->GetActorRotation();
			}
			Facing.Pitch = 0.0f;
			Facing.Roll = 0.0f;
			HmdOri = Facing.Quaternion();
			bFoundPose = true;
		}
	}

	if (!bFoundPose)
	{
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			const APlayerStart* PlayerStart = *It;
			HmdPosCm = PlayerStart->GetActorLocation();
			HmdPosCm.Z += S->DefaultStandingHeightCm;

			FRotator Facing = PlayerStart->GetActorRotation();
			Facing.Pitch = 0.0f;
			Facing.Roll = 0.0f;
			HmdOri = Facing.Quaternion();
			bFoundPose = true;
			break;
		}
	}

	if (!bFoundPose)
	{
		return false;
	}

	FOpenXRSimPose HmdPose;
	HmdPose.PositionCm = HmdPosCm;
	HmdPose.Orientation = HmdOri;
	SimState->SetDevicePose(EOpenXRSimDevice::HMD, HmdPose);

	const FQuat YawQuat(FVector::UpVector, FMath::DegreesToRadians(HmdOri.Rotator().Yaw));

	FOpenXRSimPose LeftPose;
	LeftPose.PositionCm = HmdPosCm + YawQuat.RotateVector(S->DefaultLeftControllerOffsetCm);
	LeftPose.Orientation = HmdOri;
	SimState->SetDevicePose(EOpenXRSimDevice::Left, LeftPose);

	FOpenXRSimPose RightPose;
	RightPose.PositionCm = HmdPosCm + YawQuat.RotateVector(S->DefaultRightControllerOffsetCm);
	RightPose.Orientation = HmdOri;
	SimState->SetDevicePose(EOpenXRSimDevice::Right, RightPose);

	BasePosition = FVector::ZeroVector;
	BaseOrientation = FQuat::Identity;
	return true;
}
