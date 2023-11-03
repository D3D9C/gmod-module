
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <fstream>

#define GMOD_USE_SOURCESDK
#include "GarrysMod/Lua/Interface.h"
#include "GarrysMod/Lua/Types.h"
using namespace GarrysMod::Lua;

#include "vstdlib.h"
#include "md5.h"
#include "util.h"
#include "convar.h"
#include "spoofedconvar.h"
#include "cusercmd.h"
#include "prediction.h"
#include "simulation.h"
#include "engineclient.h"
#include "inetchannel.h"
#include "clientstate.h"
#include "globalvars.h"
#include "createmovehook.h"
#include "doimpacthook.h"
#include "framestagenotify.h"
#include "clientmodecreatemovehook.h"
#include "runcommandhook.h"
#include "viewrenderhook.h"
#include "isplayingtimedemo.h"
#include "sendnetmsg.h"
#include "luashared.h"
#include "detour.h"
#include "globals.h"
#include "interfaces.h"
#include "cliententitylist.h"
#include "entity.h"
 
/*
	Prediction
*/

LUA_FUNCTION(GetServerTime) {
	LUA->CheckType(1, Type::UserCmd);

	CUserCmd* cmd = LUA->GetUserType<CUserCmd>(1, Type::UserCmd);
	LUA->PushNumber(g_prediction.GetServerTime(cmd));

	return 1; 
}

LUA_FUNCTION(StartPrediction) {
	LUA->CheckType(1, Type::UserCmd);

	g_prediction.Start(LUA->GetUserType<CUserCmd>(1, Type::UserCmd));

	return 0;
}

LUA_FUNCTION(FinishPrediction) {
	g_prediction.Finish();

	return 0;
}
 
/*
	CUserCmd
*/

LUA_FUNCTION(SetCommandNumber) {
	LUA->CheckType(1, Type::UserCmd);
	LUA->CheckNumber(2);

	CUserCmd* cmd = LUA->GetUserType<CUserCmd>(1, Type::UserCmd);
	cmd->command_number = LUA->GetNumber(2);

	return 0;
}

LUA_FUNCTION(SetRandomSeed) {
	LUA->CheckType(1, Type::UserCmd);
	LUA->CheckNumber(2);

	CUserCmd* cmd = LUA->GetUserType<CUserCmd>(1, Type::UserCmd);
	cmd->random_seed = LUA->GetNumber(2);

	return 0;
}

LUA_FUNCTION(GetCmdRandomSeed) {
	LUA->CheckType(1, Type::UserCmd);

	CUserCmd* cmd = LUA->GetUserType<CUserCmd>(1, Type::UserCmd);
	LUA->PushNumber( cmd->random_seed );

	return 1;
}

LUA_FUNCTION(GetRandomSeed) {
	LUA->CheckType(1, Type::UserCmd);

	CUserCmd* cmd = LUA->GetUserType<CUserCmd>(1, Type::UserCmd);

	uint8_t seed;
	{
		Chocobo1::MD5 md5;
		md5.addData(&cmd->command_number, sizeof(cmd->command_number));
		md5.finalize();

		seed = *reinterpret_cast<uint32_t*>(md5.toArray().data() + 6) & 0xFF;
	}

	LUA->PushNumber(seed);

	return 1;
}

LUA_FUNCTION(SetCommandTick) {
	LUA->CheckType(1, Type::UserCmd);
	LUA->CheckNumber(2);

	CUserCmd* cmd = LUA->GetUserType<CUserCmd>(1, Type::UserCmd);
	cmd->tick_count = LUA->GetNumber(2);
	 
	return 0;
}

LUA_FUNCTION(SetTyping) {
	LUA->CheckType(1, Type::UserCmd);
	LUA->CheckType(2, Type::Bool);

	CUserCmd* cmd = LUA->GetUserType<CUserCmd>(1, Type::UserCmd);
	cmd->istyping = LUA->GetBool(2);
	cmd->unk = LUA->GetBool(2);

	return 0;
}

LUA_FUNCTION(PredictSpread) {
	LUA->CheckType(1, Type::UserCmd);
	LUA->CheckType(2, Type::Angle);
	LUA->CheckType(3, Type::Vector);

	CUserCmd* cmd = LUA->GetUserType<CUserCmd>(1, Type::UserCmd);
	Angle angle = LUA->GetAngle(2);
	Vector vector = LUA->GetVector(3);

	uint8_t seed;
	{
		Chocobo1::MD5 md5;
		md5.addData(&cmd->command_number, sizeof(cmd->command_number));
		md5.finalize();

		seed = *reinterpret_cast<uint32_t*>(md5.toArray().data() + 6) & 0xFF;
	}

	vstdlib::RandomSeed(seed);

	float rand_x = vstdlib::RandomFloat(-0.5, 0.5) + vstdlib::RandomFloat(-0.5, 0.5);
	float rand_y = vstdlib::RandomFloat(-0.5, 0.5) + vstdlib::RandomFloat(-0.5, 0.5);

	Vector spreadDir = Vector(1.f, -vector.x * rand_x, vector.y * rand_y);

	LUA->PushVector(spreadDir);

	return 1;
}

LUA_FUNCTION(SetContextVector) {
	LUA->CheckType(1, Type::UserCmd);
	LUA->CheckType(2, Type::Vector);
	LUA->CheckType(3, Type::Bool);

	CUserCmd* cmd		= LUA->GetUserType<CUserCmd>(1, Type::UserCmd);
	cmd->context_menu	= LUA->GetBool(3);
	cmd->context_normal = LUA->GetVector(2);

	return 0;
}

