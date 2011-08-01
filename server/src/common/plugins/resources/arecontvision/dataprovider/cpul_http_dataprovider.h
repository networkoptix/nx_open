#ifndef cpull_httpreader_1119
#define cpull_httpreader_1119

#include "av_client_pull.h"

//single sensor HTTP reader
class AVClientPullSSHTTPStreamreader : public QnPlAVClinetPullStreamReader
{
public:
	explicit AVClientPullSSHTTPStreamreader(QnResource* dev);

	~AVClientPullSSHTTPStreamreader()
	{
		stop();
	}

protected:
	
    virtual QnAbstractDataPacketPtr getNextData() = 0;

protected:

	unsigned int m_port;
	unsigned int m_timeout;
	QAuthenticator m_auth;
	int m_model;

};

#endif //cpull_httpreader_1119
