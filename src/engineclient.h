
#pragma once

class INetChannelInfo;
class INetChannel;

#include "vmt.h"
#include "util.h"

#include "angle.h"

class IEngineClient {
public:
	VPROXY(SetViewAngles, 20, void, (Angle viewAngles), viewAngles);
	VPROXY(IsInGame, 26, bool, (void));
	VPROXY(FlashWindow, 150, bool, (void));
	VPROXY(GetNetChannelInfo, 72, INetChannelInfo*, (void));
	VPROXY(IsPlayingTimeDemo, 78, bool, (void));

	VPROXY(ServerCmd, 6, void, (char const*, bool));
	VPROXY(ClientCmd, 7, void, (char const*));
	VPROXY(ExecuteClientCmd, 102, void, (char const*));
	VPROXY(ClientCmd_Unrestricted, 106, void, (char const*));
	VPROXY(SetRestrictServerCommands, 107, void, (bool));
	VPROXY(SetRestrictClientCommands, 108, void, (bool));
	VPROXY(GMOD_RawClientCmd_Unrestricted, 140, void, (char const*));
	VPROXY(ChangeTeam, 111, void, (char const*));
	VPROXY(GMOD_SetTimeManipulator, 133, void, (float));

	VPROXY(GetLastTimeStamp, 15, float, (void));
	VPROXY(EngineTime, 14, float, (void));

	VPROXY(IsBoxVisible, 31, bool, (Vector const&, Vector const&));
	VPROXY(IsBoxInViewCluster, 32, bool, (Vector const&, Vector const&));
	VPROXY(IsOccluded, 69, bool, (Vector const&, Vector const&));
	
	VPROXY(GetGameDirectory, 35, char const*, (void));


	
	

	// /*19*/	virtual void GetViewAngles(QAngle&) = 0;
	// /*20*/	virtual void SetViewAngles(QAngle&) = 0;

	int GetLocalPlayer() { return vmt::call<int>(this, 12); }
	INetChannel* GetNetChannel() {
		return reinterpret_cast<INetChannel*>(GetNetChannelInfo());
	}
};
 