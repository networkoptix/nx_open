#ifndef cl_android_clien_pul1721
#define cl_android_clien_pul1721

#include "dataprovider/spush_streamdataprovider.h"

class CLSimpleHTTPClient;

class CLAndroidStreamreader: public QnServerPushDataProvider
{
private:
    enum {BLOCK_SIZE = 1460};
public:
	CLAndroidStreamreader(QnResource* dev);
	virtual ~CLAndroidStreamreader();

protected:
    virtual QnAbstractMediaDataPacketPtr getNextData();
    virtual void openStream();
    virtual void closeStream() ;
    virtual bool isStreamOpened() const;
private:
    CLSimpleHTTPClient* mHttpClient;

    char mData[BLOCK_SIZE];
    int mDataRemainedBeginIndex;


};

#endif //cl_android_clien_pul1721
