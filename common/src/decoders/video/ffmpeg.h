#ifndef cl_ffmpeg_h2026
#define cl_ffmpeg_h2026

#include "abstractdecoder.h"

#ifdef _USE_DXVA
#include "dxva/dxva.h"
#endif
#include "utils/media/ffmpeg_helper.h"


struct AVCodec;
struct AVCodecContext;
struct AVFrame;
struct MpegEncContext;

// client of this class is responsible for encoded data buffer meet ffmpeg restrictions
// ( see comment to decode functions for details ).

class CLFFmpegVideoDecoder : public QnAbstractVideoDecoder
{
public:
	CLFFmpegVideoDecoder(CodecID codec, const QnCompressedVideoDataPtr data, bool mtDecoding);
    bool decode(const QnCompressedVideoDataPtr data, CLVideoDecoderOutput* outFrame);
	~CLFFmpegVideoDecoder();

	void showMotion(bool show);

	virtual void setLightCpuMode(DecodeMode val);

    static bool isHardwareAccellerationPossible(CodecID codecId, int width, int height)
    {
        return codecId == CODEC_ID_H264 && width <= 1920 && height <= 1088;
    }

    AVCodecContext* getContext() const;
    bool getLastDecodedFrame(CLVideoDecoderOutput* outFrame);

    PixelFormat GetPixelFormat();
    int getWidth() const  { return m_context->width;  }
    int getHeight() const { return m_context->height; }
    double getSampleAspectRatio() const;
    virtual PixelFormat getFormat() const { return m_context->pix_fmt; }
    virtual void flush();
    virtual const AVFrame* lastFrame() { return m_frame; }
    void determineOptimalThreadType(const QnCompressedVideoDataPtr data);
    virtual void setMTDecoding(bool value);
    virtual void resetDecoder(QnCompressedVideoDataPtr data);
private:
    static AVCodec* findCodec(CodecID codecId);

    void openDecoder(const QnCompressedVideoDataPtr data);
    void closeDecoder();
    int findMotionInfo(qint64 pkt_dts);

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
    bool m_checkH264ResolutionChange;

#ifdef _USE_DXVA
    DecoderContext m_decoderContext;
#endif

	int m_width;
	int m_height;

	static bool m_first_instance;
	CodecID m_codecId;
	bool m_showmotion;
	QnAbstractVideoDecoder::DecodeMode m_decodeMode;
	QnAbstractVideoDecoder::DecodeMode m_newDecodeMode;

	unsigned int m_lightModeFrameCounter;

    int m_currentWidth;
    int m_currentHeight;

    int m_forceSliceDecoding;
    typedef QVector<QPair<qint64, QnMetaDataV1Ptr> > MotionMap; // I have used vector instead map because of 2-3 elements is tipical size
    MotionMap m_motionMap; 
};

#endif //cl_ffmpeg_h
