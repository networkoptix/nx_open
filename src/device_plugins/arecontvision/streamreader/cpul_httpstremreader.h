#ifndef cpull_httpreader_1119
#define cpull_httpreader_1119


#include "../../../streamreader/cpull_stremreader.h"


//single sensor HTTP reader
class AVClientPullSSHTTPStreamreader : public CLClientPullStreamreader
{
public:
	explicit AVClientPullSSHTTPStreamreader(CLDevice* dev);

	~AVClientPullSSHTTPStreamreader()
	{
		stop();
	}

protected:
	virtual CLAbstractMediaData* getNextData();
	virtual unsigned int getChannelNumber(){return 0;}; // single sensor camera has only one sensor
	
protected:

	QHostAddress m_ip;
	unsigned int m_port;
	unsigned int m_timeout;
	QAuthenticator m_auth;
	int m_model;


};

#endif //cpull_httpreader_1119