// OpenXRSim: Slate panel interface for simulator controls in editor.
// Easy guide: read this file first when you need this behavior.

#pragma once

#include "Widgets/SCompoundWidget.h"

class SOpenXRSimPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOpenXRSimPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply OnLoadBuiltInRoom();
	FReply OnLoadStartupRoom();
	FReply OnLoadRoomJson();
	FReply OnResetOrigin();

	FReply OnStartRecord();
	FReply OnStopRecord();
	FReply OnStartReplay();
	FReply OnStartReplayFromSettings();
	FReply OnStopReplay();

	FText GetStatusText() const;

	FString ResolveDefaultReplayPath(const FString& FileName) const;
	FString ResolvePluginContentPath(const FString& RelativePath) const;
};
