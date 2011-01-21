#ifndef cl_ffmpeg_h
#define cl_ffmpeg_h


#include "abstractdecoder.h"

#include <QMutex>

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


private:

	AVCodec *codec;
	AVCodecContext *c;
	AVFrame *picture;

	int m_width;
	int m_height;


	static bool m_first_instance;
	CLCodecType m_codec;
	bool m_showmotion;
	bool m_lightCPUMode;
	bool m_wantEscapeFromLightCPUMode;

	bool m_need_to_free_context;

	unsigned int m_lightModeFrameCounter;

	//===================
};


#endif //cl_ffmpeg_h


