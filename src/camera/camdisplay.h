#ifndef clcam_display_h_1211
#define clcam_display_h_1211


#include "../data/dataprocessor.h"
#include "base/adaptivesleep.h"
#include "device/device_video_layout.h"

class CLAbstractRenderer;
class CLVideoStreamDisplay;
class CLAudioStreamDisplay;

// stores CLVideoStreamDisplay for each channel/sensor
class CLCamDisplay : public CLAbstractDataProcessor
{
public:
	CLCamDisplay();
	~CLCamDisplay();
	void addVideoChannel(int index, CLAbstractRenderer* vw, bool can_downsacle);
	void processData(CLAbstractData* data);

	void setLightCPUMode(bool val);

	void coppyImage(bool copy);

	void playAudio(bool play);

private:
	CLVideoStreamDisplay* m_display[CL_MAX_CHANNELS];
	CLAudioStreamDisplay* m_aydio_display;
	quint64 m_prev_time;
	CLAdaptiveSleep m_delay;
	bool m_palyaudio;
	

};

#endif //clcam_display_h_1211