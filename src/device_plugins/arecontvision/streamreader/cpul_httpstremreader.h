#ifndef cpull_httpreader_1119
#define cpull_httpreader_1119

#include "av_client_pull.h"

//single sensor HTTP reader
class AVClientPullSSHTTPStreamreader : public CLAVClinetPullStreamReader
{
public:
	explicit AVClientPullSSHTTPStreamreader(CLDevice* dev);

	~AVClientPullSSHTTPStreamreader()
	{
		stop();
	}

protected:
	virtual CLAbstractMediaData* getNextData();

protected:

	unsigned int m_port;
	unsigned int m_timeout;
	QAuthenticator m_auth;
	int m_model;

};

#endif //cpull_httpreader_1119