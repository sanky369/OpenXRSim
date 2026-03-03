// OpenXRSim: XR tracking + stereo interface implementation contract.
// Easy guide: read this file first when you need this behavior.

#pragma once

#include "CoreMinimal.h"
#include "HeadMountedDisplayBase.h"
#include "XRRenderTargetManager.h"
#include "SceneViewExtension.h"
#include "OpenXRSimTypes.h"

class FOpenXRSimState;

class OPENXRSIM_API FOpenXRSimHMD final
	: public FHeadMountedDisplayBase
	, public FXRRenderTargetManager
	, public FSceneViewExtensionBase
{
public:
	FOpenXRSimHMD(const FAutoRegister& AutoRegister, TSharedPtr<FOpenXRSimState, ESPMode::ThreadSafe> InState);
	virtual ~FOpenXRSimHMD() override;

	// IXRTrackingSystem
	virtual FName GetSystemName() const override;
	virtual bool EnumerateTrackedDevices(TArray<int32>& OutDevices, EXRTrackedDeviceType Type = EXRTrackedDeviceType::Any) override;
	virtual bool GetCurrentPose(int32 DeviceId, FQuat& CurrentOrientation, FVector& CurrentPosition) override;
	virtual bool GetRelativeEyePose(int32 DeviceId, int32 ViewIndex, FQuat& OutOrientation, FVector& OutPosition) override;
	virtual bool DoesSupportPositionalTracking() const override;

	virtual IHeadMountedDisplay* GetHMDDevice() override { return this; }
	virtual TSharedPtr<IStereoRendering, ESPMode::ThreadSafe> GetStereoRenderingDevice() override { return AsShared(); }

	virtual bool OnStartGameFrame(FWorldContext& WorldContext) override;

	// IHeadMountedDisplay
	virtual bool IsHMDConnected() override;
	virtual bool IsHMDEnabled() const override;
	virtual void EnableHMD(bool bEnable = true) override;
	virtual void GetFieldOfView(float& OutHFOVInDegrees, float& OutVFOVInDegrees) const override;
	virtual FIntPoint GetIdealRenderTargetSize() const override;

	// Reset helpers
	virtual void ResetOrientationAndPosition(float Yaw = 0.f) override;

	// IStereoRendering
	virtual bool IsStereoEnabled() const override;
	virtual bool EnableStereo(bool bEnable = true) override;
	virtual void AdjustViewRect(int32 ViewIndex, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const override;
	virtual EStereoscopicPass GetViewPassForIndex(bool bStereoRequested, int32 ViewIndex) const override;
	virtual int32 GetDesiredNumberOfViews(bool bStereoRequested) const override;
	virtual FMatrix GetStereoProjectionMatrix(const int32 ViewIndex) const override;
	virtual IStereoRenderTargetManager* GetRenderTargetManager() override { return this; }
	virtual bool ShouldUseSeparateRenderTarget() const override { return false; }

	// Motion controller data (Blueprint node support)
	virtual void GetMotionControllerData(UObject* WorldContext, const EControllerHand Hand, FXRMotionControllerData& MotionControllerData) override;

protected:
	// FSceneViewExtensionBase
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;

private:
	TSharedPtr<FOpenXRSimState, ESPMode::ThreadSafe> SimState;

	bool bHmdEnabled = true;
	bool bStereoEnabled = false;

	// Base pose offsets for reset
	FQuat BaseOrientation = FQuat::Identity;
	FVector BasePosition = FVector::ZeroVector;

	// Cached world pointer for queries (game thread)
	TWeakObjectPtr<UWorld> CachedWorld;
	TWeakObjectPtr<UWorld> StartupWorld;
	bool bStartupInitialized = false;

	float GetWorldToMetersScale() const;
	void ApplyWorldStartupActions(UWorld* World);
};
