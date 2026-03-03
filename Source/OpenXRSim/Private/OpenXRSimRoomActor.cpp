// OpenXRSim: Procedural room/anchor geometry construction from built-in or JSON data.
// Easy guide: read this file first when you need this behavior.

#include "OpenXRSimRoomActor.h"
#include "OpenXRSimLog.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "DrawDebugHelpers.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"

AOpenXRSimRoomActor::AOpenXRSimRoomActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	InstancedCubes = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("InstancedCubes"));
	InstancedCubes->SetupAttachment(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		InstancedCubes->SetStaticMesh(CubeMesh.Object);
	}
	InstancedCubes->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	InstancedCubes->SetMobility(EComponentMobility::Movable);
}

void AOpenXRSimRoomActor::SetDebugDraw(bool bEnable)
{
	bDebugDraw = bEnable;
}

void AOpenXRSimRoomActor::ClearInstances()
{
	if (InstancedCubes)
	{
		InstancedCubes->ClearInstances();
	}
}

void AOpenXRSimRoomActor::AddBoxCm(const FVector& CenterCm, const FVector& SizeCm)
{
	// Engine cube is 100cm size by default. Scale accordingly.
	const FVector Scale = SizeCm / 100.0f;

	FTransform T;
	T.SetLocation(CenterCm);
	T.SetRotation(FQuat::Identity);
	T.SetScale3D(Scale);

	InstancedCubes->AddInstance(T);
}

void AOpenXRSimRoomActor::BuildHeroSmallRoom()
{
	ClearInstances();

	// Simple 4m x 4m x 2.5m room in cm
	const FVector RoomMin(-200, -200, 0);
	const FVector RoomMax( 200,  200, 250);

	// Floor
	AddBoxCm(FVector(0,0,-5), FVector(400,400,10));
	// Ceiling
	AddBoxCm(FVector(0,0,255), FVector(400,400,10));
	// Walls (10cm thick)
	AddBoxCm(FVector(0, 205, 125), FVector(400,10,250));
	AddBoxCm(FVector(0,-205, 125), FVector(400,10,250));
	AddBoxCm(FVector(205,0, 125), FVector(10,400,250));
	AddBoxCm(FVector(-205,0, 125), FVector(10,400,250));

	// Furniture box
	AddBoxCm(FVector(60, 0, 40), FVector(120,60,80)); // table-ish
	AddBoxCm(FVector(-80, -60, 45), FVector(60,60,90)); // cabinet-ish

	DebugDrawBounds(RoomMin, RoomMax);
	DebugDrawAnchor(TEXT("spawn"), FVector(0, 0, 0));
}

void AOpenXRSimRoomActor::DebugDrawBounds(const FVector& MinCm, const FVector& MaxCm)
{
	if (!bDebugDraw) return;

	const FVector Center = (MinCm + MaxCm) * 0.5f;
	const FVector Extents = (MaxCm - MinCm) * 0.5f;

	DrawDebugBox(GetWorld(), Center, Extents, FColor::Green, false, 10.0f, 0, 2.0f);
}

void AOpenXRSimRoomActor::DebugDrawAnchor(const FString& Id, const FVector& PositionCm)
{
	if (!bDebugDraw)
	{
		return;
	}

	DrawDebugSphere(GetWorld(), PositionCm, 10.0f, 12, FColor::Yellow, false, 10.0f, 0, 1.5f);
	DrawDebugCoordinateSystem(GetWorld(), PositionCm, FRotator::ZeroRotator, 20.0f, false, 10.0f, 0, 1.0f);
	DrawDebugString(GetWorld(), PositionCm + FVector(0.0f, 0.0f, 20.0f), Id, nullptr, FColor::Yellow, 10.0f);
}

