#ifndef cl_ffmpeg_codec_dll_h1619
#define cl_ffmpeg_codec_dll_h1619

#include "libavcodec\avcodec.h"
#include "decoders\video\ffmpeg.h"
#include <windows.h>

struct MpegEncContext;

class FFMPEGCodecDll
{
public:
	FFMPEGCodecDll();
	bool  init();
	~FFMPEGCodecDll();
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
	typedef void  (*dll_av_log_set_callback)(void(*callback)(void *, int, const char *, va_list)) ;
	typedef int (*dll_avcodec_decode_audio2)(AVCodecContext *avctx, int16_t *samples, int *frame_size_ptr, const uint8_t *buf, int buf_size);



	dll_avcodec_init avcodec_init;
	dll_avcodec_find_decoder avcodec_find_decoder;
	dll_avcodec_register_all avcodec_register_all;
	dll_avcodec_alloc_context avcodec_alloc_context ;
	dll_avcodec_alloc_frame avcodec_alloc_frame;
	dll_avcodec_open avcodec_open;
	dll_avcodec_close avcodec_close;
	dll_av_free av_free;
	dll_avcodec_decode_video avcodec_decode_video;
	dll_avcodec_decode_audio2 avcodec_decode_audio2;

	dll_ff_print_debug_info ff_print_debug_info;
	dll_av_log_set_callback av_log_set_callback;
	
	
private:
	HINSTANCE m_dll, m_dll2;
};

#endif //cl_ffmpeg_codec_dll_h1619