/*
	Simulation
*/

LUA_FUNCTION(StartSimulation) {
	LUA->CheckNumber(1);

	CBasePlayer* ply = reinterpret_cast<CBasePlayer*>(interfaces::entityList->GetClientEntity(LUA->GetNumber(1)));

	g_simulation.Start(ply);

	return 0;
}

LUA_FUNCTION(SimulateTick) {
	g_simulation.SimulateTick();

	return 0;
}
 
LUA_FUNCTION(GetSimulationData) {
	LUA->CreateTable();

	const auto& moveData = g_simulation.GetMoveData();

	LUA->PushAngle(moveData.m_vecViewAngles);
	LUA->SetField(-2, "m_vecViewAngles");

	LUA->PushVector(moveData.m_vecVelocity);
	LUA->SetField(-2, "m_vecVelocity");

	LUA->PushAngle(moveData.m_vecAngles);
	LUA->SetField(-2, "m_vecAngles");

	LUA->PushVector(moveData.m_vecAbsOrigin);
	LUA->SetField(-2, "m_vecAbsOrigin");

	return 1;
}

LUA_FUNCTION(FinishSimulation) {
	g_simulation.Finish();

	return 0;
}

LUA_FUNCTION(EditSimulationData) {
	LUA->CheckType(1, Type::Table);

	auto fieldExists = [&](const char* name) -> bool {
		LUA->GetField(1, name);
		bool isNil = LUA->IsType(-1, Type::Nil);
		LUA->Pop();
		return !isNil;
	};

	auto& moveData = g_simulation.GetMoveData();
	if (fieldExists("m_nButtons")) {
		LUA->GetField(1, "m_nButtons");
		moveData.m_nButtons = LUA->GetNumber();
	}

	if (fieldExists("m_nOldButtons")) {
		LUA->GetField(1, "m_nOldButtons");
		moveData.m_nOldButtons = LUA->GetNumber();
	}

	if (fieldExists("m_flForwardMove")) {
		LUA->GetField(1, "m_flForwardMove");
		moveData.m_flForwardMove = LUA->GetNumber();
	}

	if (fieldExists("m_flSideMove")) {
		LUA->GetField(1, "m_flSideMove");
		moveData.m_flSideMove = LUA->GetNumber();
	}

	if (fieldExists("m_vecViewAngles")) {
		LUA->GetField(1, "m_vecViewAngles");
		moveData.m_vecViewAngles = *LUA->GetUserType<Angle>(-1, Type::Angle);
	}

	return 0;
}


/*
	Cvar forcing
*/

LUA_FUNCTION(ConVarSetNumber) {
	LUA->CheckString(1);
	LUA->CheckNumber(2);

	auto conVarName = LUA->GetString(1);

	auto* var = interfaces::cvar->FindVar(conVarName);
	if (!var) return 1;

	var->SetValue( LUA->GetNumber( 2 ) );

	return 0;
}

LUA_FUNCTION(ConVarSetFlags) {
	LUA->CheckString(1);
	LUA->CheckNumber(2);

	auto conVarName = LUA->GetString(1);

	auto* var = interfaces::cvar->FindVar(conVarName);
	if (!var) return 1;

	var->SetFlags(LUA->GetNumber(2));

	return 0;
}

LUA_FUNCTION(ConVarRemoveFlags) {
	LUA->CheckString(1);
	LUA->CheckNumber(2);

	auto conVarName = LUA->GetString(1);

	auto* var = interfaces::cvar->FindVar(conVarName);
	if (!var) return 1;

	var->RemoveFlags(LUA->GetNumber(2));

	return 0;
}

/*
	Globals
*/

LUA_FUNCTION(SetBSendPacket) {
	LUA->CheckType(1, Type::Bool);
	globals::bSendPacket = LUA->GetBool(1);
	return 0;
}

LUA_FUNCTION(ForceBSendPacket) {
	LUA->CheckType(1, Type::Bool);
	//*sendPackets = LUA->GetBool(1);
	return 0;
}

LUA_FUNCTION(SetInterpolation) { 
	LUA->CheckType(1, Type::Bool); 
	globals::shouldInterpolate = LUA->GetBool(1); 
	return 0;
}

LUA_FUNCTION(SetSequenceInterpolation) {
	LUA->CheckType(1, Type::Bool);
	globals::shouldInterpolateSequences = LUA->GetBool(1);
	return 0;
}

LUA_FUNCTION(EnableBoneFix) {
	LUA->CheckType(1, Type::Bool);
	globals::shouldFixBones = LUA->GetBool(1);
	return 0;
}

LUA_FUNCTION(EnableAnimFix) {
	LUA->CheckType(1, Type::Bool);
	globals::shouldFixAnimations = LUA->GetBool(1);
	return 0;
}

LUA_FUNCTION(AllowAnimationUpdate) {
	LUA->CheckType(1, Type::Bool);
	globals::updateAllowed = LUA->GetBool(1);
	return 0; 
}

LUA_FUNCTION(SetMissedTicks) {
	LUA->CheckNumber(1);
	globals::updateTicks = LUA->GetNumber(1);
	return 0;
}

LUA_FUNCTION(EnableTickbaseShifting) {
	LUA->CheckType(1, Type::Bool);
	globals::canShiftTickbase = LUA->GetBool(1);
	return 0;
}

LUA_FUNCTION(SetMaxShift) {
	LUA->CheckNumber(1);
	globals::maxTickbaseShift = LUA->GetNumber(1);
	return 0;
}

