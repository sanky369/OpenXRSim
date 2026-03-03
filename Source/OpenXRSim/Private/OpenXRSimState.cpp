// OpenXRSim: Core simulation state update logic with record/replay support.
// Easy guide: read this file first when you need this behavior.

#include "OpenXRSimState.h"
#include "OpenXRSimLog.h"
#include "OpenXRSimSettings.h"
#include "Engine/World.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY(LogOpenXRSim);

static TSharedPtr<FJsonObject> PoseToJson(const FOpenXRSimPose& Pose)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetArrayField(TEXT("p"), { MakeShared<FJsonValueNumber>(Pose.PositionCm.X),
	                                MakeShared<FJsonValueNumber>(Pose.PositionCm.Y),
	                                MakeShared<FJsonValueNumber>(Pose.PositionCm.Z) });
	Obj->SetArrayField(TEXT("q"), { MakeShared<FJsonValueNumber>(Pose.Orientation.X),
	                                MakeShared<FJsonValueNumber>(Pose.Orientation.Y),
	                                MakeShared<FJsonValueNumber>(Pose.Orientation.Z),
	                                MakeShared<FJsonValueNumber>(Pose.Orientation.W) });
	return Obj;
}

static bool JsonToPose(const TSharedPtr<FJsonObject>& Obj, FOpenXRSimPose& OutPose)
{
	if (!Obj.IsValid()) return false;

	const TArray<TSharedPtr<FJsonValue>>* P = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Q = nullptr;
	if (!Obj->TryGetArrayField(TEXT("p"), P) || !Obj->TryGetArrayField(TEXT("q"), Q)) return false;
	if (P->Num() != 3 || Q->Num() != 4) return false;

	OutPose.PositionCm = FVector(
		(float)(*P)[0]->AsNumber(),
		(float)(*P)[1]->AsNumber(),
		(float)(*P)[2]->AsNumber()
	);

	OutPose.Orientation = FQuat(
		(float)(*Q)[0]->AsNumber(),
		(float)(*Q)[1]->AsNumber(),
		(float)(*Q)[2]->AsNumber(),
		(float)(*Q)[3]->AsNumber()
	);
	OutPose.Orientation.Normalize();
	return true;
}

static TSharedPtr<FJsonObject> ControllerToJson(const FOpenXRSimControllerState& C)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetObjectField(TEXT("gripPose"), PoseToJson(C.GripPose));
	Obj->SetObjectField(TEXT("aimPose"), PoseToJson(C.AimPose));
	Obj->SetNumberField(TEXT("trigger"), C.Trigger);
	Obj->SetNumberField(TEXT("grip"), C.Grip);
	Obj->SetArrayField(TEXT("stick"), { MakeShared<FJsonValueNumber>(C.Thumbstick.X),
	                                    MakeShared<FJsonValueNumber>(C.Thumbstick.Y) });
	Obj->SetBoolField(TEXT("primary"), C.bPrimaryButton);
	Obj->SetBoolField(TEXT("secondary"), C.bSecondaryButton);
	Obj->SetBoolField(TEXT("stickClick"), C.bThumbstickClick);
	return Obj;
}

static bool JsonToController(const TSharedPtr<FJsonObject>& Obj, FOpenXRSimControllerState& Out)
{
	if (!Obj.IsValid()) return false;

	TSharedPtr<FJsonObject> GripPoseObj = Obj->GetObjectField(TEXT("gripPose"));
	TSharedPtr<FJsonObject> AimPoseObj  = Obj->GetObjectField(TEXT("aimPose"));
	if (!JsonToPose(GripPoseObj, Out.GripPose)) return false;
	if (!JsonToPose(AimPoseObj,  Out.AimPose)) return false;

	Out.Trigger = (float)Obj->GetNumberField(TEXT("trigger"));
	Out.Grip    = (float)Obj->GetNumberField(TEXT("grip"));

	const TArray<TSharedPtr<FJsonValue>>* Stick = nullptr;
	if (Obj->TryGetArrayField(TEXT("stick"), Stick) && Stick->Num() == 2)
	{
		Out.Thumbstick.X = (float)(*Stick)[0]->AsNumber();
		Out.Thumbstick.Y = (float)(*Stick)[1]->AsNumber();
	}

	Out.bPrimaryButton   = Obj->GetBoolField(TEXT("primary"));
	Out.bSecondaryButton = Obj->GetBoolField(TEXT("secondary"));
	Out.bThumbstickClick = Obj->GetBoolField(TEXT("stickClick"));
	return true;
}

