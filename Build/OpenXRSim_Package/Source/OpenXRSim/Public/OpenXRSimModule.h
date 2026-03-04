// OpenXRSim: Main runtime module interface for XR + input device creation.
// Easy guide: read this file first when you need this behavior.

#pragma once

#include "Modules/ModuleManager.h"
#include "IHeadMountedDisplayModule.h"
#include "IInputDeviceModule.h"
#include "IMotionController.h"

class FOpenXRSimHMD;
class FOpenXRSimInputDevice;
class FOpenXRSimState;

class OPENXRSIM_API FOpenXRSimModule : public IHeadMountedDisplayModule
{
public:
	static FOpenXRSimModule& Get();
	static bool IsAvailable();

	// IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// IHeadMountedDisplayModule
	virtual FString GetModuleKeyName() const override;
	virtual bool IsHMDConnected() override;
	virtual TSharedPtr<class IXRTrackingSystem, ESPMode::ThreadSafe> CreateTrackingSystem() override;

	TSharedPtr<class IInputDevice> CreateInputDeviceInternal(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler);

	TSharedPtr<FOpenXRSimState, ESPMode::ThreadSafe> GetSimState() const { return SimState; }

private:
	bool IsSimulatorEnabled() const;

	TSharedPtr<FOpenXRSimHMD, ESPMode::ThreadSafe> SimHMD;
	TSharedPtr<FOpenXRSimInputDevice> SimInputDevice;
	TSharedPtr<FOpenXRSimState, ESPMode::ThreadSafe> SimState;
	TUniquePtr<IInputDeviceModule> InputDeviceFeature;
	TUniquePtr<IMotionController> MotionControllerFeature;
};
