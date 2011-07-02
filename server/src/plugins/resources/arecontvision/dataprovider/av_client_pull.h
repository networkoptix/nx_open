#ifndef cl_av_clien_pull1529
#define cl_av_clien_pull1529

#include "dataprovider/cpull_streamdataprovider.h"




struct AVLastPacketSize
{
	int x0, y0, width, height;
};

class CLAVClinetPullStreamReader : public CLClientPullStreamreader
{
public:
	CLAVClinetPullStreamReader(CLDevice* dev );
	virtual ~CLAVClinetPullStreamReader();

	virtual void setQuality(StreamQuality q);

protected:
	int getQuality() const;
	int getBitrate() const;
	bool isH264() const;

};

#endif //cl_av_clien_pull1529