FOpenXRSimState::FOpenXRSimState()
{
	// Sensible default poses (in cm)
	HMDPose.PositionCm = FVector(0, 0, 170);
	HMDPose.Orientation = FQuat::Identity;

	Left.GripPose.PositionCm  = FVector(-20, 20, 140);
	Right.GripPose.PositionCm = FVector( 20, 20, 140);
	Left.AimPose = Left.GripPose;
	Right.AimPose = Right.GripPose;

	ActiveRoomName = TEXT("Hero_SmallRoom");
	SimTimeSeconds = 0.0f;
	InstanceLabel = FString::Printf(TEXT("PID-%u"), FPlatformProcess::GetCurrentProcessId());
}

void FOpenXRSimState::Tick(UWorld* World, float DeltaSeconds)
{
	const UOpenXRSimSettings* Settings = GetDefault<UOpenXRSimSettings>();

	FScopeLock Lock(&Mutex);
	SimTimeSeconds += FMath::Max(0.0f, DeltaSeconds);
	if (World)
	{
		// PIE worlds usually carry UEDPIE_X in package names.
		const FString WorldName = World->GetName();
		InstanceLabel = FString::Printf(TEXT("PID-%u|%s"), FPlatformProcess::GetCurrentProcessId(), *WorldName);
	}

	// Replay overrides live state
	if (bReplaying)
	{
		const float Step = Settings->bReplayUseFixedTimestep ? ReplayFixedDt : DeltaSeconds;
		ReplayTime += Step;

		// advance to the frame that matches ReplayTime (no interpolation in MVP)
		while (ReplayFrameIndex + 1 < Frames.Num() && Frames[ReplayFrameIndex + 1].TimeSeconds <= ReplayTime)
		{
			ReplayFrameIndex++;
		}

		if (Frames.IsValidIndex(ReplayFrameIndex))
		{
			ApplyFrame(Frames[ReplayFrameIndex]);
		}

		// Stop when complete
		if (ReplayFrameIndex >= Frames.Num() - 1)
		{
			UE_LOG(LogOpenXRSim, Log, TEXT("Replay complete (%d frames)."), Frames.Num());
			bReplaying = false;
		}
	}

	// Recording samples the *current* state at fixed dt
	if (bRecording)
	{
		RecordAccumulator += FMath::Max(0.0f, DeltaSeconds);
		const float Step = ReplayFixedDt;
		while (RecordAccumulator >= Step)
		{
			RecordAccumulator -= Step;
			const float CurrentT = Frames.Num() == 0 ? 0.0f : (Frames.Last().TimeSeconds + Step);
			CaptureFrame(CurrentT);
		}
	}
}

FOpenXRSimPose FOpenXRSimState::GetHMDPose() const
{
	FScopeLock Lock(&Mutex);
	return HMDPose;
}

FOpenXRSimControllerState FOpenXRSimState::GetLeftController() const
{
	FScopeLock Lock(&Mutex);
	return Left;
}

FOpenXRSimControllerState FOpenXRSimState::GetRightController() const
{
	FScopeLock Lock(&Mutex);
	return Right;
}

void FOpenXRSimState::ApplyPoseDelta(EOpenXRSimDevice Device, const FVector& DeltaPosCm, const FQuat& DeltaRot)
{
	FScopeLock Lock(&Mutex);

	auto Apply = [&](FOpenXRSimPose& P)
	{
		P.PositionCm += DeltaPosCm;
		P.Orientation = (DeltaRot * P.Orientation).GetNormalized();
	};

	switch (Device)
	{
	case EOpenXRSimDevice::HMD:
		Apply(HMDPose);
		break;
	case EOpenXRSimDevice::Left:
		Apply(Left.GripPose);
		Left.AimPose = Left.GripPose;
		break;
	case EOpenXRSimDevice::Right:
		Apply(Right.GripPose);
		Right.AimPose = Right.GripPose;
		break;
	default:
		break;
	}
}

void FOpenXRSimState::SetDevicePose(EOpenXRSimDevice Device, const FOpenXRSimPose& Pose)
{
	FScopeLock Lock(&Mutex);

	switch (Device)
	{
	case EOpenXRSimDevice::HMD:
		HMDPose = Pose;
		break;
	case EOpenXRSimDevice::Left:
		Left.GripPose = Pose;
		Left.AimPose = Pose;
		break;
	case EOpenXRSimDevice::Right:
		Right.GripPose = Pose;
		Right.AimPose = Pose;
		break;
	default:
		break;
	}
}

