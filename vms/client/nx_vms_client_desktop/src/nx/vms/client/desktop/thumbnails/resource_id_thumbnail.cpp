// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_id_thumbnail.h"

#include <chrono>
#include <functional>
#include <memory>

#include <QtQml/QtQml>
#include <QtQuick/private/qquickpixmapcache_p.h>

#include <common/common_globals.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/vms/client/core/thumbnails/camera_async_image_request.h>
#include <nx/vms/client/core/thumbnails/immediate_image_result.h>
#include <nx/vms/client/core/thumbnails/local_media_async_image_request.h>
#include <nx/vms/client/core/thumbnails/proxy_image_result.h>
#include <nx/vms/client/core/thumbnails/thumbnail_cache.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/utils/volatile_unique_ptr.h>
#include <nx/vms/client/desktop/thumbnails/fallback_image_result.h>
#include <utils/common/delayed.h>

namespace nx::vms::client::desktop {

using namespace std::chrono;
using namespace nx::vms::client::core;

// ------------------------------------------------------------------------------------------------
// ResourceIdentificationThumbnail::Private

struct ResourceIdentificationThumbnail::Private
{
    ResourceIdentificationThumbnail* const q;
    bool objectComplete = true;

    milliseconds preloadDelay = 0ms;
    seconds refreshInterval = 10s;
    milliseconds nextRefreshInterval = refreshInterval;
    qreal requestScatter = 0.0;
    std::unique_ptr<QTimer> refreshTimer{new QTimer()};
    VolatileUniquePtr<QTimer, QScopedPointerDeleteLater> preloadTimer;
    nx::utils::ElapsedTimer elapsed;
    bool online = false;
    bool enforced = false;
    QnVirtualCameraResourcePtr camera;
    QImage fallbackImage;

    static ThumbnailCache* cache()
    {
        static ThumbnailCache cache;
        return &cache;
    }

    void setOnline(bool value)
    {
        if (online == value)
            return;

        online = value;
        updateRefreshTimer();

        if (!preloadTimer)
            q->update();
    }

    void setRefreshActive(bool value)
    {
        if (refreshTimer->isActive() == value)
            return;

        if (value)
            refreshTimer->start();
        else
            refreshTimer->stop();
    }

    void updateRefreshTimer()
    {
        setRefreshActive(online
            && refreshInterval > 0ms
            && (q->status() == Status::ready || q->status() == Status::unavailable)
            && q->active()
            && q->resource());
    }

    milliseconds applyScatterToInterval(milliseconds interval) const
    {
        if (qFuzzyIsNull(requestScatter))
            return interval;

        const qreal baseIntervalMs = milliseconds(interval).count();

        const qreal actualIntervalMs = baseIntervalMs
            * (1.0 + requestScatter * (((qreal) rand() / RAND_MAX) - 0.5));

        return milliseconds(qRound(actualIntervalMs));
    }

    void updateNextRefreshInterval()
    {
        nextRefreshInterval = applyScatterToInterval(refreshInterval);
    }

    milliseconds calculateTimerResolution(seconds updateInterval)
    {
        if (updateInterval > 1min)
            return 10s;

        if (updateInterval > 10s)
            return 1s;

        if (updateInterval > 1s)
            return 200ms;

        return 0ms;
    }

    void preloadDelayed()
    {
        preloadTimer.reset(executeDelayedParented(
            [q = q]() { q->update(); },
            applyScatterToInterval(preloadDelay),
            q));
    }

    bool isLicensedCamera() const
    {
        return camera && camera->isScheduleEnabled();
    }

    // This class ensures that a camera thumbnail request is not sent again
    // if the same request is being processed.
    class CameraRequestManager: public QObject
    {
        using Key = QPair<QnVirtualCameraResourcePtr, int>;
        QHash<Key, std::shared_ptr<CameraAsyncImageRequest>> m_requests;