LUA_FUNCTION(SetMinShift) {
	LUA->CheckNumber(1);
	globals::minTickbaseShift = LUA->GetNumber(1);
	return 0;
}

LUA_FUNCTION(GetCurrentCharge) {
	LUA->PushNumber(globals::curTickbaseCharge);

	return 1;
}

LUA_FUNCTION(GetIsShifting) {
	LUA->PushBool(globals::bIsShifting);

	return 1;
}

LUA_FUNCTION(GetIsCharging) {
	LUA->PushBool(globals::bIsRecharging);

	return 1;
}

LUA_FUNCTION(StartShifting) {
	LUA->CheckType(1, Type::Bool);
	globals::bShouldShift = LUA->GetBool(1);
	return 0;
}

LUA_FUNCTION(StartRecharging) {
	LUA->CheckType(1, Type::Bool);
	globals::bShouldRecharge = LUA->GetBool(1); 
	return 0;
}


/*
	Spoofed cvars
*/

std::vector<std::unique_ptr<SpoofedConVar>> spoofedConVars;
LUA_FUNCTION(SpoofConVar) {
	LUA->CheckString(1);

	auto conVarName = LUA->GetString(1);

	// Check if already spoofed
	for (auto& spoofedConVar : spoofedConVars) {
		if (strcmp(spoofedConVar->m_szOriginalName, conVarName) == 0)
			return true;
	}

	auto* var = interfaces::cvar->FindVar(conVarName);
	if (!var) {
		LUA->PushBool(false);
		return 1;
	}

	auto& spoofedConVar = spoofedConVars.emplace_back(std::make_unique<SpoofedConVar>(var));
	spoofedConVar->m_pOriginalCVar->DisableCallback();

	LUA->PushBool(true);
	return 1;
}

LUA_FUNCTION(SpoofedConVarSetNumber) {
	LUA->CheckString(1);
	LUA->CheckNumber(2);

	auto conVarName = LUA->GetString(1);
	const auto& it = std::find_if(spoofedConVars.begin(), spoofedConVars.end(), [=](const auto& spoofedConVar) {
		return strcmp(spoofedConVar->m_szOriginalName, conVarName) == 0;
	});

	if (it == spoofedConVars.end()) {
		LUA->PushBool(false);
		return 1;
	}

	auto& spoofedConVar = *it;

	spoofedConVar->m_pOriginalCVar->SetValue(LUA->CheckNumber(2));

	LUA->PushBool(true);
	return 1;
}

/*
	File 
*/

LUA_FUNCTION(Read) {
	LUA->CheckString(1);
	
	const char* path = LUA->GetString();
	std::ifstream file;
	file.open(path, std::ios_base::binary | std::ios_base::ate);
	if (!file.good()) {
		LUA->PushBool(false);
		return 1;
	}

	auto fileSize = file.tellg();

	// Prevent heap corruption
	if (fileSize == 0) {
		LUA->PushBool(true);
		LUA->PushString("");
		return 2;
	}

	char* buffer = new char[fileSize] {0};
	file.seekg(std::ios::beg);
	file.read(buffer, fileSize);
	file.close();

	LUA->PushBool(true);
	LUA->PushString(buffer, fileSize);

	delete[] buffer;

	return 2;
}

LUA_FUNCTION(Write) {
	LUA->CheckString(1);
	LUA->CheckString(2);

	const char* path = LUA->GetString(1);
	std::ofstream file;
	file.open(path, std::ios_base::binary);
	if (!file.good()) {
		LUA->PushBool(false);
		return 1;
	}

	unsigned int dataSize = 0;
	const char* data = LUA->GetString(2, &dataSize);
	file.write(data, dataSize);
	file.close();

	LUA->PushBool(true);

	return 1;
}

/*
	NetChannel
*/

LUA_FUNCTION(NetSetConVar) {
	LUA->CheckString(1);
	LUA->CheckString(2);

	const char* conVar = LUA->CheckString(1);
	const char* value = LUA->CheckString(2);

	INetChannel* netChan = interfaces::engineClient->GetNetChannel();

	uint8_t msgBuf[1024];
	NetMessageWriteable netMsg(NetMessage::net_SetConVar, msgBuf, sizeof(msgBuf));
	netMsg.write.WriteUInt(static_cast<uint32_t>(NetMessage::net_SetConVar), NET_MESSAGE_BITS);
	netMsg.write.WriteByte(1);
	netMsg.write.WriteString(conVar);
	netMsg.write.WriteString(value);

	netChan->SendNetMsg(netMsg, true);

	return 1;
}

LUA_FUNCTION(NetDisconnect) {
	LUA->CheckString(1);

	const char* str = LUA->CheckString(1);

	INetChannel* netChan = interfaces::engineClient->GetNetChannel();

	uint8_t msgBuf[1024];
	 
	NetMessageWriteable netMsg(NetMessage::net_Disconnect, msgBuf, sizeof(msgBuf));
	netMsg.write.WriteUInt(static_cast<uint32_t>(NetMessage::net_Disconnect), NET_MESSAGE_BITS);
	netMsg.write.WriteByte(1); 
	netMsg.write.WriteString(str);

	netChan->SendNetMsg(netMsg, true);

	return 1;
}

LUA_FUNCTION(RequestFile) {
	LUA->CheckString(1);

	const char* str = LUA->CheckString(1);
	INetChannel* netChan = interfaces::engineClient->GetNetChannel();

	netChan->RequestFile(str);

	return 1;
} 

LUA_FUNCTION(SendFile) {
	LUA->CheckString(1);
	LUA->CheckNumber(2);

	const char* str = LUA->CheckString(1);
	INetChannel* netChan = interfaces::engineClient->GetNetChannel();

	netChan->SendFile(str, LUA->GetNumber(2));

	return 1;
}

