#ifndef cl_iqeye_onvif_serverpush1848
#define cl_iqeye_onvif_serverpush1848

#include "core/dataprovider/spush_media_stream_provider.h"
#include "utils/network/rtpsession.h"



class CLRtpStreamParser;

class RTPH264Streamreader: public CLServerPushStreamreader
{
private:
    enum {BLOCK_SIZE = 1460};
public:
	RTPH264Streamreader(QnResourcePtr res);
	virtual ~RTPH264Streamreader();

protected:
    virtual QnAbstractMediaDataPtr getNextData();
    virtual void openStream();
    virtual void closeStream() ;
    virtual bool isStreamOpened() const;
private:
    
    RTPSession m_RtpSession;
    RTPIODevice* m_rtpIo;
    CLRtpStreamParser* m_streamParser;

};

#endif //cl_android_clien_pul1721
