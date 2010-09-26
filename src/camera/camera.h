#ifndef clcamera_h_1451
#define clcamera_h_1451


#include "../videodisplay/camdisplay.h"
#include "../statistics/statistics.h"
#include "../streamreader/streamreader.h"

class CLDevice;
class CLVideoWindowItem;

class CLVideoCamera
{
public:
	// number of videovindows in array must be the same as device->getNumberOfVideoChannels
	CLVideoCamera(){}
	CLVideoCamera(CLDevice* device, CLVideoWindowItem* videovindow);
	virtual ~CLVideoCamera();

	void startDispay();
	void stopDispay();
	
	
	void setLightCPUMode(bool val);
	void coppyImage(bool copy);

	CLDevice* getDevice() const;

	CLStreamreader* getStreamreader();
	CLVideoWindowItem* getVideoWindow();
	const CLVideoWindowItem* getVideoWindow() const;

	CLStatistics* getStatistics();
	CLCamDisplay* getCamCamDisplay();


	void setQuality(CLStreamreader::StreamQuality q, bool increase);

private:
	CLDevice* m_device;
	CLVideoWindowItem* m_videovindow;
	CLCamDisplay m_camdispay;
	CLStreamreader* m_reader;

	CLStatistics* m_stat;

};


#endif //clcamera_h_1451