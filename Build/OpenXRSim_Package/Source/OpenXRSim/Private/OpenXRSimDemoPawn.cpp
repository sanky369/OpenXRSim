// OpenXRSim: Demo pawn behavior for runtime sanity checks in PIE/VR Preview.
// Easy guide: read this file first when you need this behavior.

#include "OpenXRSimDemoPawn.h"
#include "Camera/CameraComponent.h"
#include "MotionControllerComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "OpenXRSimLog.h"
#include "InputCoreTypes.h"
#include "GameFramework/PlayerController.h"

AOpenXRSimDemoPawn::AOpenXRSimDemoPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("VRCamera"));
	Camera->SetupAttachment(Root);

	LeftMC = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("LeftMC"));
	LeftMC->SetupAttachment(Root);
	LeftMC->SetTrackingMotionSource(FName("Left"));

	RightMC = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("RightMC"));
	RightMC->SetupAttachment(Root);
	RightMC->SetTrackingMotionSource(FName("Right"));

	LeftVis = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftVis"));
	LeftVis->SetupAttachment(LeftMC);

	RightVis = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightVis"));
	RightVis->SetupAttachment(RightMC);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		LeftVis->SetStaticMesh(CubeMesh.Object);
		RightVis->SetStaticMesh(CubeMesh.Object);

		LeftVis->SetRelativeScale3D(FVector(0.05f));
		RightVis->SetRelativeScale3D(FVector(0.05f));
	}
}

void AOpenXRSimDemoPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Example “OpenXR-style” input path: read MotionController trigger axis
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	const float LegacyLT = PC->GetInputAnalogKeyState(FKey(FName(TEXT("MotionController_Left_TriggerAxis"))));
	const float LegacyRT = PC->GetInputAnalogKeyState(FKey(FName(TEXT("MotionController_Right_TriggerAxis"))));
	const float OculusLT = PC->GetInputAnalogKeyState(FKey(FName(TEXT("OculusTouch_Left_Trigger_Axis"))));
	const float OculusRT = PC->GetInputAnalogKeyState(FKey(FName(TEXT("OculusTouch_Right_Trigger_Axis"))));
	const float LT = FMath::Max(LegacyLT, OculusLT);
	const float RT = FMath::Max(LegacyRT, OculusRT);

	if (LT > 0.8f || RT > 0.8f)
	{
		// Tiny feedback so you can verify triggers are being injected.
		UE_LOG(LogOpenXRSim, Verbose, TEXT("Trigger pressed LT=%.2f RT=%.2f"), LT, RT);
	}
}
