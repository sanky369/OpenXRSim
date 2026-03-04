// OpenXRSim: Runtime module startup/shutdown and simulator object wiring.
// Easy guide: read this file first when you need this behavior.

#include "OpenXRSimModule.h"
#include "OpenXRSimHMD.h"
#include "OpenXRSimInputDevice.h"
#include "OpenXRSimState.h"
#include "OpenXRSimSettings.h"
#include "OpenXRSimLog.h"

#include "Features/IModularFeatures.h"
#include "SceneViewExtension.h"
#include "XRMotionControllerBase.h"
#include "Engine/Engine.h"

class FOpenXRSimMotionController final : public FXRMotionControllerBase
{
public:
	virtual FName GetMotionControllerDeviceTypeName() const override
	{
		return FName(TEXT("OpenXRSim"));
	}

	virtual bool GetControllerOrientationAndPosition(const int32 ControllerIndex, const FName MotionSource,
		FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const override
	{
		if (!GetDefault<UOpenXRSimSettings>()->bEnableSimulator || ControllerIndex != 0 || !GEngine || !GEngine->XRSystem.IsValid())
		{
			return false;
		}

		if (GEngine->XRSystem->GetSystemName() != FName(TEXT("OpenXRSim")))
		{
			return false;
		}

		auto QueryDevice = [&](int32 DeviceId) -> bool
		{
			FQuat OrientationQuat = FQuat::Identity;
			if (GEngine->XRSystem->GetCurrentPose(DeviceId, OrientationQuat, OutPosition))
			{
				OutOrientation = OrientationQuat.Rotator();
				return true;
			}
			return false;
		};

		EControllerHand Hand = EControllerHand::AnyHand;
		if (!IMotionController::GetHandEnumForSourceName(MotionSource, Hand))
		{
			if (MotionSource == FName(TEXT("LeftGrip")) || MotionSource == FName(TEXT("LeftAim")))
			{
				Hand = EControllerHand::Left;
			}
			else if (MotionSource == FName(TEXT("RightGrip")) || MotionSource == FName(TEXT("RightAim")))
			{
				Hand = EControllerHand::Right;
			}
		}

		if (Hand == EControllerHand::Left)
		{
			return QueryDevice(1);
		}
		if (Hand == EControllerHand::Right)
		{
			return QueryDevice(2);
		}
		if (Hand == EControllerHand::AnyHand)
		{
			return QueryDevice(1) || QueryDevice(2);
		}

		return false;
	}

	virtual ETrackingStatus GetControllerTrackingStatus(const int32 ControllerIndex, const FName MotionSource) const override
	{
		FRotator Orientation = FRotator::ZeroRotator;
		FVector Position = FVector::ZeroVector;
		return GetControllerOrientationAndPosition(ControllerIndex, MotionSource, Orientation, Position, 100.0f)
			? ETrackingStatus::Tracked
			: ETrackingStatus::NotTracked;
	}

	virtual void EnumerateSources(TArray<FMotionControllerSource>& SourcesOut) const override
	{
		SourcesOut.Add(FMotionControllerSource(IMotionController::LeftHandSourceId));
		SourcesOut.Add(FMotionControllerSource(IMotionController::RightHandSourceId));
		SourcesOut.Add(FMotionControllerSource(FName(TEXT("LeftGrip"))));
		SourcesOut.Add(FMotionControllerSource(FName(TEXT("RightGrip"))));
		SourcesOut.Add(FMotionControllerSource(FName(TEXT("LeftAim"))));
		SourcesOut.Add(FMotionControllerSource(FName(TEXT("RightAim"))));
	}
};

class FOpenXRSimInputDeviceFeature final : public IInputDeviceModule
{
public:
	explicit FOpenXRSimInputDeviceFeature(FOpenXRSimModule& InOwner)
		: Owner(InOwner)
	{
	}

	virtual TSharedPtr<IInputDevice> CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) override
	{
		return Owner.CreateInputDeviceInternal(InMessageHandler);
	}

private:
	FOpenXRSimModule& Owner;
};

IMPLEMENT_MODULE(FOpenXRSimModule, OpenXRSim);

FOpenXRSimModule& FOpenXRSimModule::Get()
{
	return FModuleManager::LoadModuleChecked<FOpenXRSimModule>(TEXT("OpenXRSim"));
}

bool FOpenXRSimModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded(TEXT("OpenXRSim"));
}

void FOpenXRSimModule::StartupModule()
{
	UE_LOG(LogOpenXRSim, Log, TEXT("OpenXRSim module startup"));

	SimState = MakeShared<FOpenXRSimState, ESPMode::ThreadSafe>();
	InputDeviceFeature = MakeUnique<FOpenXRSimInputDeviceFeature>(*this);
	MotionControllerFeature = MakeUnique<FOpenXRSimMotionController>();

	// Register as modular features so the engine can find us.
	IModularFeatures::Get().RegisterModularFeature(IHeadMountedDisplayModule::GetModularFeatureName(), this);
	IModularFeatures::Get().RegisterModularFeature(IInputDeviceModule::GetModularFeatureName(), InputDeviceFeature.Get());
	IModularFeatures::Get().RegisterModularFeature(IMotionController::GetModularFeatureName(), MotionControllerFeature.Get());
}

void FOpenXRSimModule::ShutdownModule()
{
	UE_LOG(LogOpenXRSim, Log, TEXT("OpenXRSim module shutdown"));

	if (InputDeviceFeature.IsValid())
	{
		IModularFeatures::Get().UnregisterModularFeature(IInputDeviceModule::GetModularFeatureName(), InputDeviceFeature.Get());
	}
	if (MotionControllerFeature.IsValid())
	{
		IModularFeatures::Get().UnregisterModularFeature(IMotionController::GetModularFeatureName(), MotionControllerFeature.Get());
	}
	IModularFeatures::Get().UnregisterModularFeature(IHeadMountedDisplayModule::GetModularFeatureName(), this);

	SimInputDevice.Reset();
	SimHMD.Reset();
	SimState.Reset();
	InputDeviceFeature.Reset();
	MotionControllerFeature.Reset();
}

FString FOpenXRSimModule::GetModuleKeyName() const
{
	return TEXT("OpenXRSim");
}

bool FOpenXRSimModule::IsSimulatorEnabled() const
{
	return GetDefault<UOpenXRSimSettings>()->bEnableSimulator;
}

bool FOpenXRSimModule::IsHMDConnected()
{
	// Engine will choose us if this returns true.
	return IsSimulatorEnabled();
}

TSharedPtr<IXRTrackingSystem, ESPMode::ThreadSafe> FOpenXRSimModule::CreateTrackingSystem()
{
	if (!IsSimulatorEnabled())
	{
		return nullptr;
	}

	if (!SimHMD.IsValid())
	{
		// Create and auto-register a SceneViewExtension-based HMD object.
		SimHMD = FSceneViewExtensions::NewExtension<FOpenXRSimHMD>(SimState);
		UE_LOG(LogOpenXRSim, Log, TEXT("Created OpenXRSim HMD tracking system"));
	}

	return SimHMD;
}

TSharedPtr<IInputDevice> FOpenXRSimModule::CreateInputDeviceInternal(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
{
	if (!SimInputDevice.IsValid())
	{
		SimInputDevice = MakeShared<FOpenXRSimInputDevice>(InMessageHandler, SimState);
		UE_LOG(LogOpenXRSim, Log, TEXT("Created OpenXRSim InputDevice"));
	}
	return SimInputDevice;
}
