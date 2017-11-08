#ifndef QnPlDroidIpWebCamReader_14_04_h
#define QnPlDroidIpWebCamReader_14_04_h

#ifdef ENABLE_DROID

#include <providers/spush_media_stream_provider.h>
#include <nx/network/simple_http_client.h>

class QnPlDroidIpWebCamReader : public CLServerPushStreamReader
{
    enum {BLOCK_SIZE = 1460};
public:
    QnPlDroidIpWebCamReader(const QnResourcePtr& res);
    virtual ~QnPlDroidIpWebCamReader();

protected:
    virtual QnAbstractMediaDataPtr getNextData();
    virtual CameraDiagnostics::Result openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& params) override;
    virtual void closeStream() ;
    virtual bool isStreamOpened() const;

private:
    CLSimpleHTTPClient* mHttpClient;

    char mData[BLOCK_SIZE];
    int mDataRemainedBeginIndex;

};

#endif // #ifdef ENABLE_DROID
#endif //QnPlDroidIpWebCamReader_14_04_h
