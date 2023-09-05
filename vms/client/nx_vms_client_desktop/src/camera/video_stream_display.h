// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>
#include <set>

#include <QtCore/QQueue>

#include <decoders/video/abstract_video_decoder.h>
#include <nx/media/abstract_metadata_consumer.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/stoppable.h>
#include <transcoding/filters/filter_helper.h>

#include "frame_scaler.h"

extern "C" {
struct SwsContext;
}

class QnAbstractVideoDecoder;
class QnCompressedVideoData;
class QnResourceWidgetRenderer;
class QnBufferedFrameDisplayer;

constexpr int kMaxQueueTime = 1000 * 200;

/**
  * Display one video stream. Decode the video and pass it to video window.
  */
class QnVideoStreamDisplay: public ScreenshotInterface
{
public:
    enum FrameDisplayStatus {Status_Displayed, Status_Skipped, Status_Buffered};

    QnVideoStreamDisplay(const QnMediaResourcePtr& resource, bool can_downscale, int channelNumber);
    virtual ~QnVideoStreamDisplay();

    int addRenderer(QnResourceWidgetRenderer* draw);
    int removeRenderer(QnResourceWidgetRenderer* draw);

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

    QnAspectRatio overridenAspectRatio() const;
    void setOverridenAspectRatio(QnAspectRatio aspectRatio);
    void endOfRun();

public: // used only by QnCamDisplay
    void waitForFramesDisplayed();
    QSize getMaxScreenSizeUnsafe() const;

    QnVideoStreamDisplay::FrameDisplayStatus flushFrame(
        int channel, QnFrameScaler::DownscaleFactor force_factor);
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
    const QSize& getRawDataSize() const { return m_rawDataSize; }
    MultiThreadDecodePolicy toEncoderPolicy(bool useMtDecoding) const;

private:
    mutable nx::Mutex m_mtx;

    struct Decoder
    {
        AVCodecID compressionType = AVCodecID::AV_CODEC_ID_NONE;
        std::unique_ptr<QnAbstractVideoDecoder> decoder;
    };
    Decoder m_decoderData;

    std::set<QnResourceWidgetRenderer*> m_newList;
    std::set<QnResourceWidgetRenderer*> m_renderList;
    bool m_renderListModified;
    QnMediaResourcePtr m_resource;

    QnAbstractVideoDecoder::DecodeMode m_decodeMode;
    bool m_canDownscale;
    const int m_channelNumber;

    QnFrameScaler::DownscaleFactor m_prevFactor;
    QnFrameScaler::DownscaleFactor m_scaleFactor;
    QSize m_previousOnScreenSize;

    SwsContext* m_scaleContext;
    int m_outputWidth;
    int m_outputHeight;
    std::atomic_bool m_mtDecoding = false;
    std::atomic_bool m_needReinitDecoders = false;
    bool m_reverseMode;
    bool m_prevReverseMode;
    QQueue<QSharedPointer<CLVideoDecoderOutput>> m_reverseQueue;
    bool m_flushedBeforeReverseStart;
    qint64 m_reverseSizeInBytes;
    bool m_timeChangeEnabled;
    std::unique_ptr<QnBufferedFrameDisplayer> m_bufferedFrameDisplayer;
    bool m_canUseBufferedFrameDisplayer;
    QSize m_rawDataSize;
    float m_speed;
    bool m_queueWasFilled;
    bool m_needResetDecoder;
    mutable nx::Mutex m_lastDisplayedFrameMutex;
    CLConstVideoDecoderOutputPtr m_lastDisplayedFrame;
    QSize m_imageSize;
    mutable nx::Mutex m_imageSizeMtx;
    int m_prevSrcWidth;
    int m_prevSrcHeight;
    qint64 m_lastIgnoreTime;
    mutable nx::Mutex m_renderListMtx;
    bool m_isPaused;
    bool m_isLive = false;
    QnAspectRatio m_overridenAspectRatio;
    std::optional<std::chrono::microseconds> m_blockedTimeValue;

private:
    void reorderPrevFrames();
    bool allocScaleContext(const CLVideoDecoderOutput& outFrame, int newWidth, int newHeight);
    void freeScaleContext();
    bool rescaleFrame(const CLVideoDecoderOutput& srcFrame, CLVideoDecoderOutput& outFrame, int newWidth, int newHeight);
    void flushReverseBlock(
        QnAbstractVideoDecoder* dec, QnCompressedVideoDataPtr data, QnFrameScaler::DownscaleFactor forceScaleFactor);
    bool shouldUpdateDecoder(
        const QnConstCompressedVideoDataPtr& data, bool reverseMode);

    QnAbstractVideoDecoder* createVideoDecoder(
        const QnConstCompressedVideoDataPtr& data, bool mtDecoding) const;

    QnFrameScaler::DownscaleFactor determineScaleFactor(
        std::set<QnResourceWidgetRenderer*> renderList,
        int channelNumber,
        int srcWidth,
        int srcHeight,
        QnFrameScaler::DownscaleFactor forceFactor);
    bool processDecodedFrame(
        QnAbstractVideoDecoder* dec,
        const CLConstVideoDecoderOutputPtr& outFrame);
    void checkQueueOverflow();
    void clearReverseQueue();

    void calcSampleAR(QSharedPointer<CLVideoDecoderOutput> outFrame, QnAbstractVideoDecoder* dec);

    bool downscaleOrForward(
        const CLConstVideoDecoderOutputPtr& src,
        CLVideoDecoderOutputPtr& dst,
        QnFrameScaler::DownscaleFactor forceScaleFactor);
};