LUA_FUNCTION(GetLatency) {
	LUA->CheckNumber(1);

	INetChannel* netChan = interfaces::engineClient->GetNetChannel();

	LUA->PushNumber(netChan->GetLatency(LUA->GetNumber(1)));

	return 1;
}

LUA_FUNCTION(GetAvgLatency) {
	LUA->CheckNumber(1);

	INetChannel* netChan = interfaces::engineClient->GetNetChannel();

	LUA->PushNumber(netChan->GetAvgLatency(LUA->GetNumber(1)));

	return 1;
}

LUA_FUNCTION(GetNetName) {
	LUA->PushString(interfaces::engineClient->GetNetChannel()->GetName());

	return 1;
}

LUA_FUNCTION(GetNetAdress) {
	LUA->PushString(interfaces::engineClient->GetNetChannel()->GetAddress());

	return 1;
}

LUA_FUNCTION(GetNetTime) {
	LUA->PushNumber(interfaces::engineClient->GetNetChannel()->GetTime());

	return 1;
}

LUA_FUNCTION(GetNetTimeConnected) {
	LUA->PushNumber(interfaces::engineClient->GetNetChannel()->GetTimeConnected());

	return 1;
}

LUA_FUNCTION(GetNetBufferSize) {
	LUA->PushNumber(interfaces::engineClient->GetNetChannel()->GetBufferSize());

	return 1;
}

LUA_FUNCTION(GetNetDataRate) {
	LUA->PushNumber(interfaces::engineClient->GetNetChannel()->GetDataRate());

	return 1;
}

LUA_FUNCTION(GetIsLoopback) {
	LUA->PushBool(interfaces::engineClient->GetNetChannel()->IsLoopback());

	return 1;
}

LUA_FUNCTION(GetIsTimingOut) {
	LUA->PushBool(interfaces::engineClient->GetNetChannel()->IsTimingOut());

	return 1;
}

LUA_FUNCTION(GetAvgLoss) {
	LUA->CheckNumber(1);

	LUA->PushNumber(interfaces::engineClient->GetNetChannel()->GetAvgLoss(LUA->GetNumber(1)));

	return 1;
}

LUA_FUNCTION(GetAvgChoke) {
	LUA->CheckNumber(1);

	LUA->PushNumber(interfaces::engineClient->GetNetChannel()->GetAvgChoke(LUA->GetNumber(1)));

	return 1;
}

LUA_FUNCTION(GetAvgData) {
	LUA->CheckNumber(1);

	LUA->PushNumber(interfaces::engineClient->GetNetChannel()->GetAvgData(LUA->GetNumber(1)));

	return 1;
}

LUA_FUNCTION(GetAvgPackets) {
	LUA->CheckNumber(1);

	LUA->PushNumber(interfaces::engineClient->GetNetChannel()->GetAvgPackets(LUA->GetNumber(1)));

	return 1;
}

LUA_FUNCTION(GetTotalData) {
	LUA->CheckNumber(1);

	LUA->PushNumber(interfaces::engineClient->GetNetChannel()->GetTotalData(LUA->GetNumber(1)));

	return 1;
}

LUA_FUNCTION(GetSequenceNrFlow) {
	LUA->CheckNumber(1);

	LUA->PushNumber(interfaces::engineClient->GetNetChannel()->GetSequenceNr(LUA->GetNumber(1)));

	return 1;
}

LUA_FUNCTION(IsValidPacket) {
	LUA->CheckNumber(1);
	LUA->CheckNumber(2);

	LUA->PushBool(interfaces::engineClient->GetNetChannel()->IsValidPacket(LUA->GetNumber(1), LUA->GetNumber(2)));

	return 1;
}

LUA_FUNCTION(GetPacketTime) {
	LUA->CheckNumber(1);
	LUA->CheckNumber(2);

	LUA->PushNumber(interfaces::engineClient->GetNetChannel()->GetPacketTime(LUA->GetNumber(1), LUA->GetNumber(2)));

	return 1;
}

LUA_FUNCTION(GetPacketBytes) {
	LUA->CheckNumber(1);
	LUA->CheckNumber(2);
	LUA->CheckNumber(3);

	LUA->PushNumber(interfaces::engineClient->GetNetChannel()->GetPacketBytes(LUA->GetNumber(1), LUA->GetNumber(2), LUA->GetNumber(3)));

	return 1;
}

LUA_FUNCTION(SetDataRate) {
	LUA->CheckNumber(1);

	interfaces::engineClient->GetNetChannel()->SetDataRate(LUA->GetNumber(1));

	return 0;
}

LUA_FUNCTION(SetChallengeNr) {
	LUA->CheckNumber(1);

	interfaces::engineClient->GetNetChannel()->SetChallengeNr(LUA->GetNumber(1));

	return 0;
}

LUA_FUNCTION(SetCompressionMode) {
	LUA->CheckType(1, Type::Bool);

	interfaces::engineClient->GetNetChannel()->SetCompressionMode(LUA->GetBool(1));

	return 0;
}

LUA_FUNCTION(SetInterpolationAmount) {
	LUA->CheckNumber(1);

	interfaces::engineClient->GetNetChannel()->SetInterpolationAmount(LUA->GetNumber(1));

	return 0;
}

LUA_FUNCTION(SetRemoteFramerate) {
	LUA->CheckNumber(1);
	LUA->CheckNumber(2);

	interfaces::engineClient->GetNetChannel()->SetRemoteFramerate(LUA->GetNumber(1), LUA->GetNumber(2));

	return 1;
}

