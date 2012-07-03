#ifndef clcam_display_h_1211
#define clcam_display_h_1211



#include "decoders/video/abstractdecoder.h"
#include "videostreamdisplay.h"
#include "core/dataconsumer/dataconsumer.h"
#include "core/resource/resource_media_layout.h"
#include "utils/common/adaptivesleep.h"
#include "utils/media/externaltimesource.h"

class QnAbstractRenderer;
class CLVideoStreamDisplay;
class CLAudioStreamDisplay;
struct QnCompressedVideoData;

/**
  * Stores CLVideoStreamDisplay for each channel/sensor
  */
class CLCamDisplay : public QnAbstractDataConsumer, public QnlTimeSource
{
    Q_OBJECT
public:
	CLCamDisplay(bool generateEndOfStreamSignal);
	~CLCamDisplay();

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

    void setSingleShotMode(bool single);

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
    void reachedTheEnd();
    void liveMode(bool value);
    void stillImageChanged();

protected:
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
    bool canSwitchToHighQuality();
    void resetQualityStatistics();
    qint64 getMinReverseTime() const;

    qint64 getDisplayedMax() const;
    qint64 getDisplayedMin() const;
    void setAudioBufferSize(int bufferSize, int prebufferMs);
protected:
    CLVideoStreamDisplay* m_display[CL_MAX_CHANNELS];
	QQueue<QnCompressedVideoDataPtr> m_videoQueue[CL_MAX_CHANNELS];

	CLAudioStreamDisplay* m_audioDisplay;

	CLAdaptiveSleep m_delay;

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
    int m_iFrames;
    qint64 m_lastVideoPacketTime;
    qint64 m_lastDecodedTime;
    qint64 m_ignoreTime;

    qint64 m_previousVideoTime;
    quint64 m_lastNonZerroDuration;
    qint64 m_lastSleepInterval;
    //quint64 m_previousVideoDisplayedTime;

    bool m_afterJump;

    int m_displayLasts;

    bool m_ignoringVideo;

    bool mGenerateEndOfStreamSignal;

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
    CLVideoStreamDisplay::FrameDisplayStatus m_lastFrameDisplayed;
    int m_realTimeHurryUp;
    int m_delayedFrameCnt;
    QnlTimeSource* m_extTimeSrc;
    
    //qint64 m_nextTime;
    mutable QMutex m_timeMutex;
    bool m_useMtDecoding;
    int m_buffering;
    int m_executingJump;
    int skipPrevJumpSignal;
    int m_processedPackets;
    QnMetaDataV1Ptr m_lastMetadata[CL_MAX_CHANNELS];
    bool m_bofReceived;
    qint64 m_nextReverseTime[CL_MAX_CHANNELS];
    float m_toLowQSpeed; // speed then switching to low quality for camera
    //QTime m_toLowQTimer; // try to change low to high quality (for normal playback speed every N seconds)
    int m_emptyPacketCounter;
    bool m_isStillImage;
    bool m_isLongWaiting;
    
    int m_hiQualityRetryCounter;

    static QSet<CLCamDisplay*> m_allCamDisplay;
    static QMutex m_qualityMutex;
    static qint64 m_lastQualitySwitchTime;
    bool m_executingChangeSpeed;
    bool m_eofSignalSended;
    bool m_lastLiveIsLowQuality;
    int m_audioBufferSize;
    qint64 m_minAudioDetectJumpInterval;
    qint64 m_videoQueueDuration;
};

#endif //clcam_display_h_1211
