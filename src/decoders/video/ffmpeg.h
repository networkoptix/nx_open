#ifndef cl_ffmpeg_h2026
#define cl_ffmpeg_h2026

#include "abstractdecoder.h"
#include "dxva/dxva.h"

struct AVCodec;
struct AVCodecContext;
struct AVFrame;
struct MpegEncContext;

// client of this class is responsible for encoded data buffer meet ffmpeg restrictions
// ( see comment to decode functions for details ).

class CLFFmpegVideoDecoder : public CLAbstractVideoDecoder
{
public:
	CLFFmpegVideoDecoder(CLCodecType codec, AVCodecContext* codecContext = 0);
	bool decode(CLVideoData& data);
	~CLFFmpegVideoDecoder();

	void showMotion(bool show);

	virtual void setLightCpuMode(bool val);

    static bool isHardwareAccellerationPossible(CLCodecType codecId, int width, int height)
    {
        return codecId == CL_H264 && width <= 1920 && height <= 1088;
    }
private:
    static AVCodec* findCodec(CLCodecType codecId);

    void openDecoder();
    void closeDecoder();

    void resetDecoder();

private:
    AVCodecContext *m_passedContext;

    static int hwcounter;
	AVCodec *m_codec;
	AVCodecContext *c;
	AVFrame *picture;
    DecoderContext m_decoderContext;

	int m_width;
	int m_height;

	static bool m_first_instance;
	CLCodecType m_codecId;
	bool m_showmotion;
	bool m_lightCPUMode;
	bool m_wantEscapeFromLightCPUMode;

	unsigned int m_lightModeFrameCounter;

    bool needResetCodec;
	//===================

    int m_lastWidth;
};

#endif //cl_ffmpeg_h

