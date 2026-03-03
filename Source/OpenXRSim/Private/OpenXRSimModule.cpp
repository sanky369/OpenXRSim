// OpenXRSim: Runtime module startup/shutdown and simulator object wiring.
// Easy guide: read this file first when you need this behavior.

#include "OpenXRSimModule.h"
#include "OpenXRSimHMD.h"
#include "OpenXRSimInputDevice.h"
#include "OpenXRSimState.h"
#include "OpenXRSimSettings.h"
#include "OpenXRSimLog.h"

#include "IModularFeatures.h"
#include "SceneViewExtension.h"

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

	// Register as modular features so the engine can find us.
	IModularFeatures::Get().RegisterModularFeature(IHeadMountedDisplayModule::GetModularFeatureName(), this);
	IModularFeatures::Get().RegisterModularFeature(IInputDeviceModule::GetModularFeatureName(), this);
}

void FOpenXRSimModule::ShutdownModule()
{
	UE_LOG(LogOpenXRSim, Log, TEXT("OpenXRSim module shutdown"));

	IModularFeatures::Get().UnregisterModularFeature(IInputDeviceModule::GetModularFeatureName(), this);
	IModularFeatures::Get().UnregisterModularFeature(IHeadMountedDisplayModule::GetModularFeatureName(), this);

	SimInputDevice.Reset();
	SimHMD.Reset();
	SimState.Reset();
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

TSharedPtr<IInputDevice> FOpenXRSimModule::CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
{
	if (!SimInputDevice.IsValid())
	{
		SimInputDevice = MakeShared<FOpenXRSimInputDevice>(InMessageHandler, SimState);
		UE_LOG(LogOpenXRSim, Log, TEXT("Created OpenXRSim InputDevice"));
	}
	return SimInputDevice;
}
