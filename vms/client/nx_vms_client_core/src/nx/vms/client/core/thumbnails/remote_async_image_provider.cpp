// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_async_image_provider.h"

#include <QtCore/QPointer>

#include <nx/vms/client/core/common/utils/thread_pool.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/thumbnails/generic_remote_image_request.h>

namespace nx::vms::client::core {

class RemoteAsyncImageResponse: public QQuickImageResponse
{
public:
    explicit RemoteAsyncImageResponse(
        SystemContext* context,
        const QString& id)
        :
        m_request(new GenericRemoteImageRequest(context, id))
    {
        if (m_request->isReady())
        {
            QMetaObject::invokeMethod(this, &QQuickImageResponse::finished, Qt::QueuedConnection);
        }
        else
        {
            connect(m_request.get(), &AsyncImageResult::ready,
                this, &QQuickImageResponse::finished);
        }
    }

    virtual QQuickTextureFactory* textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(
            m_request ? m_request->image() : QImage());
    }

    virtual QString errorString() const override
    {
        return m_request ? m_request->errorString() : "Request cancelled";
    }

    virtual void cancel()
    {
        m_request.reset();
        emit finished();
    }

private:
    std::unique_ptr<GenericRemoteImageRequest> m_request;
};

RemoteAsyncImageProvider::RemoteAsyncImageProvider(SystemContext* systemContext):
    SystemContextAware(systemContext)
{
}

QQuickImageResponse* RemoteAsyncImageProvider::requestImageResponse(
    const QString& requestLine, const QSize& /*requestedSize*/)
{
    // `requestedSize` is ignored.
    // If a particular request allows to obtain images of different sizes, the desired size must be
    // included in `requestLine`.

    return new RemoteAsyncImageResponse(systemContext(), requestLine);
}

} // namespace nx::vms::client::core
