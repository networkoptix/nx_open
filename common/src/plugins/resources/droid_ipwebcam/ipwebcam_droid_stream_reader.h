#ifndef QnPlDroidIpWebCamReader_14_04_h
#define QnPlDroidIpWebCamReader_14_04_h

#include "core/dataprovider/spush_media_stream_provider.h"
#include "core/dataprovider/live_stream_provider.h"
#include "utils/network/simple_http_client.h"

class QnPlDroidIpWebCamReader : public CLServerPushStreamreader , public QnLiveStreamProvider
{
    enum {BLOCK_SIZE = 1460};
public:
    QnPlDroidIpWebCamReader(QnResourcePtr res);
    virtual ~QnPlDroidIpWebCamReader();

protected:
    virtual QnAbstractMediaDataPtr getNextData();
    virtual void openStream();
    virtual void closeStream() ;
    virtual bool isStreamOpened() const;

    void updateStreamParamsBasedOnQuality() override {};
    void updateStreamParamsBasedOnFps() override {};

private:
    CLSimpleHTTPClient* mHttpClient;

    char mData[BLOCK_SIZE];
    int mDataRemainedBeginIndex;

};

#endif //QnPlDroidIpWebCamReader_14_04_h