#ifndef clcam_display_h_1211
#define clcam_display_h_1211



#include "decoders/video/abstractdecoder.h"
#include "videostreamdisplay.h"
#include "core/dataconsumer/dataconsumer.h"
#include "core/resource/resource_media_layout.h"
#include "utils/common/adaptivesleep.h"
#include "utils/media/externaltimesource.h"

class CLAbstractRenderer;
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

	void addVideoChannel(int index, CLAbstractRenderer* vw, bool can_downsacle);
	virtual bool processData(QnAbstractDataPacketPtr data);

	void pause();
	void resume();

	void setLightCPUMode(QnAbstractVideoDecoder::DecodeMode val);

	void display(QnCompressedVideoDataPtr vd, bool sleep, float speed);
	void playAudio(bool play);
    void pauseAudio();
    void setSpeed(float speed);

    // schedule to clean up buffers all; 
    // schedule - coz I do not want to introduce mutexes
    //I assume that first incoming frame after jump is keyframe

	/**
	 * \returns                         Current time in microseconds.
	 */
    virtual qint64 getCurrentTime() const;
    virtual qint64 getNextTime() const;

    void setMTDecoding(bool value);

    void setSingleShotMode(bool single);

    QImage getScreenshot(int channel);
    bool isRealTimeSource() const { return m_isRealTimeSource; }

    void setExternalTimeSource(QnlTimeSource* value) { m_extTimeSrc = value; }

public slots:
    void onBeforeJump(qint64 time, bool makeshot);
    void onJumpOccured(qint64 time); 
    void onRealTimeStreamHint(bool value);
    void onSlowSourceHint();
    void onReaderPaused();
    void onReaderResumed();
    void onPrevFrameOccured();
signals:
    void reachedTheEnd();
    void liveMode(bool value);
private:
	bool haveAudio(float speed) const;

	// puts in in queue and returns first in queue
	QnCompressedVideoDataPtr nextInOutVideodata(QnCompressedVideoDataPtr incoming, int channel);

    // this function doest not changes any quues; it just returns time of next frame been displayed
    quint64 nextVideoImageTime(QnCompressedVideoDataPtr incoming, int channel) const;

    quint64 nextVideoImageTime(int channel) const;

	void clearVideoQueue();
    void enqueueVideo(QnCompressedVideoDataPtr vd);
    void afterJump(qint64 new_time);
    void processNewSpeed(float speed);
private:
	QQueue<QnCompressedVideoDataPtr> m_videoQueue[CL_MAX_CHANNELS];

	CLVideoStreamDisplay* m_display[CL_MAX_CHANNELS];
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
    qint64 m_lastDisplayedVideoTime;

    qint64 m_previousVideoTime;
    quint64 m_lastNonZerroDuration;
    qint64 m_lastSleepInterval;
    //quint64 m_previousVideoDisplayedTime;

    bool m_afterJump;

    int m_displayLasts;

    bool m_ignoringVideo;

    bool mGenerateEndOfStreamSignal;

    bool m_isRealTimeSource;
	QAudioFormat m_expectedAudioFormat;
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
    QnlTimeSource* m_extTimeSrc;
    
    qint64 m_nextTime;
    mutable QMutex m_timeMutex;
};

#endif //clcam_display_h_1211
