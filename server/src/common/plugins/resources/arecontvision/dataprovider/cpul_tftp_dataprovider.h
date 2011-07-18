#ifndef cpull_httpreader_1119
#define cpull_httpreader_1119

#include "av_client_pull.h"

class QnAbstractMediaDataPacket;

//single sensor TFTP reader
// arecont vision TFTP stack is faster (for JPEG image; for H.264 they are almost equal )( you can get more fps with it )
// so if it's possible to use TFP ( local networks ) it's btter be TFTP but HTTP
class AVClientPullSSTFTPStreamreader : public QnPlAVClinetPullStreamReader
{
public:
	explicit AVClientPullSSTFTPStreamreader (QnResource* dev );

	~AVClientPullSSTFTPStreamreader()
	{
		stop();
	}

protected:
	virtual QnAbstractMediaDataPacketPtr getNextData();

protected:

	int m_last_width;
	int m_last_height;

	int m_last_cam_width;
	int m_last_cam_height;

	bool m_last_resolution;

	unsigned int m_timeout;
	int m_model;

	bool m_black_white; // for dual sensor only 

};

#endif //cpull_httpreader_1119
