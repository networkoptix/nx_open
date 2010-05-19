#ifndef cpull_httpreader_1119
#define cpull_httpreader_1119


#include "../../../streamreader/cpull_stremreader.h"
#include "../data/mediadata.h"


//single sensor TFTP reader
// arecont vision TFTP stack is faster (for JPEG image; for H.264 they are almost equal )( you can get more fps with it )
// so if it's possible to use TFP ( local networks ) it's btter be TFTP but HTTP
class AVClientPullSSTFTPStreamreader : public CLClientPullStreamreader
{
public:
	explicit AVClientPullSSTFTPStreamreader (CLDevice* dev );

	~AVClientPullSSTFTPStreamreader()
	{
		stop();
	}

protected:
	virtual CLAbstractMediaData* getNextData();
	virtual unsigned int getChannelNumber(){return 0;}; // single sensor camera has only one sensor
	
protected:

	int m_last_width;
	int m_last_height;
	bool m_last_resolution;

	QHostAddress m_ip;
	unsigned int m_timeout;
	int m_model;


};

#endif //cpull_httpreader_1119