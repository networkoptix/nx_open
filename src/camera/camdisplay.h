#ifndef clcam_display_h_1211
#define clcam_display_h_1211


#include "../data/dataprocessor.h"
#include "base/adaptivesleep.h"
#include "device/device_video_layout.h"

class CLAbstractRenderer;
class CLVideoStreamDisplay;
class CLAudioStreamDisplay;
class CLCompressedVideoData;

// stores CLVideoStreamDisplay for each channel/sensor
class CLCamDisplay : public CLAbstractDataProcessor
{
public:
	CLCamDisplay();
	~CLCamDisplay();

	void addVideoChannel(int index, CLAbstractRenderer* vw, bool can_downsacle);
	void processData(CLAbstractData* data);

	void pause();
	void resume();

	void setLightCPUMode(bool val);

	void coppyImage(bool copy);

	void display(CLCompressedVideoData* vd, bool sleep);
	void playAudio(bool play);


    // schedule to clean up buffers all; 
    // schedule - coz I do not want to introduce mutexes
    //I assume that first incoming frame after jump is keyframe
    void jump(); 

private:
	bool haveAudio() const;

	// puts in in queue and returns first in queue
	CLCompressedVideoData* nextInOutVideodata(CLCompressedVideoData* incoming, int channel);

    // this function doest not changes any quues; it just returns time of next frame been displayed
    quint64 nextVideoImageTime(CLCompressedVideoData* incoming, int channel) const;


    // this function returns diff between video and audio at any given moment
    qint64 diffBetweenVideoandAudio(CLCompressedVideoData* incoming, int channel, qint64& duration) const;


	void clearVideoQueue();

    void enqueueVideo(CLCompressedVideoData* vd);

    void after_jump(qint64 new_time);

private:
	QQueue<CLCompressedVideoData*> m_videoQueue[CL_MAX_CHANNELS];

	CLVideoStreamDisplay* m_display[CL_MAX_CHANNELS];
	CLAudioStreamDisplay* m_audioDisplay;
	
	CLAdaptiveSleep m_delay;

	bool m_playAudioSet;
	bool m_playAudio;

    bool m_hadAudio; // got at leat one audio packet

	quint64 m_last_audio_packet_time;
    quint64 m_last_video_packet_time;

    quint64 m_prev_video_displayed_time;

    bool m_after_jump;


    int m_display_lasts;
};

#endif //clcam_display_h_1211