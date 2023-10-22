#pragma once

#include "util.h"

class ClientClass;

struct Rect {
	int x, y;
	int width, height;
};
 
struct VRect {
	int	x, y, width, height;
	Rect* pnext;
};

class VRect;
class CViewSetup;

class bf_write;
class CHLClient {
public:
	VPROXY(GetAllClasses, 8, ClientClass*, (void));
	VPROXY(WriteUsercmdDeltaToBuffer, 23, bool, (bf_write* buf, int from, int to, bool isnewcommand), buf, from, to, isnewcommand);

	VPROXY(CreateMove, 21, void, (int sequence_number, float input_sample_frametime, bool active), sequence_number, input_sample_frametime, active);
	VPROXY(View_Render, 26, void, (void*));
	// VPROXY(RenderView, 27, void, (CViewSetup view, int nClearFlags, int whatToDraw), view, nClearFlags, whatToDraw);




	// VPROXY(CreateMove, 21, void, (int sequence_number, float input_sample_frametime, bool active), sequence_number, input_sample_frametime, active);
	// VPROXY(WriteUsercmdDeltaToBuffer, 23, bool, (bf_write* buf, int from, int to, bool isnewcommand), buf, from, to, isnewcommand);
	// VPROXY(FrameStageNotify, 35, void, (ClientFrameStage_t stage), stage);
};