bool AOpenXRSimRoomActor::BuildFromJson(const TSharedPtr<FJsonObject>& RootObj)
{
	if (!RootObj.IsValid()) return false;

	ClearInstances();

	// Bounds
	const TSharedPtr<FJsonObject>* Bounds = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* MinArr = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* MaxArr = nullptr;

	if (RootObj->TryGetObjectField(TEXT("bounds"), Bounds) && Bounds && Bounds->IsValid()
		&& (*Bounds)->TryGetArrayField(TEXT("min"), MinArr) && (*Bounds)->TryGetArrayField(TEXT("max"), MaxArr)
		&& MinArr->Num() == 3 && MaxArr->Num() == 3)
	{
		const FVector MinCm((float)(*MinArr)[0]->AsNumber(), (float)(*MinArr)[1]->AsNumber(), (float)(*MinArr)[2]->AsNumber());
		const FVector MaxCm((float)(*MaxArr)[0]->AsNumber(), (float)(*MaxArr)[1]->AsNumber(), (float)(*MaxArr)[2]->AsNumber());
		DebugDrawBounds(MinCm, MaxCm);
	}

	// Boxes
	const TArray<TSharedPtr<FJsonValue>>* Boxes = nullptr;
	if (RootObj->TryGetArrayField(TEXT("boxes"), Boxes))
	{
		for (const TSharedPtr<FJsonValue>& V : *Boxes)
		{
			TSharedPtr<FJsonObject> Box = V->AsObject();
			if (!Box.IsValid()) continue;

			const TArray<TSharedPtr<FJsonValue>>* CenterArr = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* SizeArr = nullptr;

			if (Box->TryGetArrayField(TEXT("center"), CenterArr) && Box->TryGetArrayField(TEXT("size"), SizeArr)
				&& CenterArr->Num() == 3 && SizeArr->Num() == 3)
			{
				const FVector CenterCm((float)(*CenterArr)[0]->AsNumber(), (float)(*CenterArr)[1]->AsNumber(), (float)(*CenterArr)[2]->AsNumber());
				const FVector SizeCm((float)(*SizeArr)[0]->AsNumber(), (float)(*SizeArr)[1]->AsNumber(), (float)(*SizeArr)[2]->AsNumber());
				AddBoxCm(CenterCm, SizeCm);
			}
		}
	}

	// Planes (MVP: optionally convert to thin boxes)
	const TArray<TSharedPtr<FJsonValue>>* Planes = nullptr;
	if (RootObj->TryGetArrayField(TEXT("planes"), Planes))
	{
		for (const TSharedPtr<FJsonValue>& V : *Planes)
		{
			TSharedPtr<FJsonObject> P = V->AsObject();
			if (!P.IsValid()) continue;

			const FString Type = P->GetStringField(TEXT("type"));
			const TArray<TSharedPtr<FJsonValue>>* CenterArr = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* ExtArr = nullptr;

			if (P->TryGetArrayField(TEXT("center"), CenterArr) && P->TryGetArrayField(TEXT("extents"), ExtArr)
				&& CenterArr->Num() == 3 && ExtArr->Num() == 2)
			{
				const FVector CenterCm((float)(*CenterArr)[0]->AsNumber(), (float)(*CenterArr)[1]->AsNumber(), (float)(*CenterArr)[2]->AsNumber());
				const float Ex = (float)(*ExtArr)[0]->AsNumber();
				const float Ey = (float)(*ExtArr)[1]->AsNumber();

				// thickness 5cm
				if (Type == TEXT("floor") || Type == TEXT("ceiling"))
				{
					AddBoxCm(CenterCm, FVector(Ex, Ey, 5));
				}
				else
				{
					// wall: treat extents as width/height
					AddBoxCm(CenterCm, FVector(Ex, 5, Ey));
				}
			}
		}
	}

	// Anchors
	const TArray<TSharedPtr<FJsonValue>>* Anchors = nullptr;
	if (RootObj->TryGetArrayField(TEXT("anchors"), Anchors))
	{
		for (const TSharedPtr<FJsonValue>& V : *Anchors)
		{
			const TSharedPtr<FJsonObject> AnchorObj = V->AsObject();
			if (!AnchorObj.IsValid())
			{
				continue;
			}

			FString AnchorId = TEXT("anchor");
			AnchorObj->TryGetStringField(TEXT("id"), AnchorId);

			const TSharedPtr<FJsonObject>* PoseObj = nullptr;
			if (!AnchorObj->TryGetObjectField(TEXT("pose"), PoseObj) || !PoseObj || !PoseObj->IsValid())
			{
				continue;
			}

			const TArray<TSharedPtr<FJsonValue>>* Position = nullptr;
			if ((*PoseObj)->TryGetArrayField(TEXT("p"), Position) && Position->Num() == 3)
			{
				const FVector PositionCm(
					(float)(*Position)[0]->AsNumber(),
					(float)(*Position)[1]->AsNumber(),
					(float)(*Position)[2]->AsNumber());
				DebugDrawAnchor(AnchorId, PositionCm);
			}
		}
	}

	return true;
}
