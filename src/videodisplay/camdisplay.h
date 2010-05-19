#ifndef clcam_display_h_1211
#define clcam_display_h_1211


#include "../data/dataprocessor.h"
#include "videostreamdisplay.h"
#include "abstractrenderer.h"
#include "../device/device_video_layout.h"

// stores CLVideoStreamDisplay for each channel/sensor
class CLCamDisplay : public CLAbstractDataProcessor
{
public:
	CLCamDisplay();
	~CLCamDisplay();
	void addVideoChannel(int index, CLAbstractRenderer* vw);
	void processData(CLAbstractData* data);

	void setLightCPUMode(bool val);

	void coppyImage(bool copy);

private:
	CLVideoStreamDisplay* m_display[CL_MAX_CHANNELS];

	

};

#endif //clcam_display_h_1211