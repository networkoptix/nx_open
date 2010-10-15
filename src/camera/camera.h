#ifndef clcamera_h_1451
#define clcamera_h_1451


#include "camdisplay.h"
#include "../statistics/statistics.h"
#include "../streamreader/streamreader.h"
#include "videodisplay/complicated_item.h"

class CLDevice;
class CLVideoWindowItem;
class CLAbstractSceneItem;

class CLVideoCamera : public CLAbstractComplicatedItem
{
public:
	// number of videovindows in array must be the same as device->getNumberOfVideoChannels
	CLVideoCamera(){}
	CLVideoCamera(CLDevice* device, CLVideoWindowItem* videovindow);
	virtual ~CLVideoCamera();

	virtual void startDispay();
	virtual void stopDispay();

	virtual void beforestopDispay();
	
	
	void setLightCPUMode(bool val);
	void coppyImage(bool copy);

	virtual CLDevice* getDevice() const;

	CLStreamreader* getStreamreader();
	CLVideoWindowItem* getVideoWindow();
	const CLVideoWindowItem* getVideoWindow() const;

	virtual CLAbstractSceneItem* getSceneItem() const;

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