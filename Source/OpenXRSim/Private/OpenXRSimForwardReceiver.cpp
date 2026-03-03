// OpenXRSim: UDP JSON parser that applies forwarded data into simulator state.
// Easy guide: read this file first when you need this behavior.

#include "OpenXRSimForwardReceiver.h"

#include "OpenXRSimLog.h"
#include "OpenXRSimSettings.h"
#include "OpenXRSimState.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

static bool ParsePoseObject(const TSharedPtr<FJsonObject>& Obj, FOpenXRSimPose& OutPose)
{
	if (!Obj.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* P = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Q = nullptr;
	if (!Obj->TryGetArrayField(TEXT("p"), P) || !Obj->TryGetArrayField(TEXT("q"), Q))
	{
		return false;
	}
	if (P->Num() != 3 || Q->Num() != 4)
	{
		return false;
	}

	OutPose.PositionCm = FVector(
		(float)(*P)[0]->AsNumber(),
		(float)(*P)[1]->AsNumber(),
		(float)(*P)[2]->AsNumber());

	OutPose.Orientation = FQuat(
		(float)(*Q)[0]->AsNumber(),
		(float)(*Q)[1]->AsNumber(),
		(float)(*Q)[2]->AsNumber(),
		(float)(*Q)[3]->AsNumber()).GetNormalized();

	return true;
}

static void ApplyControllerFields(const TSharedPtr<FJsonObject>& Obj, FOpenXRSimControllerState& Controller)
{
	if (!Obj.IsValid())
	{
		return;
	}

	const TSharedPtr<FJsonObject>* GripPoseObj = nullptr;
	if (Obj->TryGetObjectField(TEXT("gripPose"), GripPoseObj) && GripPoseObj && GripPoseObj->IsValid())
	{
		ParsePoseObject(*GripPoseObj, Controller.GripPose);
	}

	const TSharedPtr<FJsonObject>* AimPoseObj = nullptr;
	if (Obj->TryGetObjectField(TEXT("aimPose"), AimPoseObj) && AimPoseObj && AimPoseObj->IsValid())
	{
		ParsePoseObject(*AimPoseObj, Controller.AimPose);
	}
	else
	{
		Controller.AimPose = Controller.GripPose;
	}

	double NumberValue = 0.0;
	if (Obj->TryGetNumberField(TEXT("trigger"), NumberValue))
	{
		Controller.Trigger = FMath::Clamp((float)NumberValue, 0.0f, 1.0f);
	}
	if (Obj->TryGetNumberField(TEXT("grip"), NumberValue))
	{
		Controller.Grip = FMath::Clamp((float)NumberValue, 0.0f, 1.0f);
	}

	const TArray<TSharedPtr<FJsonValue>>* Stick = nullptr;
	if (Obj->TryGetArrayField(TEXT("stick"), Stick) && Stick->Num() == 2)
	{
		Controller.Thumbstick.X = FMath::Clamp((float)(*Stick)[0]->AsNumber(), -1.0f, 1.0f);
		Controller.Thumbstick.Y = FMath::Clamp((float)(*Stick)[1]->AsNumber(), -1.0f, 1.0f);
	}

	bool BoolValue = false;
	if (Obj->TryGetBoolField(TEXT("primary"), BoolValue))
	{
		Controller.bPrimaryButton = BoolValue;
	}
	if (Obj->TryGetBoolField(TEXT("secondary"), BoolValue))
	{
		Controller.bSecondaryButton = BoolValue;
	}
	if (Obj->TryGetBoolField(TEXT("stickClick"), BoolValue))
	{
		Controller.bThumbstickClick = BoolValue;
	}
}

FOpenXRSimForwardReceiver::~FOpenXRSimForwardReceiver()
{
	ShutdownSocket();
}

void FOpenXRSimForwardReceiver::Tick(const TSharedPtr<FOpenXRSimState, ESPMode::ThreadSafe>& SimState, const UOpenXRSimSettings* Settings)
{
	if (!Settings || !Settings->bEnableDataForwarding || !SimState.IsValid())
	{
		ShutdownSocket();
		return;
	}

	EnsureSocket(Settings);
	if (!Socket)
	{
		return;
	}

	uint32 PendingBytes = 0;
	while (Socket->HasPendingData(PendingBytes))
	{
		TArray<uint8> Buffer;
		Buffer.SetNumUninitialized(FMath::Min(PendingBytes, 64u * 1024u) + 1);
		int32 ReadBytes = 0;
		ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (!Subsystem)
		{
			break;
		}
		TSharedRef<FInternetAddr> SourceAddr = Subsystem->CreateInternetAddr();
		if (!Socket->RecvFrom(Buffer.GetData(), Buffer.Num() - 1, ReadBytes, *SourceAddr) || ReadBytes <= 0)
		{
			break;
		}

		Buffer[ReadBytes] = '\0';
		const FString Payload = UTF8_TO_TCHAR(reinterpret_cast<const char*>(Buffer.GetData()));

		TSharedPtr<FJsonObject> Root;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Payload);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			continue;
		}

		const TSharedPtr<FJsonObject>* HmdObj = nullptr;
		if (Root->TryGetObjectField(TEXT("hmd"), HmdObj) && HmdObj && HmdObj->IsValid())
		{
			FOpenXRSimPose Pose;
			if (ParsePoseObject(*HmdObj, Pose))
			{
				SimState->SetDevicePose(EOpenXRSimDevice::HMD, Pose);
			}
		}

		const TSharedPtr<FJsonObject>* LeftObj = nullptr;
		if (!Root->TryGetObjectField(TEXT("left"), LeftObj))
		{
			Root->TryGetObjectField(TEXT("l"), LeftObj);
		}
		if (LeftObj && LeftObj->IsValid())
		{
			FOpenXRSimControllerState Left = SimState->GetLeftController();
			ApplyControllerFields(*LeftObj, Left);
			SimState->SetControllerState(EOpenXRSimDevice::Left, Left);
		}

		const TSharedPtr<FJsonObject>* RightObj = nullptr;
		if (!Root->TryGetObjectField(TEXT("right"), RightObj))
		{
			Root->TryGetObjectField(TEXT("r"), RightObj);
		}
		if (RightObj && RightObj->IsValid())
		{
			FOpenXRSimControllerState Right = SimState->GetRightController();
			ApplyControllerFields(*RightObj, Right);
			SimState->SetControllerState(EOpenXRSimDevice::Right, Right);
		}

		FString DeviceName;
		if (Root->TryGetStringField(TEXT("device"), DeviceName))
		{
			const FString DeviceLower = DeviceName.ToLower();
			EOpenXRSimDevice Target = EOpenXRSimDevice::HMD;
			if (DeviceLower == TEXT("left")) Target = EOpenXRSimDevice::Left;
			else if (DeviceLower == TEXT("right")) Target = EOpenXRSimDevice::Right;

			const TSharedPtr<FJsonObject>* PoseObj = nullptr;
			if (Root->TryGetObjectField(TEXT("pose"), PoseObj) && PoseObj && PoseObj->IsValid())
			{
				FOpenXRSimPose Pose;
				if (ParsePoseObject(*PoseObj, Pose))
				{
					SimState->SetDevicePose(Target, Pose);
				}
			}

			if (Target == EOpenXRSimDevice::Left || Target == EOpenXRSimDevice::Right)
			{
				FOpenXRSimControllerState C = (Target == EOpenXRSimDevice::Left) ? SimState->GetLeftController() : SimState->GetRightController();
				ApplyControllerFields(Root, C);
				SimState->SetControllerState(Target, C);
			}
		}
	}
}