LUA_FUNCTION(SetMaxRoutablePayloadSize) {
	LUA->CheckNumber(1);

	interfaces::engineClient->GetNetChannel()->SetMaxRoutablePayloadSize(LUA->GetNumber(1));

	return 0;
}

LUA_FUNCTION(NetShutdownStr) {
	LUA->CheckString(1);

	interfaces::engineClient->GetNetChannel()->Shutdown(LUA->GetString(1));

	return 0;
}

LUA_FUNCTION(SetTimeout) {
	LUA->CheckNumber(1);

	interfaces::engineClient->GetNetChannel()->SetTimeout(LUA->GetNumber(1));

	return 0;
}

LUA_FUNCTION_GETSET(OutSequenceNr, Number, interfaces::engineClient->GetNetChannel()->m_nOutSequenceNr);
LUA_FUNCTION_GETSET(InSequenceNr, Number, interfaces::engineClient->GetNetChannel()->m_nInSequenceNr);
LUA_FUNCTION_GETSET(OutSequenceNrAck, Number, interfaces::engineClient->GetNetChannel()->m_nOutSequenceNrAck);
LUA_FUNCTION_GETSET(NetChokedPackets, Number, interfaces::engineClient->GetNetChannel()->m_nChokedPackets);
LUA_FUNCTION_GETSET(PacketDrop, Number, interfaces::engineClient->GetNetChannel()->m_PacketDrop);
LUA_FUNCTION_GETSET(OutReliableState, Number, interfaces::engineClient->GetNetChannel()->m_nOutReliableState);
LUA_FUNCTION_GETSET(InReliableState, Number, interfaces::engineClient->GetNetChannel()->m_nInReliableState);

/*
	Client state
*/

LUA_FUNCTION_GETSET(LastCommandAck, Number, interfaces::clientState->last_command_ack);
LUA_FUNCTION_GETSET(LastOutgoingCommand, Number, interfaces::clientState->lastoutgoingcommand);
LUA_FUNCTION_GETSET(ChokedCommands, Number, interfaces::clientState->chokedcommands);

LUA_FUNCTION_GETTER(GetPreviousTick, Number, interfaces::clientState->oldtickcount);

/*
	Engine client
*/

LUA_FUNCTION(EC_SetViewAngles) {
	LUA->CheckType(1, Type::Angle);
	interfaces::engineClient->SetViewAngles(LUA->GetAngle(1));
	return 0;
}

LUA_FUNCTION(EC_ServerCmd) {
	LUA->CheckString(1);
	LUA->CheckType(2, Type::Bool);

	interfaces::engineClient->ServerCmd(LUA->GetString(1), LUA->GetBool(2));
	return 0;
}

LUA_FUNCTION(EC_ClientCmd) {
	LUA->CheckString(1);

	interfaces::engineClient->ClientCmd(LUA->GetString(1));
	return 0;
}

LUA_FUNCTION(EC_ExClientCmd) {
	LUA->CheckString(1);

	interfaces::engineClient->ExecuteClientCmd(LUA->GetString(1));
	return 0;
}

LUA_FUNCTION(EC_GCmdUnrestricted) {
	LUA->CheckString(1);

	interfaces::engineClient->GMOD_RawClientCmd_Unrestricted(LUA->GetString(1));
	return 0;
}

LUA_FUNCTION(EC_UnrClientCmd) {
	LUA->CheckString(1);

	interfaces::engineClient->ClientCmd_Unrestricted(LUA->GetString(1));
	return 0;
}

LUA_FUNCTION(EC_SetRestrictServerCommands) {
	LUA->CheckType(1, Type::Bool);

	interfaces::engineClient->SetRestrictServerCommands(LUA->GetBool(1));
	return 0;
}

LUA_FUNCTION(EC_SetRestrictClientCommands) {
	LUA->CheckType(1, Type::Bool);

	interfaces::engineClient->SetRestrictClientCommands(LUA->GetBool(1));
	return 0;
}

LUA_FUNCTION(EC_GetGameDirectory) {
	LUA->PushString(interfaces::engineClient->GetGameDirectory());

	return 1;
}
 
LUA_FUNCTION(EC_ChangeTeam) {
	LUA->CheckString(1);

	interfaces::engineClient->ChangeTeam(LUA->GetString(1));
	return 0;
}

LUA_FUNCTION(EC_SetTimeManipulator) {
	LUA->CheckNumber(1);

	interfaces::engineClient->GMOD_SetTimeManipulator(LUA->GetNumber(1));
	return 0;
}

LUA_FUNCTION(EC_GetLocalPlayer) {
	LUA->PushNumber( interfaces::engineClient->GetLocalPlayer() );
	return 1;
}

LUA_FUNCTION(EC_GetTime) {
	LUA->PushNumber(interfaces::engineClient->EngineTime());
	return 1;
}

LUA_FUNCTION(EC_GetLastTimeStamp) {
	LUA->PushNumber(interfaces::engineClient->GetLastTimeStamp());
	return 1;
}

LUA_FUNCTION(EC_IsBoxVisible) {
	LUA->CheckType(1, Type::Vector);
	LUA->CheckType(2, Type::Vector);

	LUA->PushBool(interfaces::engineClient->IsBoxVisible(LUA->GetVector(1),LUA->GetVector(2)));
	return 1;
}

LUA_FUNCTION(EC_IsBoxInViewCluster) {
	LUA->CheckType(1, Type::Vector);
	LUA->CheckType(2, Type::Vector);

	LUA->PushBool(interfaces::engineClient->IsBoxInViewCluster(LUA->GetVector(1), LUA->GetVector(2)));
	return 1;
}

