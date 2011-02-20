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

	void clearVideoQueue();

	void addVideoChannel(int index, CLAbstractRenderer* vw, bool can_downsacle);
	void processData(CLAbstractData* data);

	void pause();
	void resume();

	void setLightCPUMode(bool val);

	void coppyImage(bool copy);

	void display(CLCompressedVideoData* vd);
	void playAudio(bool play);

private:
	QQueue<CLCompressedVideoData*> m_videoQueue[CL_MAX_CHANNELS];

	CLVideoStreamDisplay* m_display[CL_MAX_CHANNELS];
	CLAudioStreamDisplay* m_audioDisplay;
	quint64 m_prev_time;
	CLAdaptiveSleep m_delay;

	bool m_playAudioSet;
	bool m_playAudio;

	// in mks
	qint64 m_audioClock;
	qint64 m_videoClock;

	qint64 m_audioDuration;
	qint64 m_videoDuration;
};

#endif //clcam_display_h_1211