void FOpenXRSimForwardReceiver::EnsureSocket(const UOpenXRSimSettings* Settings)
{
	if (!Settings)
	{
		return;
	}

	const FString DesiredEndpoint = FString::Printf(TEXT("%s:%d"), *Settings->DataForwardBindAddress, Settings->DataForwardPort);
	if (Socket && bSocketInitialized && SocketEndpoint == DesiredEndpoint)
	{
		return;
	}

	ShutdownSocket();

	ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!Subsystem)
	{
		return;
	}

	TSharedRef<FInternetAddr> BindAddr = Subsystem->CreateInternetAddr();
	bool bValidIp = false;
	BindAddr->SetIp(*Settings->DataForwardBindAddress, bValidIp);
	BindAddr->SetPort(Settings->DataForwardPort);
	if (!bValidIp)
	{
		UE_LOG(LogOpenXRSim, Warning, TEXT("Invalid forwarding bind address: %s"), *Settings->DataForwardBindAddress);
		return;
	}

	Socket = Subsystem->CreateSocket(NAME_DGram, TEXT("OpenXRSimForwardSocket"), false);
	if (!Socket)
	{
		return;
	}

	int32 BufferSize = 2 * 1024 * 1024;
	Socket->SetReuseAddr(true);
	Socket->SetNonBlocking(true);
	Socket->SetReceiveBufferSize(BufferSize, BufferSize);

	if (!Socket->Bind(*BindAddr))
	{
		UE_LOG(LogOpenXRSim, Warning, TEXT("Failed to bind forwarding socket to %s"), *DesiredEndpoint);
		ShutdownSocket();
		return;
	}

	SocketEndpoint = DesiredEndpoint;
	bSocketInitialized = true;
	UE_LOG(LogOpenXRSim, Log, TEXT("Data forwarding enabled on UDP %s"), *SocketEndpoint);
}

void FOpenXRSimForwardReceiver::ShutdownSocket()
{
	if (!Socket)
	{
		bSocketInitialized = false;
		SocketEndpoint.Reset();
		return;
	}

	ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	Socket->Close();
	if (Subsystem)
	{
		Subsystem->DestroySocket(Socket);
	}
	Socket = nullptr;
	bSocketInitialized = false;
	SocketEndpoint.Reset();
}

