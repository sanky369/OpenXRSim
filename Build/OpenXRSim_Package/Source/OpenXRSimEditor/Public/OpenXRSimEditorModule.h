// OpenXRSim: Editor module interface for toolbar/menu/panel hooks.
// Easy guide: read this file first when you need this behavior.

#pragma once

#include "Modules/ModuleManager.h"

class FOpenXRSimEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	void ToggleSimulator();
	bool IsSimulatorEnabled() const;

	TSharedRef<class SDockTab> SpawnSimTab(const class FSpawnTabArgs& Args);
};