LUA_FUNCTION(EC_GetEngineIsOccluded) {
	LUA->CheckType(1, Type::Vector);
	LUA->CheckType(2, Type::Vector);

	LUA->PushBool(interfaces::engineClient->IsOccluded(LUA->GetVector(1), LUA->GetVector(2)));
	return 1;
}



/*
	Global vars
*/

LUA_FUNCTION(SetCurTime) {
	LUA->CheckNumber(1);
	interfaces::globalVars->curtime = LUA->GetNumber(1);
	return 0;
}

LUA_FUNCTION(SetFrameTime) {
	LUA->CheckNumber(1);
	interfaces::globalVars->frametime = LUA->GetNumber(1);
	return 0;
}

LUA_FUNCTION(SetRealTime) {
	LUA->CheckNumber(1);
	interfaces::globalVars->realtime = LUA->GetNumber(1);
	return 0;
}

LUA_FUNCTION(SetFrameCount) {
	LUA->CheckNumber(1);
	interfaces::globalVars->framecount = LUA->GetNumber(1);
	return 0;
}

LUA_FUNCTION(SetAbsFrameTime) {
	LUA->CheckNumber(1);
	interfaces::globalVars->absoluteframetime = LUA->GetNumber(1);
	return 0;
}

LUA_FUNCTION(SetInterpAmt) {
	LUA->CheckNumber(1);
	interfaces::globalVars->interpolation_amount = LUA->GetNumber(1);
	return 0;
}

/*
	Entity
*/

LUA_FUNCTION(GetSimulationTime) {
	LUA->CheckNumber(1);

	CBasePlayer* Ply = reinterpret_cast<CBasePlayer*>(interfaces::entityList->GetClientEntity(LUA->GetNumber(1)));
	
	LUA->PushNumber(Ply->m_flSimulationTime());

	return 1;
}

LUA_FUNCTION(GetTargetLowerBodyYaw) {
	LUA->CheckNumber(1);

	CBasePlayer* Ply = reinterpret_cast<CBasePlayer*>(interfaces::entityList->GetClientEntity(LUA->GetNumber(1)));
	CBasePlayerAnimState* animState = Ply->GetAnimState();

	LUA->PushNumber(animState->m_flGoalFeetYaw);

	return 1;
}

LUA_FUNCTION(GetCurrentLowerBodyYaw) {
	LUA->CheckNumber(1);

	CBasePlayer* Ply = reinterpret_cast<CBasePlayer*>(interfaces::entityList->GetClientEntity(LUA->GetNumber(1)));
	CBasePlayerAnimState* animState = Ply->GetAnimState();

	LUA->PushNumber(animState->m_flCurrentFeetYaw);

	return 1;
}

LUA_FUNCTION(SetTargetLowerBodyYaw) {
	LUA->CheckNumber(1);
	LUA->CheckNumber(2);

	CBasePlayer* Ply = reinterpret_cast<CBasePlayer*>(interfaces::entityList->GetClientEntity(LUA->GetNumber(1)));
	CBasePlayerAnimState* animState = Ply->GetAnimState();

	animState->m_flGoalFeetYaw = LUA->GetNumber(2);

	return 1;
}

LUA_FUNCTION(SetCurrentLowerBodyYaw) {
	LUA->CheckNumber(1);
	LUA->CheckNumber(2);

	CBasePlayer* Ply = reinterpret_cast<CBasePlayer*>(interfaces::entityList->GetClientEntity(LUA->GetNumber(1)));
	CBasePlayerAnimState* animState = Ply->GetAnimState();

	animState->m_flCurrentFeetYaw = LUA->GetNumber(2);

	return 1;
}

LUA_FUNCTION(UpdateClientAnimation) {
	LUA->CheckNumber(1);

	CBasePlayer* Ply = reinterpret_cast<CBasePlayer*>( interfaces::entityList->GetClientEntity( LUA->GetNumber(1) ) );
	Ply->UpdateClientsideAnimation();

	return 1;
}

LUA_FUNCTION(UpdateAnimations) {
	LUA->CheckNumber(1);
	LUA->CheckNumber(2);
	LUA->CheckNumber(3);

	CBasePlayer* Ply = reinterpret_cast<CBasePlayer*>(interfaces::entityList->GetClientEntity(LUA->GetNumber(1)));
	CBasePlayerAnimState* animState = Ply->GetAnimState();

	animState->Update( LUA->GetNumber(2), LUA->GetNumber(3) );

	return 1;
}

LUA_FUNCTION(SetEntityFlags) {
	LUA->CheckNumber(1);
	LUA->CheckNumber(2);

	CBasePlayer* Ply = reinterpret_cast<CBasePlayer*>( interfaces::entityList->GetClientEntity( LUA->GetNumber(1) ) );
	Ply->m_fFlags() = static_cast<int>( LUA->GetNumber(2) );

	return 0;
}

/*
	Window shit
*/

LUA_FUNCTION(ExcludeFromCapture) {
	LUA->CheckType(1, Type::Bool);

	HWND hWnd = FindWindowA("Valve001", nullptr);

	if (LUA->GetBool(1)) {
		SetWindowDisplayAffinity(hWnd, WDA_EXCLUDEFROMCAPTURE);
	}
	else
	{
		SetWindowDisplayAffinity(hWnd, !WDA_EXCLUDEFROMCAPTURE);
	}

	return 1;
}

/*
	API
*/

