#ifndef cl_iqeye_264_serverpush1848
#define cl_iqeye_264_serverpush1848

#include "dataprovider/spush_streamdataprovider.h"
#include "network/rtpsession.h"



class CLRtpStreamParser;

class CLIQEyeH264treamreader: public CLServerPushStreamreader
{
private:
    enum {BLOCK_SIZE = 1460};
public:
	CLIQEyeH264treamreader(CLDevice* dev);
	virtual ~CLIQEyeH264treamreader();

protected:
    virtual CLAbstractMediaData* getNextData();
    virtual void openStream();
    virtual void closeStream() ;
    virtual bool isStreamOpened() const;
private:
    
    RTPSession m_RtpSession;
    RTPIODevice* m_rtpIo;
    CLRtpStreamParser* m_streamParser;

};

#endif //cl_android_clien_pul1721
