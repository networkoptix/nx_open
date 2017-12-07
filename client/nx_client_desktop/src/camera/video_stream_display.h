#pragma once

#include <set>

extern "C"
{
    #include <libswscale/swscale.h>
}

#include <QtCore/QQueue>

#include "decoders/video/abstract_video_decoder.h"
#include <nx/utils/thread/stoppable.h>
#include <nx/utils/thread/mutex.h>
#include "frame_scaler.h"
#include "transcoding/filters/filter_helper.h"
#include <nx/media/abstract_metadata_consumer.h>


class QnAbstractVideoDecoder;
class QnCompressedVideoData;
class QnAbstractRenderer;
class QnBufferedFrameDisplayer;

static const int kMaxFrameQueueSize = 6;
static const int MAX_QUEUE_TIME = 1000 * 200;

/**
  * Display one video stream. Decode the video and pass it to video window.
  */
class QnVideoStreamDisplay
:
    public QnStoppable, public ScreenshotInterface
{
public:
    // TODO: #Elric #enum
    enum FrameDisplayStatus {Status_Displayed, Status_Skipped, Status_Buffered};

    QnVideoStreamDisplay(bool can_downscale, int channelNumber);
    virtual ~QnVideoStreamDisplay() override;

    //!Implementation of QnStoppable::pleaseStop()
    virtual void pleaseStop() override;

    int addRenderer(QnAbstractRenderer* draw);
    int removeRenderer(QnAbstractRenderer* draw);

    void addMetadataConsumer(const nx::media::AbstractMetadataConsumerPtr& metadataConsumer);
    void removeMetadataConsumer(const nx::media::AbstractMetadataConsumerPtr& metadataConsumer);

    FrameDisplayStatus display(
        QnCompressedVideoDataPtr data,
        bool draw,
        QnFrameScaler::DownscaleFactor force_factor = QnFrameScaler::factor_any);

    void setLightCPUMode(QnAbstractVideoDecoder::DecodeMode val);

    QnFrameScaler::DownscaleFactor getCurrentDownscaleFactor() const;

    void setMTDecoding(bool value);

    void setSpeed(float value);
    virtual CLVideoDecoderOutputPtr getScreenshot(bool anyQuality) override;
    virtual QImage getGrayscaleScreenshot() override;
    void setCurrentTime(qint64 time);
    void canUseBufferedFrameDisplayer(bool value);
    qint64 nextReverseTime() const;
    QSize getImageSize() const;

    QSize getMaxScreenSize() const;
    bool selfSyncUsed() const;

    qreal overridenAspectRatio() const;
    void setOverridenAspectRatio(qreal aspectRatio);
private:
    friend class QnCamDisplay;

    void waitForFramesDisplayed();
    /**
      * Return last decoded frame
      */
    QSharedPointer<CLVideoDecoderOutput> flush(QnFrameScaler::DownscaleFactor force_factor, int channelNum);
    QnVideoStreamDisplay::FrameDisplayStatus flushFrame(int channel, QnFrameScaler::DownscaleFactor force_factor);
    //!Blocks until all frames passed to \a display have been rendered
    void flushFramesToRenderer();
    void overrideTimestampOfNextFrameToRender(qint64 value);
    //!Returns timestamp of frame that will be rendered next. It can be already displayed frame (if no new frames available)
    qint64 getTimestampOfNextFrameToRender() const;
    void blockTimeValue(qint64 time);
    void blockTimeValueSafe(qint64 time);
    void setPausedSafe(bool value);
    void unblockTimeValue();
    bool isTimeBlocked() const;
    void afterJump();
    void onNoVideo();

    void updateRenderList();
    const QSize& getRawDataSize() const {
        return m_rawDataSize;
    }
private:
    mutable QnMutex m_mtx;
    QMap<AVCodecID, QnAbstractVideoDecoder*> m_decoder;

    std::set<QnAbstractRenderer*> m_newList;
    std::set<QnAbstractRenderer*> m_renderList;
    bool m_renderListModified;

    /**
      * to reduce image size for weak video cards
      */

    QSharedPointer<CLVideoDecoderOutput> m_frameQueue[kMaxFrameQueueSize];
    int m_frameQueueIndex;

    QnAbstractVideoDecoder::DecodeMode m_decodeMode;
    bool m_canDownscale;
    const int m_channelNumber;

    QnFrameScaler::DownscaleFactor m_prevFactor;
    QnFrameScaler::DownscaleFactor m_scaleFactor;
    QSize m_previousOnScreenSize;

    SwsContext *m_scaleContext;
    int m_outputWidth;
    int m_outputHeight;
    bool m_enableFrameQueue;
    bool m_queueUsed;
    bool m_needReinitDecoders;
    bool m_reverseMode;
    bool m_prevReverseMode;
    QQueue<QSharedPointer<CLVideoDecoderOutput> > m_reverseQueue;
    bool m_flushedBeforeReverseStart;
    qint64 m_reverseSizeInBytes;
    bool m_timeChangeEnabled;
    QnBufferedFrameDisplayer* m_bufferedFrameDisplayer;
    bool m_canUseBufferedFrameDisplayer;
    QSize m_rawDataSize;
private:
    float m_speed;
    bool m_queueWasFilled;
    bool m_needResetDecoder;
    QSharedPointer<CLVideoDecoderOutput> m_lastDisplayedFrame;
    QSize m_imageSize;
    mutable QnMutex m_imageSizeMtx;
    int m_prevSrcWidth;
    int m_prevSrcHeight;
    qint64 m_lastIgnoreTime;
    mutable QnMutex m_renderListMtx;
    bool m_isPaused;
    qreal m_overridenAspectRatio;

    mutable QnMutex m_metadataConsumersHashMutex;
    QMultiMap<MetadataType, QWeakPointer<nx::media::AbstractMetadataConsumer>>
        m_metadataConsumerByType;

    void reorderPrevFrames();
    bool allocScaleContext(const CLVideoDecoderOutput& outFrame, int newWidth, int newHeight);
    void freeScaleContext();
    bool rescaleFrame(const CLVideoDecoderOutput& srcFrame, CLVideoDecoderOutput& outFrame, int newWidth, int newHeight);

    QnFrameScaler::DownscaleFactor findScaleFactor(int width, int height, int fitWidth, int fitHeight);
    QnFrameScaler::DownscaleFactor determineScaleFactor(
        std::set<QnAbstractRenderer*> renderList,
        int channelNumber,
        int srcWidth,
        int srcHeight,
        QnFrameScaler::DownscaleFactor force_factor);
    bool processDecodedFrame(QnAbstractVideoDecoder* dec, const QSharedPointer<CLVideoDecoderOutput>& outFrame, bool enableFrameQueue, bool reverseMode);
    void processMetadata(const FrameMetadata& metadataList, int channel);
    void checkQueueOverflow(QnAbstractVideoDecoder* dec);
    void clearReverseQueue();
    /*!
        \param outFrame MUST contain initialized \a CLVideoDecoderOutput object, but method is allowed to return just reference
            to another frame and not copy data to this object (TODO)
    */
    bool getLastDecodedFrame( QnAbstractVideoDecoder* dec, QSharedPointer<CLVideoDecoderOutput>* const outFrame );

    void calcSampleAR(QSharedPointer<CLVideoDecoderOutput> outFrame, QnAbstractVideoDecoder* dec);

    bool downscaleFrame(const CLVideoDecoderOutputPtr& src, const CLVideoDecoderOutputPtr& dst, QnFrameScaler::DownscaleFactor scaleFactor, AVPixelFormat pixFmt);
};
