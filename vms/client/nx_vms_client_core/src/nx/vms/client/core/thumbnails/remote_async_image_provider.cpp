// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_async_image_provider.h"

#include <optional>

#include <QtCore/QCache>
#include <QtCore/QFuture>
#include <QtCore/QFutureWatcher>
#include <QtCore/QPromise>
#include <QtCore/QScopedPointerDeleteLater>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>

#include <nx/utils/log/assert.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/common/utils/thread_pool.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/core/thumbnails/generic_remote_image_request.h>

namespace nx::vms::client::core {

using ImageResult = std::tuple<QImage /*image*/, QString /*errorString*/>;
using namespace std::chrono;

namespace {

static const QString kSystemIdParameterName = "___systemId";
static const QString kExpirationParameterName = "___expirationSeconds";

struct InternalParams
{
    QString systemId;
    std::optional<seconds> expiration;
};

SystemContext* getSystemContext(const QString& crossSystemId = QString{})
{
    if (crossSystemId.isEmpty())
        return appContext()->currentSystemContext();

    const auto crossSystem = appContext()->cloudCrossSystemManager()->systemContext(crossSystemId);
    return crossSystem ? crossSystem->systemContext() : nullptr;
}

std::tuple<InternalParams, QString /*preprocessedLine*/> extractInternalParams(
    const QString& requestLine)
{
    QUrl url(requestLine);
    QUrlQuery query(url);

    bool expirable = false;
    const auto expiration = seconds(query.queryItemValue(kExpirationParameterName).toLongLong(
        &expirable));

    const InternalParams result{
        .systemId = query.queryItemValue(kSystemIdParameterName),
        .expiration = expirable ? std::make_optional(expiration) : std::nullopt};

    query.removeQueryItem(kSystemIdParameterName);
    query.removeQueryItem(kExpirationParameterName);
    url.setQuery(query);

    return {result, url.toString(QUrl::RemoveScheme | QUrl::RemoveAuthority)};
}

} // namespace

// ------------------------------------------------------------------------------------------------
// RemoteAsyncImageProvider::Response

class RemoteAsyncImageProvider::Response: public QQuickImageResponse
{
public:
    explicit Response(const QFuture<ImageResult>& future)
    {
        connect(m_watcher.get(), &QFutureWatcher<ImageResult>::finished,
            this, &QQuickImageResponse::finished, Qt::QueuedConnection);

        m_watcher->setFuture(future);
    }

    virtual QQuickTextureFactory* textureFactory() const override
    {
        const auto image = m_watcher && NX_ASSERT(m_watcher->isFinished())
            ? std::get<QImage>(m_watcher->result())
            : QImage{};

        return QQuickTextureFactory::textureFactoryForImage(image);
    }

    virtual QString errorString() const override
    {
        if (!m_watcher)
            return "Request canceled";

        return NX_ASSERT(m_watcher->isFinished())
            ? std::get<QString>(m_watcher->result())
            : QString{};
    }

    virtual void cancel()
    {
        m_watcher.reset();
        emit finished();
    }

private:
    std::unique_ptr<QFutureWatcher<ImageResult>> m_watcher{new QFutureWatcher<ImageResult>{}};
};

// ------------------------------------------------------------------------------------------------
// RemoteAsyncImageProvider::RequestCache

// Running and finished requests cache.
// Finished requests are stored with the cost of the result image size.
// Running requests are stored with the minimal cost of 1.
// Uses QCache internally; when the maximum cost is exceeded, the least recently used items are
// discarded.
// Optional expiration time can be specified when a request is added. Expiration is checked only
// at get attempts.
class RemoteAsyncImageProvider::RequestCache
{
    struct Item
    {
        QFuture<ImageResult> future;
        const QDeadlineTimer expiration;
    };

public:
    std::optional<QFuture<ImageResult>> get(const QString& key)
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        const auto itemPtr = m_cache[key];
        if (itemPtr && !itemPtr->expiration.hasExpired())
            return itemPtr->future;
        return {};
    }

    void add(const QString& key, const QFuture<ImageResult>& future,
        const std::optional<seconds>& expiration = {})
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        const auto expirationTimer = expiration
            ? QDeadlineTimer(*expiration)
            : QDeadlineTimer(QDeadlineTimer::Forever);
        m_cache.insert(key, new Item{QFuture<ImageResult>(future), expirationTimer}, cost(future));
    }

    void remove(const QString& key)
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        m_cache.remove(key);
    }

private:
    static int cost(const ImageResult& result)
    {
        const QSize size = std::get<QImage>(result).size();
        return size.isEmpty() ? 1 : (size.width() * size.height());
    }

    static int cost(const QFuture<ImageResult>& future)
    {
        // Running requests have the minimal cost of 1.
        return (future.isFinished() && NX_ASSERT(future.isValid())) ? cost(future.result()) : 1;
    }

private:
    nx::Mutex m_mutex;

    // An item cost is equal to its image size.
    // The maximum cost is for about 1000 320x180 images (~230 MB).
    QCache<QString, Item> m_cache{320 * 180 * 1000};
};

// ------------------------------------------------------------------------------------------------
// RemoteAsyncImageProvider

RemoteAsyncImageProvider::RemoteAsyncImageProvider():
    m_cache(std::make_shared<RequestCache>())
{
}

QQuickImageResponse* RemoteAsyncImageProvider::requestImageResponse(
    const QString& requestLine, const QSize& /*requestedSize*/)
{
    // `requestedSize` is ignored.
    // If a particular request allows to obtain images of different sizes, the desired size must be
    // included in `requestLine`.

    if (auto future = m_cache->get(requestLine))
        return new Response(*future);

    const auto [internalParams, preprocessedLine] = extractInternalParams(requestLine);

    auto future = AsyncImageResult::toFuture(std::make_unique<GenericRemoteImageRequest>(
        getSystemContext(internalParams.systemId), preprocessedLine));

    // Insert a running request to the cache (internally will be inserted with a cost of 1).
    m_cache->add(requestLine, future, internalParams.expiration);

    return new Response(future.then(
        [requestLine, future, expiration = internalParams.expiration, cacheGuard =
            std::weak_ptr<RequestCache>(m_cache)](const ImageResult& result) -> ImageResult
        {
            const auto cache = cacheGuard.lock();
            if (!cache)
                return result;

            if (std::get<QImage>(result).isNull())
            {
                // Remove the request from the cache in case of error.
                cache->remove(requestLine);
            }
            else
            {
                // Re-insert finished request to the cache (to update its cost internally).
                cache->add(requestLine, future, expiration);
            }

            return result;
        }));
}

QString RemoteAsyncImageProvider::systemIdParameterName()
{
    return kSystemIdParameterName;
}

QString RemoteAsyncImageProvider::expirationParameterName()
{
    return kExpirationParameterName;
}

} // namespace nx::vms::client::core
