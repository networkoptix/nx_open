#ifndef cl_ffmpeg_h2026
#define cl_ffmpeg_h2026

#include "abstractdecoder.h"

#ifdef _USE_DXVA
#include "dxva/dxva.h"
#endif
#include "base/ffmpeg_helper.h"

struct AVCodec;
struct AVCodecContext;
struct AVFrame;
struct MpegEncContext;

// client of this class is responsible for encoded data buffer meet ffmpeg restrictions
// ( see comment to decode functions for details ).

class CLFFmpegVideoDecoder : public CLAbstractVideoDecoder
{
public:
	CLFFmpegVideoDecoder(CodecID codec, AVCodecContext* codecContext = 0);
    bool decode(const CLCompressedVideoData& data, CLVideoDecoderOutput* outFrame);
	~CLFFmpegVideoDecoder();

	void showMotion(bool show);

	virtual void setLightCpuMode(DecodeMode val);

    static bool isHardwareAccellerationPossible(CodecID codecId, int width, int height)
    {
        return codecId == CODEC_ID_H264 && width <= 1920 && height <= 1088;
    }

    PixelFormat GetPixelFormat();
    int getWidth() const  { return m_context->width;  }
    int getHeight() const { return m_context->height; }
    virtual PixelFormat getFormat() const { return m_context->pix_fmt; }
    virtual void flush();
    virtual const AVFrame* lastFrame() { return m_frame; }
    virtual void resetDecoder();
private:
    static AVCodec* findCodec(CodecID codecId);

    void openDecoder();
    void closeDecoder();


private:
    AVCodecContext *m_passedContext;

    static int hwcounter;
	AVCodec *m_codec;
	AVCodecContext *m_context;
    FrameTypeExtractor* m_frameTypeExtractor;
	AVFrame *m_frame;
    QImage m_tmpImg;
    CLVideoDecoderOutput m_tmpQtFrame;
    bool m_usedQtImage;

	
	quint8* m_deinterlaceBuffer;
	AVFrame *m_deinterlacedFrame;

#ifdef _USE_DXVA
    DecoderContext m_decoderContext;
#endif

	int m_width;
	int m_height;

	static bool m_first_instance;
	CodecID m_codecId;
	bool m_showmotion;
	CLAbstractVideoDecoder::DecodeMode m_decodeMode;
	CLAbstractVideoDecoder::DecodeMode m_newDecodeMode;

	unsigned int m_lightModeFrameCounter;
};

#endif //cl_ffmpeg_h
