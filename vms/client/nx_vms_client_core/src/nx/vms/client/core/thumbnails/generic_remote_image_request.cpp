// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "generic_remote_image_request.h"

#include <QtConcurrent/QtConcurrentRun>
#include <QtCore/QFutureWatcher>

#include <api/server_rest_connection.h>
#include <nx/network/http/custom_headers.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/url.h>
#include <nx/vms/client/core/common/utils/thread_pool.h>
#include <nx/vms/client/core/system_context.h>

namespace nx::vms::client::core {

struct GenericRemoteImageRequest::Private
{
    QFutureWatcher<QImage> imageWatcher{};
    rest::Handle requestId{};

    // Used for asynchronous decoding of a received image.
    QThreadPool* threadPool() const
    {
        return ThreadPool::instance("GenericRemoteImageRequest_thread_pool");
    }
};

// ------------------------------------------------------------------------------------------------
// GenericRemoteImageRequest

GenericRemoteImageRequest::GenericRemoteImageRequest(
    SystemContext* systemContext,
    const QString& requestLine,
    QObject* parent)
    :
    base_type(parent),
    d(new Private{})
{
    if (!NX_ASSERT(!requestLine.isEmpty())
        || !NX_ASSERT(systemContext)
        || !NX_ASSERT(d->threadPool()))
    {
        setError("Invalid operation");
        return;
    }

    const auto api = systemContext->connectedServerApi();
    if (!api)
    {
        setError("No server connection");
        return;
    }

    connect(&d->imageWatcher, &QFutureWatcherBase::finished, this,
        [this]() { setImage(d->imageWatcher.future().result()); });

    const auto callback = nx::utils::guarded(this,
        [this](
            bool success,
            rest::Handle requestId,
            QByteArray imageData,
            const nx::network::http::HttpHeaders& headers)
        {
            if (!NX_ASSERT(requestId == d->requestId))
                return;

            const bool isJsonError = nx::network::http::getHeaderValue(headers, "Content-Type")
                == Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json);

            if (!success || imageData.isEmpty() || isJsonError || !d->threadPool())
            {
                setError(QString::fromUtf8(imageData));
                return;
            }

            d->requestId = {};

            const nx::String timestampUs = nx::network::http::getHeaderValue(headers,
                Qn::FRAME_TIMESTAMP_US_HEADER_NAME);

            // Decompress image in another thread.
            d->imageWatcher.setFuture(QtConcurrent::run(d->threadPool(),
                [imageData, timestampUs]() -> QImage
                {
                    QImage image;
                    image.loadFromData(imageData);
                    image.setText(kTimestampUsKey, timestampUs);
                    return image;
                }));
        });

    NX_VERBOSE(this, "Requesting an image (%1)", requestLine);

    nx::utils::Url url(requestLine);

    d->requestId = api->getRawResult(url.toString(QUrl::PrettyDecoded | QUrl::RemoveQuery),
        nx::network::rest::Params::fromList(url.queryItems()),
        callback,
        thread());

    if (d->requestId == rest::Handle())
        setError("Failed to send request");
};

GenericRemoteImageRequest::~GenericRemoteImageRequest()
{
    // Required here for forward-declared scoped pointer destruction.
}

} // namespace nx::vms::client::core
