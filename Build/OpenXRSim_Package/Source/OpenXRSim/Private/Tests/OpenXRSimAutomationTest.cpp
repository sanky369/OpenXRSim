// OpenXRSim: Automation smoke test for replay loading and simulator lifecycle.
// Easy guide: read this file first when you need this behavior.

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Interfaces/IPluginManager.h"
#include "OpenXRSimModule.h"
#include "OpenXRSimState.h"
#include "OpenXRSimSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenXRSimReplayLoadsTest,
	"OpenXRSim.Replay.LoadsAndRuns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenXRSimReplayLoadsTest::RunTest(const FString& Parameters)
{
	// Enable sim (config)
	UOpenXRSimSettings* Settings = GetMutableDefault<UOpenXRSimSettings>();
	Settings->bEnableSimulator = true;
	Settings->SaveConfig();

	// Ensure module is loaded
	FOpenXRSimModule& Mod = FOpenXRSimModule::Get();
	TestTrue(TEXT("SimState valid"), Mod.GetSimState().IsValid());

	// Try starting replay from plugin content example
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("OpenXRSim"));
	TestTrue(TEXT("Plugin found"), Plugin.IsValid());
	if (!Plugin.IsValid()) return false;

	const FString ReplayPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Content/Replays/Example_10s_Walk.json"));

	const bool bStarted = Mod.GetSimState()->StartReplay(ReplayPath);
	TestTrue(TEXT("Replay started"), bStarted);

	// Tick sim a little (no world tick here, but state tick will be called by HMD in real run).
	// For MVP test, we just ensure it entered replay mode.
	TestTrue(TEXT("IsReplaying"), Mod.GetSimState()->IsReplaying());

	Mod.GetSimState()->StopReplay();
	TestFalse(TEXT("Stopped replay"), Mod.GetSimState()->IsReplaying());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
