#ifndef QN_CAM_DISPLAY_H
#define QN_CAM_DISPLAY_H

#include <QtCore/QTime>

#include <utils/common/adaptive_sleep.h>
#include <utils/media/externaltimesource.h>

#include <core/resource/resource_fwd.h>
#include <nx/streaming/abstract_data_consumer.h>
#include <nx/streaming/audio_data_packet.h>
#include <core/resource/resource_media_layout.h>

#include <decoders/video/abstract_video_decoder.h>

#include "video_stream_display.h"
#include <map>

// TODO: #GDM use forward declaration
#include <nx/core/transcoding/filters/legacy_transcoding_settings.h>

class QnAbstractRenderer;
class QnVideoStreamDisplay;
class QnAudioStreamDisplay;
class QnCompressedVideoData;
class QnArchiveStreamReader;
class QnlTimeSource;

/*
* This class is not duplicate statistics from Reader. If not enough CPU/network this class still show full (correct) stream fps
*/
class QnFpsStatistics
{
public:
    static const int MAX_QUEUE_SIZE = 30;

    QnFpsStatistics(): m_lastTime(AV_NOPTS_VALUE), m_queue(MAX_QUEUE_SIZE), m_queueSum(0) {}
    void updateFpsStatistics(QnCompressedVideoDataPtr vd);
    int getFps() const;
private:
    mutable QnMutex m_mutex;
    qint64 m_lastTime;
    QnUnsafeQueue<qint64> m_queue;
    qint64 m_queueSum;
};

/**
  * Stores QnVideoStreamDisplay for each channel/sensor
  */
class QnCamDisplay : public QnAbstractDataConsumer, public QnlTimeSource
{
    Q_OBJECT
public:
    QnCamDisplay(QnMediaResourcePtr resource, QnArchiveStreamReader* reader);
    ~QnCamDisplay();

    void addVideoRenderer(int channelCount, QnAbstractRenderer* vw, bool canDownscale);
    void removeVideoRenderer(QnAbstractRenderer* vw);
    int channelsCount() const;

    virtual bool processData(const QnAbstractDataPacketPtr& data);

    virtual void pleaseStop() override;

    void pause();
    void resume();

    void setLightCPUMode(QnAbstractVideoDecoder::DecodeMode val);

    bool display(QnCompressedVideoDataPtr vd, bool sleep, float speed);
    bool doDelayForAudio(QnConstCompressedAudioDataPtr ad, float speed);
    bool isAudioBuffering() const;
    void playAudio(bool play);
    float getSpeed() const;

    // schedule to clean up buffers all;
    // schedule - coz I do not want to introduce mutexes
    //I assume that first incoming frame after jump is keyframe

    /**
     * \returns                         Current time in microseconds.
     */

    virtual qint64 getCurrentTime() const;
    virtual qint64 getDisplayedTime() const;
    virtual qint64 getExternalTime() const;
    virtual qint64 getNextTime() const;

    void setMTDecoding(bool value);

    QSize getFrameSize(int channel) const;
    QImage getScreenshot(const QnLegacyTranscodingSettings& imageProcessingParams, bool anyQuality);
    QImage getGrayscaleScreenshot(int channel);
    QSize getVideoSize() const;
    bool isRealTimeSource() const;

    void setExternalTimeSource(QnlTimeSource* value);

    bool canAcceptData() const;
    bool isLongWaiting() const;
    bool isEOFReached() const;
    bool isStillImage() const;
    virtual void putData(const QnAbstractDataPacketPtr& data) override;
    QSize getMaxScreenSize() const;
    QnArchiveStreamReader* getArchiveReader() const;
    bool isFullScreen() const;
    bool isZoomWindow() const;
    void setFullScreen(bool fullScreen);

    bool isFisheyeEnabled() const;
    void setFisheyeEnabled(bool fisheyeEnabled);

    int getAvarageFps() const;
    virtual bool isBuffering() const override;

    qreal overridenAspectRatio() const;
    void setOverridenAspectRatio(qreal aspectRatio);

    const QSize& getRawDataSize() const {
        return m_display[0]->getRawDataSize();
    }

    QnMediaResourcePtr resource() const;

public slots:
    void onBeforeJump(qint64 time);
    void onSkippingFrames(qint64 time);
    void onJumpOccured(qint64 time);
    void onJumpCanceled(qint64 time);
    void onRealTimeStreamHint(bool value);
    void onSlowSourceHint();
    void onReaderPaused();
    void onReaderResumed();
    void onPrevFrameOccured();
    void onNextFrameOccured();

signals:
    void liveMode(bool value);
    void stillImageChanged();

protected:
    void setSingleShotMode(bool single);
    virtual void setSpeed(float speed) override;

    bool haveAudio(float speed) const;

    // puts in in queue and returns first in queue
    QnCompressedVideoDataPtr nextInOutVideodata(QnCompressedVideoDataPtr incoming, int channel);

    // this function doest not changes any quues; it just returns time of next frame been displayed
    quint64 nextVideoImageTime(QnCompressedVideoDataPtr incoming, int channel) const;

    quint64 nextVideoImageTime(int channel) const;

