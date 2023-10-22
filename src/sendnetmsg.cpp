#include "sendnetmsg.h"
#include "engineclient.h"
#include "interfaces.h"
#include "clientstate.h"
#include "vmt.h"
#include "chlclient.h"
#include "globals.h"

#define MAX_CMD_BUFFER 4000

#define NUM_NEW_COMMAND_BITS		4
#define MAX_NEW_COMMANDS			((1 << NUM_NEW_COMMAND_BITS) - 1)

#define NUM_BACKUP_COMMAND_BITS		3
#define MAX_BACKUP_COMMANDS			((1 << NUM_BACKUP_COMMAND_BITS) - 1)

namespace sendnetmsg {
	using SendNetMsgFn = bool(__fastcall*)(INetChannel* self, INetMessage& msg, bool bForceReliable, bool bVoice);
	SendNetMsgFn SendNetMsgOriginal = nullptr;

	void SendMove() {
		uint8_t data[MAX_CMD_BUFFER];

		int nextCommandNr = interfaces::clientState->lastoutgoingcommand + interfaces::clientState->chokedcommands + 1;

		CLC_Move moveMsg;
		// moveMsg.SetupVMT();

		moveMsg.m_DataOut.StartWriting(data, sizeof(data));

		// How many real new commands have queued up
		moveMsg.m_nNewCommands = 1 + interfaces::clientState->chokedcommands;
		moveMsg.m_nNewCommands = std::clamp(moveMsg.m_nNewCommands, 0, MAX_NEW_COMMANDS);

		int extraCommands = interfaces::clientState->chokedcommands + 1 - moveMsg.m_nNewCommands;

		int backupCommands = max(2, extraCommands);
		moveMsg.m_nBackupCommands = std::clamp(backupCommands, 0, MAX_BACKUP_COMMANDS);

		int numCommands = moveMsg.m_nNewCommands + moveMsg.m_nBackupCommands;

		int from = -1;	// first command is deltaed against zeros 
		 
		bool bOK = true;

		for (int to = nextCommandNr - numCommands + 1; to <= nextCommandNr; to++) {
			bool isnewcmd = to >= (nextCommandNr - moveMsg.m_nNewCommands + 1);

			// first valid command number is 1
			bOK = bOK && interfaces::client->WriteUsercmdDeltaToBuffer(&moveMsg.m_DataOut, from, to, isnewcmd);
			from = to;
		}

		// only write message if all usercmds were written correctly, otherwise parsing would fail
		if (bOK) {
			INetChannel* chan = reinterpret_cast<INetChannel*>(interfaces::engineClient->GetNetChannelInfo());

			chan->m_nChokedPackets -= extraCommands;

			SendNetMsgOriginal(chan, reinterpret_cast<INetMessage&>(moveMsg), false, false);
		}
	}

	bool __fastcall SendNetMsgHookFunc(INetChannel* self, INetMessage& msg, bool bForceReliable, bool bVoice) {
		if (msg.GetGroup() == static_cast<int>(NetMessage::clc_VoiceData))
			bVoice = true;

		auto msgName = msg.GetName();

		if (globals::luaInit) {
			auto* lua = interfaces::clientLua;

			lua->PushSpecial(SPECIAL_GLOB);
			lua->GetField(-1, "hook");
			lua->GetField(-1, "Run");
			 
			lua->PushString("SendNetMsg");

			lua->PushString(msgName);

			lua->Call(2, 0);
			lua->Pop(2);
		}
		
		/*
		if (strcmp(msgName, "clc_Move") == 0) {

			if (globals::bSendPacket) {
				// sendmove here 
				interfaces::clientState->chokedcommands = 0;
				SendMove();
				return true;
			} else {
				INetChannel* chan = reinterpret_cast<INetChannel*>(interfaces::engineClient->GetNetChannelInfo());
				chan->SetChoked();
				interfaces::clientState->chokedcommands++;
				return true;
			}
			
		}
		*/

		return SendNetMsgOriginal(self, msg, bForceReliable, bVoice);
	}

	void hook() {
		vmt::hook(interfaces::engineClient->GetNetChannel(), &SendNetMsgOriginal, (const void*)SendNetMsgHookFunc, 40);
	}

	void unHook() {
		SendNetMsgFn dummy = nullptr;
		vmt::hook(interfaces::engineClient->GetNetChannel(), &dummy, SendNetMsgOriginal, 40);
	}
}