void FOpenXRSimState::SetControllerActions(EOpenXRSimDevice WhichController, float Trigger, float Grip,
	const FVector2D& Stick, bool bPrimary, bool bSecondary, bool bStickClick)
{
	FScopeLock Lock(&Mutex);

	FOpenXRSimControllerState* C = nullptr;
	if (WhichController == EOpenXRSimDevice::Left)  C = &Left;
	if (WhichController == EOpenXRSimDevice::Right) C = &Right;
	if (!C) return;

	C->Trigger = FMath::Clamp(Trigger, 0.0f, 1.0f);
	C->Grip    = FMath::Clamp(Grip,    0.0f, 1.0f);
	C->Thumbstick.X = FMath::Clamp(Stick.X, -1.0f, 1.0f);
	C->Thumbstick.Y = FMath::Clamp(Stick.Y, -1.0f, 1.0f);
	C->bPrimaryButton = bPrimary;
	C->bSecondaryButton = bSecondary;
	C->bThumbstickClick = bStickClick;
}

void FOpenXRSimState::SetControllerState(EOpenXRSimDevice WhichController, const FOpenXRSimControllerState& InState)
{
	FScopeLock Lock(&Mutex);

	if (WhichController == EOpenXRSimDevice::Left)
	{
		Left = InState;
	}
	else if (WhichController == EOpenXRSimDevice::Right)
	{
		Right = InState;
	}
}

bool FOpenXRSimState::StartRecording(const FString& AbsolutePath)
{
	FScopeLock Lock(&Mutex);

	if (bReplaying)
	{
		UE_LOG(LogOpenXRSim, Warning, TEXT("Cannot start recording while replay is active."));
		return false;
	}

	if (bRecording)
	{
		UE_LOG(LogOpenXRSim, Warning, TEXT("Already recording."));
		return false;
	}

	Frames.Reset();
	RecordAccumulator = 0.0f;
	bRecording = true;
	RecordPath = AbsolutePath;
	CaptureFrame(0.0f);

	UE_LOG(LogOpenXRSim, Log, TEXT("Recording started: %s"), *RecordPath);
	return true;
}

bool FOpenXRSimState::StopRecording()
{
	FScopeLock Lock(&Mutex);

	if (!bRecording) return false;
	bRecording = false;

	const bool bSaved = SaveFramesToFile(RecordPath);
	UE_LOG(LogOpenXRSim, Log, TEXT("Recording stopped. Saved=%s Frames=%d"),
		bSaved ? TEXT("true") : TEXT("false"),
		Frames.Num());
	return bSaved;
}

bool FOpenXRSimState::StartReplay(const FString& AbsolutePath)
{
	FScopeLock Lock(&Mutex);

	if (bRecording)
	{
		UE_LOG(LogOpenXRSim, Warning, TEXT("Cannot start replay while recording is active."));
		return false;
	}

	if (bReplaying)
	{
		UE_LOG(LogOpenXRSim, Warning, TEXT("Already replaying."));
		return false;
	}

	if (!LoadFramesFromFile(AbsolutePath))
	{
		UE_LOG(LogOpenXRSim, Error, TEXT("Failed to load replay file: %s"), *AbsolutePath);
		return false;
	}

	ReplayPath = AbsolutePath;
	ReplayTime = 0.0f;
	ReplayFrameIndex = 0;
	RecordAccumulator = 0.0f;
	bReplaying = true;

	UE_LOG(LogOpenXRSim, Log, TEXT("Replay started: %s Frames=%d"), *ReplayPath, Frames.Num());
	return true;
}

bool FOpenXRSimState::StopReplay()
{
	FScopeLock Lock(&Mutex);

	if (!bReplaying) return false;
	bReplaying = false;
	UE_LOG(LogOpenXRSim, Log, TEXT("Replay stopped."));
	return true;
}

bool FOpenXRSimState::IsRecording() const
{
	FScopeLock Lock(&Mutex);
	return bRecording;
}

bool FOpenXRSimState::IsReplaying() const
{
	FScopeLock Lock(&Mutex);
	return bReplaying;
}

void FOpenXRSimState::SetActiveRoomName(const FString& InRoomName)
{
	FScopeLock Lock(&Mutex);
	ActiveRoomName = InRoomName;
}

FString FOpenXRSimState::GetActiveRoomName() const
{
	FScopeLock Lock(&Mutex);
	return ActiveRoomName;
}

