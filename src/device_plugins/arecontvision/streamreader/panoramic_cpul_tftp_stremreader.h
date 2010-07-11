#ifndef panoramic_cpull_httpreader_1800
#define panoramic_cpull_httpreader_1800


#include "av_client_pull.h"
#include "../data/mediadata.h"


//single sensor TFTP reader
// arecont vision TFTP stack is faster (for JPEG image; for H.264 they are almost equal )( you can get more fps with it )
// so if it's possible to use TFP ( local networks ) it's btter be TFTP but HTTP
class AVPanoramicClientPullSSTFTPStreamreader : public CLAVClinetPullStreamReader
{
public:
	explicit AVPanoramicClientPullSSTFTPStreamreader  (CLDevice* dev );

	~AVPanoramicClientPullSSTFTPStreamreader()
	{
		stop();
	}

protected:
	virtual CLAbstractMediaData* getNextData();
	
	
protected:

	int m_last_width;
	int m_last_height;
	bool m_last_resolution;

	QHostAddress m_ip;
	unsigned int m_timeout;
	int m_model;


};

#endif //cpull_httpreader_1119