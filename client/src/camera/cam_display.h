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
class QnArchiveStreamReader;

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
    mutable QMutex m_mutex;
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

    void addVideoChannel(int index, QnAbstractRenderer* vw, bool can_downsacle);
    virtual bool processData(QnAbstractDataPacketPtr data);

    void pause();
    void resume();

    void setLightCPUMode(QnAbstractVideoDecoder::DecodeMode val);

    bool display(QnCompressedVideoDataPtr vd, bool sleep, float speed);
    void playAudio(bool play);
    void setSpeed(float speed);
    float getSpeed() const;
    bool isSyncAllowed() const;

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
    QSize getVideoSize() const;
    bool isRealTimeSource() const;

    void setExternalTimeSource(QnlTimeSource* value);

    bool canAcceptData() const;
    bool isLongWaiting() const;
    bool isEOFReached() const;
    bool isStillImage() const;
    virtual void putData(QnAbstractDataPacketPtr data) override;
    QSize getScreenSize() const;
    QnArchiveStreamReader* getArchiveReader();
    bool isFullScreen() const;
    void setFullScreen(bool fullScreen);
    int getAvarageFps() const;
    virtual bool isBuffering() const override;
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
    void unblockTimeValue();
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
    int m_fczFrames;
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
    QMutex m_audioChangeMutex;
    bool m_videoBufferOverflow;
    bool m_singleShotMode;
    bool m_singleShotQuantProcessed;
    qint64 m_jumpTime;
    QnCodecAudioFormat m_playingFormat;
    int m_storedMaxQueueSize;
    QnAbstractVideoDecoder::DecodeMode m_lightCpuMode;
    QnVideoStreamDisplay::FrameDisplayStatus m_lastFrameDisplayed;
    int m_realTimeHurryUp;
    int m_delayedFrameCount;
    QnlTimeSource* m_extTimeSrc;
    
    bool m_useMtDecoding;
    int m_buffering;
    int m_executingJump;
    int m_skipPrevJumpSignal;
    int m_processedPackets;
    QnMetaDataV1Ptr m_lastMetadata[CL_MAX_CHANNELS];
    qint64 m_nextReverseTime[CL_MAX_CHANNELS];
    int m_emptyPacketCounter;
    bool m_isStillImage;
    bool m_isLongWaiting;
    
    bool m_executingChangeSpeed;
    bool m_eofSignalSended;
    int m_audioBufferSize;
    qint64 m_minAudioDetectJumpInterval;
    qint64 m_videoQueueDuration;
    bool m_useMTRealTimeDecode;

    mutable QMutex m_timeMutex;
    QnMediaResourcePtr m_resource;
	QTime m_afterJumpTimer;
	qint64 m_firstAfterJumpTime;
	qint64 m_receivedInterval;
    QnArchiveStreamReader* m_archiveReader;

    bool m_fullScreen;
    QnFpsStatistics m_fpsStat;
    int m_prevLQ;
    bool m_doNotChangeDisplayTime;
};

#endif //QN_CAM_DISPLAY_H
