#ifndef cl_android_clien_pul1721
#define cl_android_clien_pul1721

#include "streamreader/spush_streamreader.h"
#include "network/simple_http_client.h"



class CLAndroidStreamreader: public CLServerPushStreamreader
{
private:
    enum {BLOCK_SIZE = 1460};
public:
	CLAndroidStreamreader(CLDevice* dev);
	virtual ~CLAndroidStreamreader();

protected:
    virtual CLAbstractMediaData* getNextData();
    virtual void openStream();
    virtual void closeStream() ;
    virtual bool isStreamOpened() const;
private:
    CLSimpleHTTPClient* mHttpClient;

    char mData[BLOCK_SIZE];
    int mDataRemainedBeginIndex;


};

#endif //cl_android_clien_pul1721