ILuaBase* luaBase;
auto cLuaF = [&](const char* name, CFunc func) {
	luaBase->PushCFunction(func);
	luaBase->SetField(-2, name);
};

/*
	Fucking runstring hook wont work so i decided to move it
*/

using RunStringExFn = bool(__fastcall*)(void* self, const char* filename, const char* path, const char* runstring, bool run, bool print_errors, bool dont_push_errors, bool no_returns);
RunStringExFn RunStringExOriginal = nullptr;

bool __fastcall RunStringExHookFunc(void* self, const char* filename, const char* path, const char* runstring, bool run, bool print_errors, bool dont_push_errors, bool no_returns) {
	luaBase->PushSpecial(SPECIAL_GLOB);
	luaBase->GetField(-1, "hook");
	luaBase->GetField(-1, "Run");

	luaBase->PushString("RunStringEx");

	luaBase->PushString(filename);
	luaBase->PushString(runstring);

	luaBase->Call(2, 0);
	luaBase->Pop(3);

	return RunStringExOriginal(self, filename, path, runstring, run, print_errors, dont_push_errors, no_returns);
}

GMOD_MODULE_OPEN() {
	 
	luaBase = LUA;

	//vmt::hook(luaBase, &RunStringExOriginal, (const void*)RunStringExHookFunc, 111);

	/* Interfaces / netvars */

	interfaces::init();
	netvars::init();

	/* Virtual */

	//sendnetmsg::hook();
	createmovehook::hook();
	doimpacthook::hook();
	framestagenotify::hook(); 
	cm_createmove::hook();
	runcommandhook::hook(); 
	// viewrender::hook();
	isplayingtimedemohook::hook(); 

	/* Detour */	

	detour::hook();

	/* LUA Api */

	LUA->PushSpecial(SPECIAL_GLOB);

	/*
	

	

	

	//	  
	
	 

	//  Window
	pushAPIFunction("ExcludeFromCapture",		ExcludeFromCapture);

	*/


	//	Prediction
	LUA->CreateTable();
		cLuaF("GetServerTime", GetServerTime);
		cLuaF("StartPrediction", StartPrediction);
		cLuaF("FinishPrediction", FinishPrediction);

		//	Simulation
		cLuaF("StartSimulation", StartSimulation);
		cLuaF("SimulateTick", SimulateTick);
		cLuaF("GetSimulationData", GetSimulationData);
		cLuaF("FinishSimulation", FinishSimulation);
		cLuaF("EditSimulationData", EditSimulationData);

		//	CUserCmd
		cLuaF("SetCommandNumber", SetCommandNumber);
		cLuaF("SetCommandTick", SetCommandTick);
		cLuaF("SetTyping", SetTyping);
		cLuaF("PredictSpread", PredictSpread);
		cLuaF("SetContextVector", SetContextVector);

		cLuaF("SetRandomSeed", SetRandomSeed);
		cLuaF("GetCmdRandomSeed", GetCmdRandomSeed);
		cLuaF("GetRandomSeed", GetRandomSeed);
		 
		cLuaF("SetBSendPacket", SetBSendPacket);
		cLuaF("ForceBSendPacket", ForceBSendPacket);

		// Tickbase
		cLuaF("EnableTickbaseShifting", EnableTickbaseShifting);

		cLuaF("SetMaxShift", SetMaxShift);
		cLuaF("SetMinShift", SetMinShift);
		cLuaF("GetCurrentCharge", GetCurrentCharge);

		cLuaF("GetIsShifting", GetIsShifting);
		cLuaF("GetIsCharging", GetIsCharging);

		cLuaF("StartShifting", StartShifting);
		cLuaF("StartRecharging", StartRecharging);

		// Spoof cvar
		cLuaF("SpoofConVar", SpoofConVar);
		cLuaF("SpoofedConVarSetNumber", SpoofedConVarSetNumber);

		// Cvar
		cLuaF("ConVarSetNumber", ConVarSetNumber);
		cLuaF("ConVarSetFlags", ConVarSetFlags);
		cLuaF("ConVarRemoveFlags", ConVarRemoveFlags);

		// File 
		cLuaF("Read", Read);
		cLuaF("Write", Write);

		// Entity
		cLuaF("SetEntityFlags", SetEntityFlags);

		cLuaF("UpdateAnimations", UpdateAnimations);
		cLuaF("UpdateClientAnimation", UpdateClientAnimation);

		cLuaF("SetCurrentLowerBodyYaw", SetCurrentLowerBodyYaw);
		cLuaF("SetTargetLowerBodyYaw", SetTargetLowerBodyYaw);

		cLuaF("GetCurrentLowerBodyYaw", GetCurrentLowerBodyYaw);
		cLuaF("GetTargetLowerBodyYaw", GetTargetLowerBodyYaw);

		cLuaF("GetSimulationTime", GetSimulationTime);

		cLuaF("SetInterpolation", SetInterpolation);
		cLuaF("SetSequenceInterpolation", SetSequenceInterpolation);

		cLuaF("EnableBoneFix", EnableBoneFix);

		cLuaF("EnableAnimFix", EnableAnimFix);
		cLuaF("AllowAnimationUpdate", AllowAnimationUpdate);
		cLuaF("SetMissedTicks", SetMissedTicks);

		//	Engine client 
		cLuaF("SetViewAngles", EC_SetViewAngles);

		cLuaF("ServerCmd", EC_ServerCmd);
		cLuaF("ClientCmd", EC_ClientCmd);
		cLuaF("ExecuteClientCmd", EC_ExClientCmd);
		cLuaF("RawClientCmdUnrestricted", EC_GCmdUnrestricted);
		cLuaF("ClientCmdUnrestricted", EC_UnrClientCmd);

		cLuaF("SetRestrictServerCommands", EC_SetRestrictServerCommands);
		cLuaF("SetRestrictClientCommands", EC_SetRestrictClientCommands);

		cLuaF("GetGameDirectory", EC_GetGameDirectory);

		cLuaF("IsBoxVisible", EC_IsBoxVisible);
		cLuaF("IsBoxInViewCluster", EC_IsBoxInViewCluster);
		cLuaF("IsOccluded", EC_GetEngineIsOccluded);
		 
		cLuaF("SetTimeManipulator", EC_SetTimeManipulator);
		cLuaF("GetTime", EC_GetTime);
		cLuaF("GetLastTimeStamp", EC_GetLastTimeStamp);

		cLuaF("GetLocalPlayer", EC_GetLocalPlayer);
		cLuaF("ChangeTeam", EC_ChangeTeam);
		// !!! HOOK IsTakingScreenshot !!!

		//  Global Vars 
		cLuaF("SetFrameTime", SetFrameTime);
		cLuaF("SetCurTime", SetCurTime);
		cLuaF("SetRealTime", SetRealTime);
		cLuaF("SetFrameCount", SetFrameCount);
		cLuaF("SetAbsFrameTime", SetAbsFrameTime);
		cLuaF("SetInterpolationAmount", SetInterpAmt);

		//	ClientState
		cLuaF("SetChokedCommands", SetChokedCommands);
		cLuaF("GetChokedCommands", GetChokedCommands);

		cLuaF("SetLastCommandAck", SetLastCommandAck);
		cLuaF("GetLastCommandAck", GetLastCommandAck);

		cLuaF("SetLastOutgoingCommand", SetLastOutgoingCommand);
		cLuaF("GetLastOutgoingCommand", GetLastOutgoingCommand);

		cLuaF("GetPreviousTick", GetPreviousTick);

		// NetChannel

		cLuaF("SetOutSequenceNr", SetOutSequenceNr);
		cLuaF("GetOutSequenceNr", GetOutSequenceNr);

		cLuaF("SetInSequenceNr", SetInSequenceNr);
		cLuaF("GetInSequenceNr", GetInSequenceNr);

		cLuaF("SetOutSequenceNrAck", SetOutSequenceNrAck);
		cLuaF("GetOutSequenceNrAck", GetOutSequenceNrAck);

		cLuaF("SetChokedPackets", SetNetChokedPackets);
		cLuaF("GetChokedPackets", GetNetChokedPackets);

		cLuaF("SetPacketDrop", SetPacketDrop);
		cLuaF("GetPacketDrop", GetPacketDrop);

		cLuaF("SetInReliableState", SetInReliableState);
		cLuaF("GetInReliableState", GetInReliableState);

		cLuaF("SetOutReliableState", SetOutReliableState);
		cLuaF("GetOutReliableState", GetOutReliableState);

		cLuaF("GetLatency", GetLatency);
		cLuaF("GetAvgLatency", GetAvgLatency);

		cLuaF("GetChannelName", GetNetName);
		cLuaF("GetChannelAdress", GetNetAdress);
		cLuaF("GetTime", GetNetTime);
		cLuaF("GetTimeConnected", GetNetTimeConnected);
		cLuaF("GetBufferSize", GetNetBufferSize);
		cLuaF("GetDataRate", GetNetDataRate);
		cLuaF("GetIsLoopback", GetIsLoopback);
		cLuaF("GetIsTimingOut", GetIsTimingOut);

		cLuaF("GetAvgLoss", GetAvgLoss);
		cLuaF("GetAvgChoke", GetAvgChoke);
		cLuaF("GetAvgData", GetAvgData);
		cLuaF("GetAvgPackets", GetAvgPackets);
		cLuaF("GetTotalData", GetTotalData);
		cLuaF("GetSequenceNrFlow", GetSequenceNrFlow);
		cLuaF("IsValidPacket", IsValidPacket);
		cLuaF("GetPacketTime", GetPacketTime);
		cLuaF("GetPacketBytes", GetPacketBytes);

		cLuaF("NetSetConVar", NetSetConVar);
		cLuaF("NetDisconnect", NetDisconnect);
		cLuaF("SendFile", SendFile);
		cLuaF("RequestFile", RequestFile);

		cLuaF("SetDataRate", SetDataRate);
		cLuaF("SetChallengeNr", SetChallengeNr);

		cLuaF("SetCompressionMode", SetCompressionMode);
		cLuaF("SetInterpolationAmount", SetInterpolationAmount);
		cLuaF("SetRemoteFramerate", SetRemoteFramerate);
		cLuaF("SetMaxRoutablePayloadSize", SetMaxRoutablePayloadSize);

		cLuaF("NetShutdownStr", NetShutdownStr);

		cLuaF("SetTimeout", SetTimeout);
	LUA->SetField(-2, "ded");

	return 0;
}

GMOD_MODULE_CLOSE() {
	 
	/* Virtual */

	//sendnetmsg::unHook();
	doimpacthook::unHook();
	framestagenotify::unHook();
	createmovehook::unHook();
	cm_createmove::unHook();
	runcommandhook::unHook();
	// viewrender::unHook();
	isplayingtimedemohook::unHook();
	// runstringex::unHook();

	//RunStringExFn dummy = nullptr;
	//vmt::hook(luaBase, &dummy, RunStringExOriginal, 111);

	/* Detour */

	detour::unHook();

	/* Cvars */
	 
	spoofedConVars.clear();

	return 0;
}
