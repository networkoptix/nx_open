#ifndef clcamera_h_1451
#define clcamera_h_1451


#include "../videodisplay/video_window.h"
#include "../videodisplay/camdisplay.h"
#include "../statistics/statistics.h"

class CLDevice;
class CLStreamreader;

class CLVideoCamera
{
public:
	// number of videovindows in array must be the same as device->getNumberOfVideoChannels
	CLVideoCamera(){}
	CLVideoCamera(CLDevice* device, CLVideoWindow* videovindow);
	virtual ~CLVideoCamera();

	void startDispay();
	void stopDispay();
	
	
	void setLightCPUMode(bool val);
	void coppyImage(bool copy);

	CLDevice* getDevice() const;
	CLStreamreader* getStreamreader();
	CLVideoWindow* getVideoWindow();
	CLStatistics* getStatistics();
	CLCamDisplay* getCamCamDisplay();

private:
	CLDevice* m_device;
	CLVideoWindow* m_videovindow;
	CLCamDisplay m_camdispay;
	CLStreamreader* m_reader;

	CLStatistics* m_stat;

};


#endif //clcamera_h_1451