#ifndef cl_ffmpeg_h2026
#define cl_ffmpeg_h2026

#include <QtGui/QImage>

#include "abstractdecoder.h"
#include "core/datapacket/video_data_packet.h"

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
class CLFFmpegVideoDecoder
:
    public QnAbstractVideoDecoder
{
public:
    /*!
        \param swDecoderCount Atomically incremented in constructor and atommically decremented in destructor
    */
    CLFFmpegVideoDecoder(CodecID codec, const QnConstCompressedVideoDataPtr& data, bool mtDecoding, QAtomicInt* const swDecoderCount = NULL);
    ~CLFFmpegVideoDecoder();
    bool decode( const QnConstCompressedVideoDataPtr& data, QSharedPointer<CLVideoDecoderOutput>* const outFrame );

    void showMotion(bool show);

    virtual void setLightCpuMode(DecodeMode val);

    static bool isHardwareAccellerationPossible(CodecID codecId, int width, int height)
    {
        return codecId == CODEC_ID_H264 && width <= 1920 && height <= 1088;
    }

    AVCodecContext* getContext() const;

    PixelFormat GetPixelFormat() const;
    QnAbstractPictureDataRef::PicStorageType targetMemoryType() const;
    int getWidth() const  { return m_context->width;  }
    int getHeight() const { return m_context->height; }
    //!Implementation of QnAbstractVideoDecoder::getOriginalPictureSize
    virtual QSize getOriginalPictureSize() const;
    double getSampleAspectRatio() const;
    virtual PixelFormat getFormat() const { return m_context->pix_fmt; }
    virtual void flush();
    virtual const AVFrame* lastFrame() const { return m_frame; }
    void determineOptimalThreadType(const QnConstCompressedVideoDataPtr& data);
    virtual void setMTDecoding(bool value);
    virtual void resetDecoder(const QnConstCompressedVideoDataPtr& data);
    virtual void setOutPictureSize( const QSize& outSize );
    //!Implementation of QnAbstractVideoDecoder::getDecoderCaps
    /*!
        Supports \a multiThreadedMode
    */
    virtual unsigned int getDecoderCaps() const;
    virtual void setSpeed( float newValue ) override;
    void forceMtDecoding(bool value);
private:
    static AVCodec* findCodec(CodecID codecId);

    void openDecoder(const QnConstCompressedVideoDataPtr& data);
    void closeDecoder();
    int findMotionInfo(qint64 pkt_dts);
    void reallocateDeinterlacedFrame();
    void processNewResolutionIfChanged(const QnConstCompressedVideoDataPtr& data, int width, int height);
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

    int m_width;
    int m_height;

    static bool m_first_instance;
    CodecID m_codecId;
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
    typedef QVector<QPair<qint64, QnMetaDataV1Ptr> > MotionMap; // I have used vector instead map because of 2-3 elements is tipical size
    MotionMap m_motionMap; 
    QAtomicInt* const m_swDecoderCount;
    mutable double m_prevSampleAspectRatio;
    bool m_forcedMtDecoding;
    qint64 m_prevTimestamp;
};

#endif //cl_ffmpeg_h
