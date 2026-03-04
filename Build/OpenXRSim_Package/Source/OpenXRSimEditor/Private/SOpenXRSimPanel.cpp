// OpenXRSim: Slate panel UI implementation for rooms, replay, and status.
// Easy guide: read this file first when you need this behavior.

#include "SOpenXRSimPanel.h"

#include "OpenXRSimModule.h"
#include "OpenXRSimState.h"
#include "OpenXRSimSettings.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"

#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Editor.h"
#include "OpenXRSimRoomSubsystem.h"
#include "IXRTrackingSystem.h"
#include "StereoRendering.h"

static FString ResolvePanelConfiguredPath(const FString& ConfigPath)
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

void SOpenXRSimPanel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(4)
		[
			SNew(STextBlock)
			.Text(this, &SOpenXRSimPanel::GetStatusText)
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(4)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().AutoWidth().Padding(2)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([]()
				{
					return GetDefault<UOpenXRSimSettings>()->bEnableSimulator ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([](ECheckBoxState NewState)
				{
					UOpenXRSimSettings* Settings = GetMutableDefault<UOpenXRSimSettings>();
					Settings->bEnableSimulator = (NewState == ECheckBoxState::Checked);
					Settings->SaveConfig();

					if (GEngine && GEngine->StereoRenderingDevice.IsValid())
					{
						GEngine->StereoRenderingDevice->EnableStereo(Settings->bEnableSimulator);
					}
				})
				[
					SNew(STextBlock).Text(FText::FromString("Enable Simulator"))
				]
			]

			+ SHorizontalBox::Slot().AutoWidth().Padding(2)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([]()
				{
					return GetDefault<UOpenXRSimSettings>()->bEnableXInputGamepad ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([](ECheckBoxState NewState)
				{
					UOpenXRSimSettings* Settings = GetMutableDefault<UOpenXRSimSettings>();
					Settings->bEnableXInputGamepad = (NewState == ECheckBoxState::Checked);
					Settings->SaveConfig();
				})
				[
					SNew(STextBlock).Text(FText::FromString("Enable XInput"))
				]
			]

			+ SHorizontalBox::Slot().AutoWidth().Padding(2)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([]()
				{
					return GetDefault<UOpenXRSimSettings>()->bEnableDataForwarding ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([](ECheckBoxState NewState)
				{
					UOpenXRSimSettings* Settings = GetMutableDefault<UOpenXRSimSettings>();
					Settings->bEnableDataForwarding = (NewState == ECheckBoxState::Checked);
					Settings->SaveConfig();
				})
				[
					SNew(STextBlock).Text(FText::FromString("Enable UDP Forwarding"))
				]
			]
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(4)
		[
			SNew(SButton)
			.Text(FText::FromString("Load Built-in Room"))
			.OnClicked(this, &SOpenXRSimPanel::OnLoadBuiltInRoom)
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(4)
		[
			SNew(SButton)
			.Text(FText::FromString("Load Startup Room (Settings)"))
			.OnClicked(this, &SOpenXRSimPanel::OnLoadStartupRoom)
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(4)
		[
			SNew(SButton)
			.Text(FText::FromString("Load Example JSON Room (plugin content)"))
			.OnClicked(this, &SOpenXRSimPanel::OnLoadRoomJson)
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(4)
		[
			SNew(SButton)
			.Text(FText::FromString("Reset Origin"))
			.OnClicked(this, &SOpenXRSimPanel::OnResetOrigin)
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(4)
		[
			SNew(SButton)
			.Text(FText::FromString("Start Record (10s suggested)"))
			.OnClicked(this, &SOpenXRSimPanel::OnStartRecord)
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(4)
		[
			SNew(SButton)
			.Text(FText::FromString("Stop Record"))
			.OnClicked(this, &SOpenXRSimPanel::OnStopRecord)
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(4)
		[
			SNew(SButton)
			.Text(FText::FromString("Start Replay (Example file)"))
			.OnClicked(this, &SOpenXRSimPanel::OnStartReplay)
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(4)
		[
			SNew(SButton)
			.Text(FText::FromString("Start Replay (Settings AutoReplayPath)"))
			.OnClicked(this, &SOpenXRSimPanel::OnStartReplayFromSettings)
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(4)
		[
			SNew(SButton)
			.Text(FText::FromString("Stop Replay"))
			.OnClicked(this, &SOpenXRSimPanel::OnStopReplay)
		]
	];
}

FString SOpenXRSimPanel::ResolvePluginContentPath(const FString& RelativePath) const
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("OpenXRSim"));
	if (!Plugin.IsValid())
	{
		return FString();
	}
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(Plugin->GetBaseDir(), RelativePath));
}

FString SOpenXRSimPanel::ResolveDefaultReplayPath(const FString& FileName) const
{
	const UOpenXRSimSettings* S = GetDefault<UOpenXRSimSettings>();
	const FString AbsDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), S->DefaultReplayDirRelative));
	IFileManager::Get().MakeDirectory(*AbsDir, true);
	return FPaths::Combine(AbsDir, FileName);
}

FText SOpenXRSimPanel::GetStatusText() const
{
	const UOpenXRSimSettings* S = GetDefault<UOpenXRSimSettings>();

	const FString Enabled = S->bEnableSimulator ? TEXT("ACTIVE") : TEXT("INACTIVE");
	const int32 DevIdx = S->ActiveDeviceIndex;
	const FString Gp = S->bEnableXInputGamepad ? TEXT("ON") : TEXT("OFF");
	const FString Fwd = S->bEnableDataForwarding ? TEXT("ON") : TEXT("OFF");

	FString SimDiag = TEXT("State:Unavailable");
	if (FOpenXRSimModule::IsAvailable() && FOpenXRSimModule::Get().GetSimState().IsValid())
	{
		const TSharedPtr<FOpenXRSimState, ESPMode::ThreadSafe> SimState = FOpenXRSimModule::Get().GetSimState();
		SimDiag = FString::Printf(
			TEXT("Room=%s | Rec=%s | Replay=%s | t=%.2f | Instance=%s"),
			*SimState->GetActiveRoomName(),
			SimState->IsRecording() ? TEXT("ON") : TEXT("OFF"),
			SimState->IsReplaying() ? TEXT("ON") : TEXT("OFF"),
			SimState->GetSimTimeSeconds(),
			*SimState->GetInstanceLabel());
	}

	return FText::FromString(FString::Printf(
		TEXT("OpenXRSim: %s | ActiveDeviceIndex=%d | Gamepad=%s | Forwarding=%s | %s | Controls: Alt+Mouse/WASDQE, 1/2/3 HMD/Left/Right, LMB Trigger, RMB Grip, Arrows Stick."),
		*Enabled, DevIdx, *Gp, *Fwd, *SimDiag));
}

FReply SOpenXRSimPanel::OnLoadBuiltInRoom()
{
	if (!GEditor) return FReply::Handled();

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return FReply::Handled();

	UOpenXRSimRoomSubsystem* Rooms = World->GetSubsystem<UOpenXRSimRoomSubsystem>();
	if (Rooms)
	{
		Rooms->LoadBuiltInRoom();
	}
	return FReply::Handled();
}

FReply SOpenXRSimPanel::OnLoadRoomJson()
{
	if (!GEditor) return FReply::Handled();

	const FString Path = ResolvePluginContentPath(TEXT("Content/Rooms/Example_CustomRoom.json"));

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return FReply::Handled();

	UOpenXRSimRoomSubsystem* Rooms = World->GetSubsystem<UOpenXRSimRoomSubsystem>();
	if (Rooms)
	{
		Rooms->LoadRoomFromJsonFile(Path);
	}
	return FReply::Handled();
}

FReply SOpenXRSimPanel::OnLoadStartupRoom()
{
	if (!GEditor) return FReply::Handled();

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return FReply::Handled();

	UOpenXRSimRoomSubsystem* Rooms = World->GetSubsystem<UOpenXRSimRoomSubsystem>();
	if (!Rooms) return FReply::Handled();
	Rooms->LoadDefaultRoomFromSettings();
	return FReply::Handled();
}

FReply SOpenXRSimPanel::OnResetOrigin()
{
	if (GEngine && GEngine->XRSystem.IsValid())
	{
		GEngine->XRSystem->ResetOrientationAndPosition();
	}
	return FReply::Handled();
}

FReply SOpenXRSimPanel::OnStartRecord()
{
	FOpenXRSimModule& Mod = FOpenXRSimModule::Get();
	const FString OutPath = ResolveDefaultReplayPath(TEXT("recording.json"));
	Mod.GetSimState()->StartRecording(OutPath);
	return FReply::Handled();
}

FReply SOpenXRSimPanel::OnStopRecord()
{
	FOpenXRSimModule& Mod = FOpenXRSimModule::Get();
	Mod.GetSimState()->StopRecording();
	return FReply::Handled();
}

FReply SOpenXRSimPanel::OnStartReplay()
{
	FOpenXRSimModule& Mod = FOpenXRSimModule::Get();

	// Start replay from plugin content example
	const FString Path = ResolvePluginContentPath(TEXT("Content/Replays/Example_10s_Walk.json"));
	Mod.GetSimState()->StartReplay(Path);
	return FReply::Handled();
}

FReply SOpenXRSimPanel::OnStartReplayFromSettings()
{
	FOpenXRSimModule& Mod = FOpenXRSimModule::Get();
	const UOpenXRSimSettings* S = GetDefault<UOpenXRSimSettings>();
	const FString Path = ResolvePanelConfiguredPath(S->AutoReplayPath);
	if (!Path.IsEmpty())
	{
		Mod.GetSimState()->StartReplay(Path);
	}
	return FReply::Handled();
}

FReply SOpenXRSimPanel::OnStopReplay()
{
	FOpenXRSimModule& Mod = FOpenXRSimModule::Get();
	Mod.GetSimState()->StopReplay();
	return FReply::Handled();
}
