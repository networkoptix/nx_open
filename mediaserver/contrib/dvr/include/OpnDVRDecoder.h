#include "windows.h"

#ifndef _DVRDECODER_H_
#define _DVRDECODER_H_

/*
------------------------------------------------------------------------------------

   CALLBACK Windows' wParam and lParam values.

------------------------------------------------------------------------------------
*/

const DWORD FOURCC_YUY2  = DWORD(byte('Y') | (byte('U') << 8) | (byte('U') << 16) | (byte('2') << 24));
const DWORD FOURCC_RGB24 = DWORD(byte('R') | (byte('G') << 8) | (byte('B') << 16) | (byte('2') << 24));
const DWORD FOURCC_NONE	 = 0;


typedef HANDLE DECHANDLE;
typedef DECHANDLE* LPDECHANDLE;

typedef enum {
    DECRESULT_OK                    = 0,    
    DECRESULT_INVALID_HANDLE        = 1,    
    DECRESULT_INVALID_PARAM         = 2,    
    DECRESULT_DCODING_PROGRESS      = 3,    
    DECRESULT_INVALID_CODECID       = 4,    
    DECRESULT_INVALID_CODECDATA     = 5,    
    DECRESULT_INVALID_FRAMEDATA     = 6,    
    DECRESULT_DECODEING_FAIL        = 7,    
    DECRESULT_KEYFRAME_NEEDED       = 8,    
    DECRESULT_MEMORY_ERROR          = 9,    
    DECRESULT_INITIALIZE_FAIL       = 10,   
    DECRESULT_GNERNAL_ERROR         = 11,   
    DECRESULT_SEQUENCE_ERROR        = 12,   
    DECRESULT_ENCODEING_FAIL        = 13,   
    DECRESULT_DECODING_GREEN        = 14,   
    DECRESULT_ALREADY_OPEN          = 15,   
    DECRESULT_DLL_NOT_FOUND         = 16,   
    DECRESULT_INVALID_DLL           = 17,   
    DECRESULT_UNKNOWN               = 18,
    DECRESULT_NOMOREFRAME           = 19,   
    DECRESULT_EXCEPTION             = 20,   
    DECRESULT_UNSUPPORTED           = 21    
}
DECResult; 

typedef enum {
	FLIPMODE_DEFAULT = 0,
	FLIPMODE_NORMAL = 1,
	FLIPMODE_VERTICAL = 2
}
DECFlipMode;

struct DVRDecoderPrepareInfo
{
	DWORD cbSize;
	int nChannelNo;
	int nWidth;
	int nHeight;
	DWORD dwDecodeSize;
	DVRDecoderPrepareInfo() : cbSize(sizeof(DVRDecoderPrepareInfo))	{}
};

DECHANDLE WINAPI DVRDecoderOpen(void);
DECResult WINAPI DVRDecoderClose(DECHANDLE hDecoder);
DECResult WINAPI DVRDecoderInitChannel(DECHANDLE hDecoder, int nChannelNo);
DECResult WINAPI DVRDecoderSetFormat(DECHANDLE hDecoder, int nChannelNo, /*FOURCC_XXX*/DWORD dwDecodeFormat, DECFlipMode nFlipMode);
DECResult WINAPI DVRDecoderPush(DECHANDLE hDecoder, int nChannelNo, void* pEncodedData, DWORD dwEncodedSize);
DECResult WINAPI DVRDecoderPrepare(DECHANDLE hDecoder, DVRDecoderPrepareInfo* pPrepareInfo);
DECResult WINAPI DVRDecoderPop(DECHANDLE hDecoder, void* pDecodeBuffer, DWORD& nOutCodecID);
DECResult WINAPI DVRConvertYUV2RGB(void* YUVbuf, void* RGBbuf, int nW, int nH, bool bVerticalFlip);
#endif
