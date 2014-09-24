#ifndef cl_mjpeg_dataprovider_h_1140
#define cl_mjpeg_dataprovider_h_1140

#ifdef ENABLE_DATA_PROVIDERS

#include <memory>

#include "core/dataprovider/spush_media_stream_provider.h"
#include "utils/network/simple_http_client.h"


class MJPEGStreamReader: public CLServerPushStreamReader
{
public:
    MJPEGStreamReader(const QnResourcePtr& res, const QString& streamHttpPath);
    virtual ~MJPEGStreamReader();

protected:
    //!Implementation of QnAbstractMediaStreamProvider::getNextData
    virtual QnAbstractMediaDataPtr getNextData() override;
    //!Implementation of QnAbstractMediaStreamProvider::openStream
    virtual CameraDiagnostics::Result openStream() override;
    //!Implementation of QnAbstractMediaStreamProvider::closeStream
    virtual void closeStream() override;
    //!Implementation of QnAbstractMediaStreamProvider::isStreamOpened
    virtual bool isStreamOpened() const override;

    void updateStreamParamsBasedOnQuality() override {};
    void updateStreamParamsBasedOnFps() override {};

private:
    std::unique_ptr<CLSimpleHTTPClient> mHttpClient;

    QString m_request;
};

#endif // ENABLE_DATA_PROVIDERS

#endif //cl_mjpeg_dataprovider_h_1140
