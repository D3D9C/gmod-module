
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

	// /*19*/	virtual void GetViewAngles(QAngle&) = 0;
	// /*20*/	virtual void SetViewAngles(QAngle&) = 0;

	int GetLocalPlayer() { return vmt::call<int>(this, 12); }
	INetChannel* GetNetChannel() {
		return reinterpret_cast<INetChannel*>(GetNetChannelInfo());
	}
};
 