    public:
        std::unique_ptr<AsyncImageResult> request(
            const QnVirtualCameraResourcePtr& camera, int maximumSize)
        {
            if (!NX_ASSERT(camera && camera->resourcePool()))
                return {};

            const auto key = Key(camera, maximumSize);
            if (m_requests.contains(key))
                return std::make_unique<ProxyImageResult>(m_requests.value(key));

            const auto request = std::make_shared<CameraAsyncImageRequest>(camera, maximumSize);
            if (request->isReady())
                return std::make_unique<ProxyImageResult>(request);

            const std::function<void()> onReady = nx::utils::guarded(this,
                [this, key]()
                {
                    m_requests.remove(key);
                    NX_VERBOSE(this, "Camera requests count: %1", m_requests.size());
                });

            connect(request.get(), &AsyncImageResult::ready, this, onReady);

            connect(camera->resourcePool(), &QnResourcePool::resourcesRemoved,
                this, &CameraRequestManager::handleResourcesRemoved, Qt::UniqueConnection);

            m_requests[key] = request;
            NX_VERBOSE(this, "Camera requests count: %1", m_requests.size());

            return std::make_unique<ProxyImageResult>(request);
        }

    private:
        void handleResourcesRemoved(const QnResourceList& resources)
        {
            QSet<QnVirtualCameraResourcePtr> removedCameras;
            for (const auto& resource: resources)
            {
                if (auto cameraResource = resource.dynamicCast<QnVirtualCameraResource>())
                    removedCameras.insert(cameraResource);
            }

            if (removedCameras.empty())
                return;

            for (auto it = m_requests.begin(); it != m_requests.end(); )
            {
                if (removedCameras.contains(it.key().first))
                    it = m_requests.erase(it);
                else
                    ++it;
            }
        }
    };

