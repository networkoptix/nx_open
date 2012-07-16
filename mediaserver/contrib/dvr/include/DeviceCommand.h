#include "windows.h"

#ifndef _DEVICECOMMAND_H_
#define _DEVICECOMMAND_H_

#define CMD_DEVICESETUP_GET 100
#define CMD_DEVICESETUP_SET 101

#define VSCMD_TVOUT   1000

typedef struct tagVSCfgTVOUT
{
	BOOL	bFixedMode;			// 0, 1, 2, 3, 4(Quad)
	INT		nFixedCh;
	INT		nInterval;
	WORD	wRotateMask;		// 1, 2, 4, 8, 16(Quad)
	WORD	wMotionMask;		// 1, 2, 4, 8	
}
VSCfgTVOut, *LPVSCfgTVOut;

#endif