void FOpenXRSimState::SetReplayFixedDt(float InFixedDt)
{
	FScopeLock Lock(&Mutex);
	ReplayFixedDt = FMath::Clamp(InFixedDt, 0.001f, 0.1f);
}

float FOpenXRSimState::GetReplayFixedDt() const
{
	FScopeLock Lock(&Mutex);
	return ReplayFixedDt;
}

float FOpenXRSimState::GetSimTimeSeconds() const
{
	FScopeLock Lock(&Mutex);
	return SimTimeSeconds;
}

FString FOpenXRSimState::GetInstanceLabel() const
{
	FScopeLock Lock(&Mutex);
	return InstanceLabel;
}

void FOpenXRSimState::CaptureFrame(float TimeSeconds)
{
	FOpenXRSimFrame Frame;
	Frame.TimeSeconds = TimeSeconds;
	Frame.HMD = HMDPose;
	Frame.Left = Left;
	Frame.Right = Right;

	Frames.Add(MoveTemp(Frame));
}

void FOpenXRSimState::ApplyFrame(const FOpenXRSimFrame& Frame)
{
	HMDPose = Frame.HMD;
	Left    = Frame.Left;
	Right   = Frame.Right;
}

bool FOpenXRSimState::SaveFramesToFile(const FString& AbsolutePath) const
{
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), 1);
	Root->SetNumberField(TEXT("fixed_dt"), ReplayFixedDt);
	Root->SetStringField(TEXT("room"), ActiveRoomName);

	TArray<TSharedPtr<FJsonValue>> FramesJson;
	for (const FOpenXRSimFrame& F : Frames)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetNumberField(TEXT("t"), F.TimeSeconds);
		Obj->SetObjectField(TEXT("hmd"), PoseToJson(F.HMD));
		Obj->SetObjectField(TEXT("l"), ControllerToJson(F.Left));
		Obj->SetObjectField(TEXT("r"), ControllerToJson(F.Right));
		FramesJson.Add(MakeShared<FJsonValueObject>(Obj));
	}
	Root->SetArrayField(TEXT("frames"), FramesJson);

	FString OutStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutStr);
	if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
	{
		return false;
	}

	return FFileHelper::SaveStringToFile(OutStr, *AbsolutePath);
}

bool FOpenXRSimState::LoadFramesFromFile(const FString& AbsolutePath)
{
	FString InStr;
	if (!FFileHelper::LoadFileToString(InStr, *AbsolutePath))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InStr);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	double FixedDtValue = ReplayFixedDt;
	if (Root->TryGetNumberField(TEXT("fixed_dt"), FixedDtValue))
	{
		ReplayFixedDt = FMath::Clamp((float)FixedDtValue, 0.001f, 0.1f);
	}

	FString RoomNameValue;
	if (Root->TryGetStringField(TEXT("room"), RoomNameValue))
	{
		ActiveRoomName = RoomNameValue;
	}

	const TArray<TSharedPtr<FJsonValue>>* FramesArr = nullptr;
	if (!Root->TryGetArrayField(TEXT("frames"), FramesArr))
	{
		return false;
	}

	Frames.Reset();
	for (const TSharedPtr<FJsonValue>& V : *FramesArr)
	{
		TSharedPtr<FJsonObject> Obj = V->AsObject();
		if (!Obj.IsValid()) continue;

		FOpenXRSimFrame F;
		double TimeValue = 0.0;
		if (!Obj->TryGetNumberField(TEXT("t"), TimeValue)) continue;
		F.TimeSeconds = (float)TimeValue;

		const TSharedPtr<FJsonObject>* HmdObj = nullptr;
		const TSharedPtr<FJsonObject>* LeftObj = nullptr;
		const TSharedPtr<FJsonObject>* RightObj = nullptr;
		if (!Obj->TryGetObjectField(TEXT("hmd"), HmdObj) || !HmdObj->IsValid()) continue;
		if (!Obj->TryGetObjectField(TEXT("l"), LeftObj) || !LeftObj->IsValid()) continue;
		if (!Obj->TryGetObjectField(TEXT("r"), RightObj) || !RightObj->IsValid()) continue;

		if (!JsonToPose(*HmdObj, F.HMD)) continue;
		if (!JsonToController(*LeftObj, F.Left)) continue;
		if (!JsonToController(*RightObj, F.Right)) continue;

		Frames.Add(MoveTemp(F));
	}

	return Frames.Num() > 0;
}
