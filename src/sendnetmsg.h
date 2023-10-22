#pragma once

#include "inetchannel.h"

namespace sendnetmsg {
	bool __fastcall SendNetMsgHookFunc(INetChannel* self, INetMessage& msg, bool bForceReliable = false, bool bVoice = false);

	void hook();
	void unHook();
}
