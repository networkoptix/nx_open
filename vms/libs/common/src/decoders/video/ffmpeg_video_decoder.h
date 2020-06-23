#ifndef FFMPEG_VIDEO_DECODER_H
#define FFMPEG_VIDEO_DECODER_H

#ifdef ENABLE_DATA_PROVIDERS

#include <QtGui/QImage>

#include "abstract_video_decoder.h"
#include "nx/streaming/video_data_packet.h"
#include <deque>

#ifdef _USE_DXVA
#include "dxva/dxva.h"
#endif
#include "utils/media/ffmpeg_helper.h"

class FrameTypeExtractor;
struct AVCodec;
struct AVCodecContext;
struct AVFrame;
struct MpegEncContext;

/**
 * Client of this class is responsible for encoded data buffer to meet ffmpeg
 * restrictions (see the comment for decoding functions for details).
 */
class QnFfmpegVideoDecoder: public QnAbstractVideoDecoder
{
public:
    /*!
        \param swDecoderCount Atomically incremented in constructor and atomically decremented in destructor
    */
    QnFfmpegVideoDecoder(const DecoderConfig& config, const QnConstCompressedVideoDataPtr& data);
    ~QnFfmpegVideoDecoder();
    bool decode( const QnConstCompressedVideoDataPtr& data, QSharedPointer<CLVideoDecoderOutput>* const outFrame );

    void showMotion(bool show);

    virtual void setLightCpuMode(DecodeMode val);

    static bool isHardwareAccellerationPossible(AVCodecID codecId, int width, int height)
    {
        return codecId == AV_CODEC_ID_H264 && width <= 1920 && height <= 1088;
    }

    AVCodecContext* getContext() const;

    virtual AVPixelFormat GetPixelFormat() const override;
    QnAbstractPictureDataRef::PicStorageType targetMemoryType() const;
    int getWidth() const  { return m_context->width;  }
    int getHeight() const { return m_context->height; }
    //!Implementation of QnAbstractVideoDecoder::getOriginalPictureSize
    virtual QSize getOriginalPictureSize() const override;
    double getSampleAspectRatio() const;
    virtual AVPixelFormat getFormat() const { return m_context->pix_fmt; }
    virtual void flush();
    virtual const AVFrame* lastFrame() const override { return m_frame; }
    void determineOptimalThreadType(const QnConstCompressedVideoDataPtr& data);
    void setMultiThreadDecodePolicy(MultiThreadDecodePolicy mtDecodingPolicy);
    virtual void resetDecoder(const QnConstCompressedVideoDataPtr& data) override;
    virtual void setOutPictureSize( const QSize& outSize ) override;
    //!Implementation of QnAbstractVideoDecoder::getDecoderCaps
    /*!
        Supports \a multiThreadedMode
    */
    virtual unsigned int getDecoderCaps() const override;
    virtual void setSpeed( float newValue ) override;

    int getLastDecodeResult() const { return m_lastDecodeResult; }
private:
    static AVCodec* findCodec(AVCodecID codecId);

    void openDecoder(const QnConstCompressedVideoDataPtr& data);
    void closeDecoder();
    void reallocateDeinterlacedFrame();
    void processNewResolutionIfChanged(const QnConstCompressedVideoDataPtr& data, int width, int height);
    int decodeVideo(
        AVCodecContext *avctx,
        AVFrame *picture,
        int *got_picture_ptr,
        const AVPacket *avpkt);
    void setMultiThreadDecoding(bool value);
private:
    AVCodecContext *m_passedContext;

    static int hwcounter;
    AVCodec *m_codec;
    AVCodecContext *m_context;

    AVFrame *m_frame;
    QImage m_tmpImg;
    CLVideoDecoderOutput m_tmpQtFrame;


    AVFrame *m_deinterlacedFrame;

#ifdef _USE_DXVA
    DecoderContext m_decoderContext;
#endif

    //int m_width;
    //int m_height;

    static bool m_first_instance;
    AVCodecID m_codecId;
    bool m_showmotion;
    QnAbstractVideoDecoder::DecodeMode m_decodeMode;
    QnAbstractVideoDecoder::DecodeMode m_newDecodeMode;

    unsigned int m_lightModeFrameCounter;
    FrameTypeExtractor* m_frameTypeExtractor;
    quint8* m_deinterlaceBuffer;
    bool m_usedQtImage;

    int m_currentWidth;
    int m_currentHeight;
    bool m_checkH264ResolutionChange;

    int m_forceSliceDecoding;
    // I have used vector instead map because of 2-3 elements is typical size
    typedef QVector<QPair<qint64, FrameMetadata> > MotionMap;
    mutable double m_prevSampleAspectRatio;
    qint64 m_prevTimestamp;
    bool m_spsFound;
    MultiThreadDecodePolicy m_mtDecodingPolicy;
    bool m_useMtDecoding;
    bool m_needRecreate;
    int m_lastDecodeResult = 0;
};

#endif // ENABLE_DATA_PROVIDERS

#endif //FFMPEG_VIDEO_DECODER_H
