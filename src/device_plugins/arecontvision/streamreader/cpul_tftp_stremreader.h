#ifndef cpull_httpreader_1119
#define cpull_httpreader_1119


#include "av_client_pull.h"
#include "../data/mediadata.h"


//single sensor TFTP reader
// arecont vision TFTP stack is faster (for JPEG image; for H.264 they are almost equal )( you can get more fps with it )
// so if it's possible to use TFP ( local networks ) it's btter be TFTP but HTTP
class AVClientPullSSTFTPStreamreader : public CLAVClinetPullStreamReader
{
public:
	explicit AVClientPullSSTFTPStreamreader (CLDevice* dev );

	~AVClientPullSSTFTPStreamreader()
	{
		stop();
	}

protected:
	virtual CLAbstractMediaData* getNextData();
	
	
protected:

	int m_last_width;
	int m_last_height;

	int m_last_cam_width;
	int m_last_cam_height;


	bool m_last_resolution;


	unsigned int m_timeout;
	int m_model;


};

#endif //cpull_httpreader_1119