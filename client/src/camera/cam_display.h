#ifndef QN_CAM_DISPLAY_H
#define QN_CAM_DISPLAY_H

#include "decoders/video/abstractdecoder.h"
#include "video_stream_display.h"
#include "core/dataconsumer/abstract_data_consumer.h"
#include "core/resource/resource_media_layout.h"
#include "utils/common/adaptive_sleep.h"
#include "utils/media/externaltimesource.h"
#include "core/resource/resource_fwd.h"

class QnAbstractRenderer;
class QnVideoStreamDisplay;
class QnAudioStreamDisplay;
struct QnCompressedVideoData;

/**
  * Stores QnVideoStreamDisplay for each channel/sensor
  */
class QnCamDisplay : public QnAbstractDataConsumer, public QnlTimeSource
{
    Q_OBJECT
public:
    QnCamDisplay(QnMediaResourcePtr resource);
    ~QnCamDisplay();

    void addVideoChannel(int index, QnAbstractRenderer* vw, bool can_downsacle);
    virtual bool processData(QnAbstractDataPacketPtr data);

    void pause();
    void resume();

    void setLightCPUMode(QnAbstractVideoDecoder::DecodeMode val);

    bool display(QnCompressedVideoDataPtr vd, bool sleep, float speed);
    void playAudio(bool play);
    void setSpeed(float speed);
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
    QImage getScreenshot(int channel);
    bool isRealTimeSource() const;

    void setExternalTimeSource(QnlTimeSource* value);

    bool canAcceptData() const;
    bool isNoData() const;
    bool isStillImage() const;
    virtual void putData(QnAbstractDataPacketPtr data) override;
public slots:
    void onBeforeJump(qint64 time);
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
    bool useSync(QnCompressedVideoDataPtr vd);
    int getBufferingMask();

private:
    void hurryUpCheck(QnCompressedVideoDataPtr vd, float speed, qint64 needToSleep, qint64 realSleepTime);
    void hurryUpCheckForCamera(QnCompressedVideoDataPtr vd, float speed, qint64 needToSleep, qint64 realSleepTime);
    void hurryUpCheckForLocalFile(QnCompressedVideoDataPtr vd, float speed, qint64 needToSleep, qint64 realSleepTime);
	void hurryUpCkeckForCamera2(QnAbstractMediaDataPtr media);
    bool canSwitchToHighQuality();
    void resetQualityStatistics();
    qint64 getMinReverseTime() const;

    qint64 getDisplayedMax() const;
    qint64 getDisplayedMin() const;
    void setAudioBufferSize(int bufferSize, int prebufferMs);
    bool isLastVideoQualityLow() const;
protected:
    QnVideoStreamDisplay* m_display[CL_MAX_CHANNELS];
    QQueue<QnCompressedVideoDataPtr> m_videoQueue[CL_MAX_CHANNELS];

    QnAudioStreamDisplay* m_audioDisplay;

    QnAdaptiveSleep m_delay;

    bool m_playAudioSet;
    float m_speed;
    float m_prevSpeed;

    bool m_playAudio;
    bool m_needChangePriority;

    /**
     * got at least one audio packet
     */
    bool m_hadAudio;

    qint64 m_lastAudioPacketTime;
    qint64 m_syncAudioTime;
    int m_totalFrames;
    int m_fczFrames;
    int m_iFrames;
    qint64 m_lastVideoPacketTime;
    qint64 m_lastDecodedTime;
    qint64 m_ignoreTime;

    qint64 m_previousVideoTime;
    quint64 m_lastNonZerroDuration;
    qint64 m_lastSleepInterval;
    //quint64 m_previousVideoDisplayedTime;

    bool m_afterJump;

    bool m_bofReceived;

    int m_displayLasts;

    bool m_ignoringVideo;

    bool m_isRealTimeSource;
    QnAudioFormat m_expectedAudioFormat;
    QMutex m_audioChangeMutex;
    bool m_videoBufferOverflow;
    bool m_singleShotMode;
    bool m_singleShotQuantProcessed;
    qint64 m_jumpTime;
    QnCodecAudioFormat m_playingFormat;
    int m_playingCompress;
    int m_playingBitrate;
    int m_tooSlowCounter;
    int m_storedMaxQueueSize;
    QnAbstractVideoDecoder::DecodeMode m_lightCpuMode;
    QnVideoStreamDisplay::FrameDisplayStatus m_lastFrameDisplayed;
    int m_realTimeHurryUp;
    int m_delayedFrameCount;
    QnlTimeSource* m_extTimeSrc;
    
    //qint64 m_nextTime;
    bool m_useMtDecoding;
    int m_buffering;
    int m_executingJump;
    int m_skipPrevJumpSignal;
    int m_processedPackets;
    QnMetaDataV1Ptr m_lastMetadata[CL_MAX_CHANNELS];
    qint64 m_nextReverseTime[CL_MAX_CHANNELS];
    float m_toLowQSpeed; // speed then switching to low quality for camera
    //QTime m_toLowQTimer; // try to change low to high quality (for normal playback speed every N seconds)
    int m_emptyPacketCounter;
    int m_hiQualityRetryCounter;
    bool m_isStillImage;
    bool m_isLongWaiting;
    

    static QSet<QnCamDisplay*> m_allCamDisplay;
    static QMutex m_qualityMutex;
    static qint64 m_lastQualitySwitchTime;
    bool m_executingChangeSpeed;
    bool m_eofSignalSended;
    bool m_lastLiveIsLowQuality;
    int m_audioBufferSize;
    qint64 m_minAudioDetectJumpInterval;
    qint64 m_videoQueueDuration;
    bool m_useMTRealTimeDecode;

    mutable QMutex m_timeMutex;
    QnMediaResourcePtr m_resource;
    bool m_isLastVideoQualityLow;
	QTime m_afterJumpTimer;
	qint64 m_firstAfterJumpTime;
	qint64 m_receivedInterval;
};

#endif //QN_CAM_DISPLAY_H
