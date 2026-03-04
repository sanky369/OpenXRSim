// OpenXRSim: Room loading implementation (built-in + JSON + startup defaults).
// Easy guide: read this file first when you need this behavior.

#include "OpenXRSimRoomSubsystem.h"
#include "OpenXRSimRoomActor.h"
#include "OpenXRSimModule.h"
#include "OpenXRSimState.h"
#include "OpenXRSimLog.h"
#include "OpenXRSimSettings.h"

#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Interfaces/IPluginManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

static FString ResolveRoomPathFromSettings(const FString& ConfigPath)
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

void UOpenXRSimRoomSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UOpenXRSimRoomSubsystem::Deinitialize()
{
	if (RoomActor)
	{
		RoomActor->Destroy();
		RoomActor = nullptr;
	}
	Super::Deinitialize();
}

void UOpenXRSimRoomSubsystem::EnsureRoomActor()
{
	if (RoomActor)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	FActorSpawnParameters Params;
	Params.Name = TEXT("OpenXRSimRoomActor");
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	RoomActor = World->SpawnActor<AOpenXRSimRoomActor>(AOpenXRSimRoomActor::StaticClass(), FTransform::Identity, Params);
	if (RoomActor)
	{
		RoomActor->SetDebugDraw(bDebugDraw);
	}
}

void UOpenXRSimRoomSubsystem::LoadBuiltInRoom()
{
	EnsureRoomActor();
	if (RoomActor)
	{
		RoomActor->BuildHeroSmallRoom();
		if (FOpenXRSimModule::IsAvailable() && FOpenXRSimModule::Get().GetSimState().IsValid())
		{
			FOpenXRSimModule::Get().GetSimState()->SetActiveRoomName(TEXT("Hero_SmallRoom"));
		}
	}
}

bool UOpenXRSimRoomSubsystem::LoadRoomFromJsonFile(const FString& AbsolutePath)
{
	EnsureRoomActor();
	if (!RoomActor)
	{
		return false;
	}

	FString JsonStr;
	if (!FFileHelper::LoadFileToString(JsonStr, *AbsolutePath))
	{
		UE_LOG(LogOpenXRSim, Error, TEXT("Failed to read room JSON: %s"), *AbsolutePath);
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogOpenXRSim, Error, TEXT("Failed to parse room JSON: %s"), *AbsolutePath);
		return false;
	}

	const bool bLoaded = RoomActor->BuildFromJson(Root);
	if (!bLoaded)
	{
		return false;
	}

	FString RoomName = FPaths::GetBaseFilename(AbsolutePath);
	Root->TryGetStringField(TEXT("name"), RoomName);

	if (FOpenXRSimModule::IsAvailable() && FOpenXRSimModule::Get().GetSimState().IsValid())
	{
		FOpenXRSimModule::Get().GetSimState()->SetActiveRoomName(RoomName);
	}

	return true;
}

bool UOpenXRSimRoomSubsystem::LoadDefaultRoomFromSettings()
{
	const UOpenXRSimSettings* S = GetDefault<UOpenXRSimSettings>();
	if (S->bPreferBuiltInRoomOnWorldStart)
	{
		LoadBuiltInRoom();
		return true;
	}

	const FString Path = ResolveRoomPathFromSettings(S->DefaultRoomJsonRelative);
	if (Path.IsEmpty())
	{
		LoadBuiltInRoom();
		return false;
	}

	if (!LoadRoomFromJsonFile(Path))
	{
		LoadBuiltInRoom();
		return false;
	}

	return true;
}

void UOpenXRSimRoomSubsystem::SetDebugDraw(bool bEnable)
{
	bDebugDraw = bEnable;
	if (RoomActor)
	{
		RoomActor->SetDebugDraw(bEnable);
	}
}
