// OpenXRSim: UDP forwarding receiver interface for external pose/action feeds.
// Easy guide: read this file first when you need this behavior.

#pragma once

#include "CoreMinimal.h"

class FSocket;
class FOpenXRSimState;
class UOpenXRSimSettings;

class FOpenXRSimForwardReceiver
{
public:
	~FOpenXRSimForwardReceiver();

	void Tick(const TSharedPtr<FOpenXRSimState, ESPMode::ThreadSafe>& SimState, const UOpenXRSimSettings* Settings);

private:
	void EnsureSocket(const UOpenXRSimSettings* Settings);
	void ShutdownSocket();

	FSocket* Socket = nullptr;
	bool bSocketInitialized = false;
	FString SocketEndpoint;
};

