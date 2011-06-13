#ifndef cl_iqeye_264_serverpush1848
#define cl_iqeye_264_serverpush1848

#include "streamreader/spush_streamreader.h"
#include "network/simple_http_client.h"
#include "network/rtpsession.h"



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
    QIODevice* m_rtpIo;

};

#endif //cl_android_clien_pul1721