    static CameraRequestManager* cameraRequestManager()
    {
        static CameraRequestManager instance;
        return &instance;
    }
};

// ------------------------------------------------------------------------------------------------
// ResourceIdentificationThumbnail

ResourceIdentificationThumbnail::ResourceIdentificationThumbnail(QObject* parent):
    base_type(Private::cache(), parent),
    d(new Private{this})
{
    setMaximumSize(320); //< Default.
    setLoadedAutomatically(false);
    d->updateNextRefreshInterval();

    d->refreshTimer->callOnTimeout(this,
        [this]()
        {
            if (!d->elapsed.isValid() || !d->elapsed.hasExpired(d->nextRefreshInterval))
                return;

            if (status() != Status::ready && status() != Status::unavailable)
                return;

            d->updateNextRefreshInterval();
            d->elapsed.restart();
            update();
        });

    connect(this, &AbstractResourceThumbnail::updated, this,
        [this]()
        {
            d->updateNextRefreshInterval();
            d->preloadTimer.reset();
            d->elapsed.restart();
        });

    connect(this, &AbstractResourceThumbnail::activeChanged, this,
        [this]() { d->updateRefreshTimer(); });

    connect(this, &AbstractResourceThumbnail::statusChanged, this,
        [this]()
        {
            if (status() == Status::ready || status() == Status::unavailable)
                d->elapsed.restart();

            d->updateRefreshTimer();
        });

    connect(this, &AbstractResourceThumbnail::resourceChanged, this,
        [this]()
        {
            d->online = resource() && resource()->isOnline();
            d->camera = resource().objectCast<QnVirtualCameraResource>();
            d->fallbackImage = {};
            d->elapsed.invalidate();
            d->preloadTimer.reset();
            d->updateRefreshTimer();

            if (!resource())
                return;

            // No need to bother with disconnecting (see AbstractResourceThumbnail::setResource).
            connect(resource().get(), &QnResource::statusChanged, this,
                [this](const QnResourcePtr& resource) { d->setOnline(resource->isOnline()); });

            if (d->objectComplete)
                d->preloadDelayed();
        });
}

ResourceIdentificationThumbnail::~ResourceIdentificationThumbnail()
{
    // Required here for forward-declared scoped pointer destruction.
}

seconds ResourceIdentificationThumbnail::refreshInterval() const
{
    return d->refreshInterval;
}

void ResourceIdentificationThumbnail::setRefreshInterval(seconds value)
{
    if (d->refreshInterval == value)
        return;

    d->refreshInterval = value;
    d->refreshTimer->setInterval(d->calculateTimerResolution(d->refreshInterval));
    d->updateRefreshTimer();
    emit refreshIntervalChanged();
}

qreal ResourceIdentificationThumbnail::requestScatter() const
{
    return d->requestScatter;
}

void ResourceIdentificationThumbnail::setRequestScatter(qreal value)
{
    const auto allowedValue = qBound(0.0, value, 1.0);

    if (qFuzzyEquals(d->requestScatter, allowedValue))
        return;

    d->requestScatter = allowedValue;
    emit requestScatterChanged();

    d->updateNextRefreshInterval();

    if (d->preloadTimer)
        d->preloadDelayed(); //< Restart preload if needed.
}

std::chrono::milliseconds ResourceIdentificationThumbnail::preloadDelay() const
{
    return d->preloadDelay;
}

void ResourceIdentificationThumbnail::setPreloadDelay(std::chrono::milliseconds value)
{
    if (d->preloadDelay == value)
        return;

    d->preloadDelay = value;
    emit preloadDelayChanged();

    if (d->preloadTimer)
        d->preloadDelayed(); //< Restart preload if needed.
}

std::unique_ptr<AsyncImageResult> ResourceIdentificationThumbnail::getImageAsync(
    bool forceRefresh) const
{
    if (!NX_ASSERT(resource() && active()))
        return {};

    if (d->camera)
        forceRefresh = d->isLicensedCamera();

    NX_VERBOSE(this, "getImageAsync for %1, forceRefresh=%2", resource(), forceRefresh);

    auto baseRequest = base_type::getImageAsync(forceRefresh);
    if (!baseRequest && d->fallbackImage.isNull())
        return {};

    return std::make_unique<FallbackImageResult>(std::move(baseRequest), d->fallbackImage,
        [](const QImage& image1, const QImage& image2)
        {
            const auto timestamp1 = AsyncImageResult::timestamp(image1);
            const auto timestamp2 = AsyncImageResult::timestamp(image2);
            return (timestamp1 < timestamp2) ? image2 : image1;
        });
}

std::unique_ptr<AsyncImageResult> ResourceIdentificationThumbnail::getImageAsyncUncached() const
{
    if (d->camera)
    {
        const auto doRequest = d->isLicensedCamera() || (d->enforced && status() != Status::ready);

        NX_VERBOSE(this,
            "getImageAsyncUncached for camera %1, licensed=%2, enforced=%3, status=%4, skip=%5",
            d->camera, d->isLicensedCamera(), d->enforced, status(), !doRequest);

        return doRequest
            ? Private::cameraRequestManager()->request(d->camera, maximumSize())
            : std::unique_ptr<AsyncImageResult>();
    }

    if (resource()->hasFlags(Qn::local_media))
    {
        NX_VERBOSE(this, "getImageAsyncUncached for local file %1", resource());

        return std::make_unique<LocalMediaAsyncImageRequest>(
            resource(),
            LocalMediaAsyncImageRequest::kMiddlePosition,
            maximumSize());
    }

    // TODO: #vkutin Support other resource types in the future.

    NX_VERBOSE(this, "getImageAsyncUncached for irrelevant resource %1, skipping", resource());

    return {};
}

void ResourceIdentificationThumbnail::classBegin()
{
    d->objectComplete = false;
}

void ResourceIdentificationThumbnail::componentComplete()
{
    d->objectComplete = true;

    if (resource())
        d->preloadDelayed();
}

bool ResourceIdentificationThumbnail::enforced() const
{
    return d->enforced;
}

void ResourceIdentificationThumbnail::setEnforced(bool value)
{
    if (d->enforced == value)
        return;

    d->enforced = value;
    emit enforcedChanged();

    if (d->enforced && status() != Status::ready)
        update();
}

void ResourceIdentificationThumbnail::setFallbackImage(const QUrl& imageUrl, qint64 timestampMs,
    int rotationQuadrants)
{
    const auto oldTimestamp = AsyncImageResult::timestamp(d->fallbackImage);
    const microseconds newTimestamp = milliseconds(timestampMs);

    if (newTimestamp < oldTimestamp)
        return;

    const QQuickPixmap quickImage(appContext()->qmlEngine(), imageUrl);
    d->fallbackImage = quickImage.image().transformed(QTransform().rotate(-rotationQuadrants * 90));
    AsyncImageResult::setTimestamp(d->fallbackImage, newTimestamp);
    update();
}

ThumbnailCache* ResourceIdentificationThumbnail::thumbnailCache()
{
    return Private::cache();
}

void ResourceIdentificationThumbnail::registerQmlType()
{
    qmlRegisterType<ResourceIdentificationThumbnail>("nx.vms.client.desktop", 1, 0,
        "ResourceIdentificationThumbnail");
}

} // namespace nx::vms::client::desktop
