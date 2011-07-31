#ifndef cl_av_clien_pull1529
#define cl_av_clien_pull1529


#include "dataprovider/cpull_media_stream_provider.h"

struct AVLastPacketSize
{
	int x0, y0, width, height;
};

class QnPlAVClinetPullStreamReader : public QnClientPullStreamProvider
{
public:
	QnPlAVClinetPullStreamReader(QnResourcePtr res);
	virtual ~QnPlAVClinetPullStreamReader();

	virtual void setQuality(QnStreamQuality q);

protected:
	
    virtual void updateStreamParamsBasedOnQuality(); 

	int getBitrate() const;
	bool isH264() const;

};

#endif //cl_av_clien_pull1529

