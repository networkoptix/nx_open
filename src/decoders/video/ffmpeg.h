#ifndef cl_ffmpeg_h
#define cl_ffmpeg_h

#include <windows.h>
#include "abstractdecoder.h"

#include <QMutex>

class AVCodec;
class AVCodecContext;
class AVFrame;
class MpegEncContext;

// client of this class is responsible for encoded data buffer meet ffmpeg restrictions
// ( see comment to decode functions for details ).

class CLFFmpegVideoDecoder : public CLAbstractVideoDecoder
{
public:
	CLFFmpegVideoDecoder(CLCodecType codec);
	bool decode(CLVideoData& data);
	~CLFFmpegVideoDecoder();

	void showMotion(bool show);

	virtual void setLightCpuMode(bool val);

	class CodecDll
	{
	public:
		CodecDll();
		bool  init();
		~CodecDll();
		//============dll =======
		typedef void (*dll_avcodec_init)(void);
		typedef AVCodec* (*dll_avcodec_find_decoder)(enum CodecID id);

		typedef void (*dll_avcodec_register_all)(void);
		typedef AVCodecContext* (*dll_avcodec_alloc_context)(void) ;
		typedef AVFrame*(*dll_avcodec_alloc_frame)(void);
		typedef int (*dll_avcodec_open)(AVCodecContext *avctx, AVCodec *codec);
		typedef int (*dll_avcodec_close)(AVCodecContext *avctx);
		typedef void (*dll_av_free)(void *ptr);
		typedef int (*dll_avcodec_decode_video)(AVCodecContext *avctx, AVFrame *picture, int *got_picture_ptr, unsigned char *buf, int buf_size) ;
		typedef void (*dll_ff_print_debug_info)(MpegEncContext *s, AVFrame *pict);



		dll_avcodec_init avcodec_init;
		dll_avcodec_find_decoder avcodec_find_decoder;
		dll_avcodec_register_all avcodec_register_all;
		dll_avcodec_alloc_context avcodec_alloc_context ;
		dll_avcodec_alloc_frame avcodec_alloc_frame;
		dll_avcodec_open avcodec_open;
		dll_avcodec_close avcodec_close;
		dll_av_free av_free;
		dll_avcodec_decode_video avcodec_decode_video;
		dll_ff_print_debug_info ff_print_debug_info;
	private:
		HINSTANCE m_dll, m_dll2;
	};

	static CodecDll dll;


	// functions: avcodec_find_decoder, avcodec_find_decoder, avcodec_alloc_context, avcodec_open, avcodec_close are not thread safe
	// so if client use this class the way that it creates and destroy codecs in different we need to make it safe
	static QMutex m_static_mutex;

private:
	void resart_decoder();

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

	unsigned int m_lightModeFrameCounter;

	//===================
};


#endif //cl_ffmpeg_h


