#ifndef clcam_display_h_1211
#define clcam_display_h_1211

#include "../data/dataprocessor.h"
#include "base/adaptivesleep.h"
#include "device/device_video_layout.h"

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
	void processData(CLAbstractData* data);

	void pause();
	void resume();

	void setLightCPUMode(bool val);

	void copyImage(bool copy);

	void display(CLCompressedVideoData* vd, bool sleep);
	void playAudio(bool play);

    // schedule to clean up buffers all; 
    // schedule - coz I do not want to introduce mutexes
    //I assume that first incoming frame after jump is keyframe
    void jump(); 

	quint64 currentTime() const { return m_previousVideoDisplayedTime; }

signals:
    void reachedTheEnd();

private:
	bool haveAudio() const;

	// puts in in queue and returns first in queue
	CLCompressedVideoData* nextInOutVideodata(CLCompressedVideoData* incoming, int channel);

    // this function doest not changes any quues; it just returns time of next frame been displayed
    quint64 nextVideoImageTime(CLCompressedVideoData* incoming, int channel) const;

    // this function returns diff between video and audio at any given moment
    qint64 diffBetweenVideoAndAudio(CLCompressedVideoData* incoming, int channel, qint64& duration) const;

	void clearVideoQueue();
    void enqueueVideo(CLCompressedVideoData* vd);
    void afterJump(qint64 new_time);
private:
	QQueue<CLCompressedVideoData*> m_videoQueue[CL_MAX_CHANNELS];

	CLVideoStreamDisplay* m_display[CL_MAX_CHANNELS];
	CLAudioStreamDisplay* m_audioDisplay;

	CLAdaptiveSleep m_delay;

	bool m_playAudioSet;
	bool m_playAudio;

    /**
      * got at least one audio packet
      */
    bool m_hadAudio;

	quint64 m_lastAudioPacketTime;
    quint64 m_lastVideoPacketTime;

    quint64 m_previousVideoTime;
    quint64 m_previousVideoDisplayedTime;

    bool m_afterJump;

    int m_displayLasts;

    bool m_ignoringVideo;

    bool mGenerateEndOfStreamSignal;
};

#endif //clcam_display_h_1211
