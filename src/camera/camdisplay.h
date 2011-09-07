#ifndef clcam_display_h_1211
#define clcam_display_h_1211

#include "../data/dataprocessor.h"
#include "base/adaptivesleep.h"
#include "device/device_video_layout.h"
#include "streamreader/streamreader.h"
#include "data/mediadata.h"
#include "decoders/video/abstractdecoder.h"

class CLAbstractRenderer;
class CLVideoStreamDisplay;
class CLAudioStreamDisplay;
struct CLCompressedVideoData;

/**
  * Stores CLVideoStreamDisplay for each channel/sensor
  */
class CLCamDisplay : public CLAbstractDataProcessor
{
    Q_OBJECT
public:
	CLCamDisplay(bool generateEndOfStreamSignal);
	~CLCamDisplay();

	void addVideoChannel(int index, CLAbstractRenderer* vw, bool can_downsacle);
	virtual bool processData(CLAbstractData* data);

	void pause();
	void resume();

	void setLightCPUMode(CLAbstractVideoDecoder::DecodeMode val);

	void display(CLCompressedVideoData* vd, bool sleep);
	void playAudio(bool play);
    void setSpeed(double speed);

    // schedule to clean up buffers all; 
    // schedule - coz I do not want to introduce mutexes
    //I assume that first incoming frame after jump is keyframe
    void jump(qint64 time); 

	//quint64 currentTime() const { return m_previousVideoDisplayedTime; }
    quint64 currentTime() const;

    void setMTDecoding(bool value);

    void setSingleShotMode(bool single);

    QImage getScreenshot(int channel);

public slots:
    void onRealTimeStreamHint(bool value);
    void onSlowSourceHint();
signals:
    void reachedTheEnd();
private:
	bool haveAudio() const;

	// puts in in queue and returns first in queue
	CLCompressedVideoData* nextInOutVideodata(CLCompressedVideoData* incoming, int channel);

    // this function doest not changes any quues; it just returns time of next frame been displayed
    quint64 nextVideoImageTime(CLCompressedVideoData* incoming, int channel) const;

    quint64 nextVideoImageTime(int channel) const;

    // this function returns diff between video and audio at any given moment
    qint64 diffBetweenVideoAndAudio(CLCompressedVideoData* incoming, int channel, qint64& duration);

	void clearVideoQueue();
    void enqueueVideo(CLCompressedVideoData* vd);
    void afterJump(qint64 new_time);

private:
	QQueue<CLCompressedVideoData*> m_videoQueue[CL_MAX_CHANNELS];

	CLVideoStreamDisplay* m_display[CL_MAX_CHANNELS];
	CLAudioStreamDisplay* m_audioDisplay;

	CLAdaptiveSleep m_delay;

	bool m_playAudioSet;
    double m_speed;
	bool m_playAudio;
    bool m_needChangePriority;

    /**
      * got at least one audio packet
      */
    bool m_hadAudio;

    qint64 m_lastAudioPacketTime;
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
    CLCodecAudioFormat m_playingFormat;
    int m_playingCompress;
    int m_playingBitrate;
    int m_tooSlowCounter;
    int m_storedMaxQueueSize;
    CLAbstractVideoDecoder::DecodeMode m_lightCpuMode;
    bool m_lastFrameDisplayed;
};

#endif //clcam_display_h_1211
