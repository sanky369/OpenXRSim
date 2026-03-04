// OpenXRSim: Editor integration: tab registration, menu entries, simulator toggle.
// Easy guide: read this file first when you need this behavior.

#include "OpenXRSimEditorModule.h"
#include "OpenXRSimSettings.h"
#include "OpenXRSimLog.h"

#include "ToolMenus.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"

#include "SOpenXRSimPanel.h"

#include "Engine/Engine.h"
#include "StereoRendering.h"

static const FName OpenXRSimTabName(TEXT("OpenXRSimTab"));

IMPLEMENT_MODULE(FOpenXRSimEditorModule, OpenXRSimEditor);

void FOpenXRSimEditorModule::StartupModule()
{
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FOpenXRSimEditorModule::RegisterMenus)
	);

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(OpenXRSimTabName,
		FOnSpawnTab::CreateRaw(this, &FOpenXRSimEditorModule::SpawnSimTab))
		.SetDisplayName(FText::FromString(TEXT("OpenXR Simulator")))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FOpenXRSimEditorModule::ShutdownModule()
{
	if (UToolMenus::IsToolMenuUIEnabled())
	{
		UToolMenus::UnRegisterStartupCallback(this);
	}

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(OpenXRSimTabName);
}

TSharedRef<SDockTab> FOpenXRSimEditorModule::SpawnSimTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SOpenXRSimPanel)
		];
}

bool FOpenXRSimEditorModule::IsSimulatorEnabled() const
{
	return GetDefault<UOpenXRSimSettings>()->bEnableSimulator;
}

void FOpenXRSimEditorModule::ToggleSimulator()
{
	UOpenXRSimSettings* Settings = GetMutableDefault<UOpenXRSimSettings>();
	Settings->bEnableSimulator = !Settings->bEnableSimulator;
	Settings->SaveConfig();

	UE_LOG(LogOpenXRSim, Log, TEXT("Simulator toggled: %s"), Settings->bEnableSimulator ? TEXT("ON") : TEXT("OFF"));

	// Keep stereo state aligned when an XR system is already active.
	if (GEngine)
	{
		if (GEngine->StereoRenderingDevice.IsValid())
		{
			GEngine->StereoRenderingDevice->EnableStereo(Settings->bEnableSimulator);
		}
	}
}

void FOpenXRSimEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
	FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");

	Section.AddMenuEntry(
		"OpenXRSim.OpenPanel",
		FText::FromString("OpenXR Simulator"),
		FText::FromString("Open the OpenXR Simulator panel."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]()
		{
			FGlobalTabmanager::Get()->TryInvokeTab(OpenXRSimTabName);
		}))
	);

	UToolMenu* Toolbar = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
	FToolMenuSection& TBSection = Toolbar->FindOrAddSection("Settings");

	TBSection.AddEntry(FToolMenuEntry::InitToolBarButton(
		"OpenXRSim.Toggle",
		FUIAction(
			FExecuteAction::CreateRaw(this, &FOpenXRSimEditorModule::ToggleSimulator),
			FCanExecuteAction(),
			FIsActionChecked::CreateRaw(this, &FOpenXRSimEditorModule::IsSimulatorEnabled)
		),
		FText::FromString("OpenXR Sim"),
		FText::FromString("Toggle OpenXR Simulator (no headset)."),
		FSlateIcon(),
		EUserInterfaceActionType::ToggleButton
	));
}
