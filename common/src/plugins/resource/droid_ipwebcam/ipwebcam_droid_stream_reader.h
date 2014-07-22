#ifndef QnPlDroidIpWebCamReader_14_04_h
#define QnPlDroidIpWebCamReader_14_04_h

#ifdef ENABLE_DROID

#include "core/dataprovider/spush_media_stream_provider.h"
#include "utils/network/simple_http_client.h"

class QnPlDroidIpWebCamReader : public CLServerPushStreamReader
{
    enum {BLOCK_SIZE = 1460};
public:
    QnPlDroidIpWebCamReader(const QnResourcePtr& res);
    virtual ~QnPlDroidIpWebCamReader();

protected:
    virtual QnAbstractMediaDataPtr getNextData();
    virtual CameraDiagnostics::Result openStream();
    virtual void closeStream() ;
    virtual bool isStreamOpened() const;

    void updateStreamParamsBasedOnQuality() override {};
    void updateStreamParamsBasedOnFps() override {};

private:
    CLSimpleHTTPClient* mHttpClient;

    char mData[BLOCK_SIZE];
    int mDataRemainedBeginIndex;

};

#endif // #ifdef ENABLE_DROID
#endif //QnPlDroidIpWebCamReader_14_04_h