    void clearVideoQueue();
    void enqueueVideo(QnCompressedVideoDataPtr vd);
    QnCompressedVideoDataPtr dequeueVideo(int channel);
    bool isAudioHoleDetected(QnCompressedVideoDataPtr vd);
    void afterJump(QnAbstractMediaDataPtr media);
    void processNewSpeed(float speed);
    bool useSync(QnConstAbstractMediaDataPtr md);
    int getBufferingMask();
    void pauseAudio();

private:
    void hurryUpCheck(QnCompressedVideoDataPtr vd, float speed, qint64 needToSleep, qint64 realSleepTime);
    void hurryUpCheckForCamera(QnCompressedVideoDataPtr vd, float speed, qint64 needToSleep, qint64 realSleepTime);
    void hurryUpCheckForLocalFile(QnCompressedVideoDataPtr vd, float speed, qint64 needToSleep, qint64 realSleepTime);
    void hurryUpCkeckForCamera2(QnAbstractMediaDataPtr media);
    qint64 getMinReverseTime() const;

    qint64 getDisplayedMax() const;
    qint64 getDisplayedMin() const;
    void setAudioBufferSize(int bufferSize, int prebufferMs);

    void blockTimeValue(qint64 time);
    void blockTimeValueSafe(qint64 time); // can be called from other thread
    void unblockTimeValue();
    void waitForFramesDisplayed();
    void restoreVideoQueue(QnCompressedVideoDataPtr incoming, QnCompressedVideoDataPtr vd, int channel);
    template <class T> void markIgnoreBefore(const T& queue, qint64 time);
    bool needBuffering(qint64 vTime) const;
    void processSkippingFramesTime();
    void clearMetaDataInfo();
    void mapMetadataFrame(const QnCompressedVideoDataPtr& video);
    qint64 doSmartSleep(const qint64 needToSleep, float speed);

    static qint64 initialLiveBufferMkSecs();
    static qint64 maximumLiveBufferMkSecs();

protected:
    QnVideoStreamDisplay* m_display[CL_MAX_CHANNELS];
    QQueue<QnCompressedVideoDataPtr> m_videoQueue[CL_MAX_CHANNELS];

    QnAudioStreamDisplay* m_audioDisplay;

    QnAdaptiveSleep m_delay;

    float m_speed;
    float m_prevSpeed;

    bool m_playAudio;
    bool m_needChangePriority;
    bool m_hadAudio; // got at least one audio packet

    qint64 m_lastAudioPacketTime;
    qint64 m_syncAudioTime;
    int m_totalFrames;
    int m_iFrames;
    qint64 m_lastVideoPacketTime;
    qint64 m_lastDecodedTime;
    qint64 m_previousVideoTime;
    quint64 m_lastNonZerroDuration;
    qint64 m_lastSleepInterval;
    bool m_afterJump;
    bool m_bofReceived;
    int m_displayLasts;
    bool m_ignoringVideo;
    bool m_isRealTimeSource;
    QnAudioFormat m_expectedAudioFormat;
    mutable QnMutex m_audioChangeMutex;
    bool m_videoBufferOverflow;
    bool m_singleShotMode;
    bool m_singleShotQuantProcessed;
    qint64 m_jumpTime;
    QnCodecAudioFormat m_playingFormat;
    int m_storedMaxQueueSize;
    QnAbstractVideoDecoder::DecodeMode m_lightCpuMode;
    QnVideoStreamDisplay::FrameDisplayStatus m_lastFrameDisplayed;
    bool m_realTimeHurryUp;
    int m_delayedFrameCount;
    QnlTimeSource* m_extTimeSrc;

    bool m_useMtDecoding;
    int m_buffering;
    int m_executingJump;
    int m_skipPrevJumpSignal;
    int m_processedPackets;
    std::map<qint64, QnAbstractCompressedMetadataPtr> m_lastMetadata[CL_MAX_CHANNELS];
    qint64 m_nextReverseTime[CL_MAX_CHANNELS];
    int m_emptyPacketCounter;
    bool m_isStillImage;
    bool m_isLongWaiting;
    qint64 m_skippingFramesTime;

    bool m_executingChangeSpeed;
    bool m_eofSignalSended;
    int m_audioBufferSize;
    qint64 m_minAudioDetectJumpInterval;
    qint64 m_videoQueueDuration;
    bool m_useMTRealTimeDecode; // multi thread decode for live temporary allowed
    bool m_forceMtDecoding; // force multi thread decode in any case

    mutable QnMutex m_timeMutex;
    QnMediaResourcePtr m_resource;
    QTime m_afterJumpTimer;
    qint64 m_firstAfterJumpTime;
    qint64 m_receivedInterval;
    QnArchiveStreamReader* m_archiveReader;

    bool m_fullScreen;
    QnFpsStatistics m_fpsStat;
    int m_prevLQ;
    bool m_doNotChangeDisplayTime;
    bool m_multiView;
    bool m_fisheyeEnabled;
    int m_channelsCount;

    qint64 m_lastQueuedVideoTime;
    int m_liveBufferSize;
    bool m_liveMaxLenReached;
    bool m_hasVideo;
};

#endif //QN_CAM_DISPLAY_H
