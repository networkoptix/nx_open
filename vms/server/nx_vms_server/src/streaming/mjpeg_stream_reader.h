#pragma once
#if defined(ENABLE_DATA_PROVIDERS)

#include <memory>

#include <providers/spush_media_stream_provider.h>
#include <nx/network/deprecated/simple_http_client.h>

class MJPEGStreamReader: public CLServerPushStreamReader
{
public:
    MJPEGStreamReader(
        const nx::vms::server::resource::CameraPtr& res,
        const QString& streamHttpPath);
    virtual ~MJPEGStreamReader();

protected:
    //!Implementation of QnAbstractMediaStreamProvider::getNextData
    virtual QnAbstractMediaDataPtr getNextData() override;
    //!Implementation of QnAbstractMediaStreamProvider::openStream
    virtual CameraDiagnostics::Result openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& params) override;
    //!Implementation of QnAbstractMediaStreamProvider::closeStream
    virtual void closeStream() override;
    //!Implementation of QnAbstractMediaStreamProvider::isStreamOpened
    virtual bool isStreamOpened() const override;

    virtual void pleaseReopenStream() override {};

private:
    std::unique_ptr<CLSimpleHTTPClient> mHttpClient;

    QString m_request;
    nx::vms::server::resource::CameraPtr m_camera;
};

#endif // defined(ENABLE_DATA_PROVIDERS)
