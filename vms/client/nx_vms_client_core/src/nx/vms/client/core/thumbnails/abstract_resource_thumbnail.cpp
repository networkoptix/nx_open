// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_resource_thumbnail.h"

#include <chrono>
#include <cmath>
#include <memory>

#include <QtCore/QMetaEnum>
#include <QtQml/QtQml>

#include <core/resource/avi/avi_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_media_layout.h>
#include <nx/utils/enum_utils.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/pending_operation.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/thumbnails/generic_image_store.h>
#include <nx/vms/client/core/thumbnails/thumbnail_image_provider.h>
#include <utils/common/synctime.h>

#include "async_image_result.h"

using namespace std::chrono;

namespace nx::vms::client::core {

namespace {

static const QnAspectRatio kDefaultAspectRatio(16, 9);

static constexpr auto kObsolescenceUpdatePeriod = 30s;

static constexpr auto kUpdateRequestDelay = 100ms;

GenericImageStore* imageStore()
{
    static GenericImageStore store(ThumbnailImageProvider::instance());
    return &store;
}

QnAspectRatio channelAspectRatio(const QnResourcePtr& resource)
{
    if (auto camera = resource.objectCast<QnVirtualCameraResource>())
        return camera->aspectRatio();

    if (auto localMedia = resource.objectCast<QnAviResource>())
    {
        if (localMedia->hasFlags(Qn::still_image))
            return localMedia->imageAspectRatio();

        const auto ar = localMedia->customAspectRatio();
        if (ar.isValid())
            return ar;
    }

    return {};
}

} // namespace

struct AbstractResourceThumbnail::Private
{
    AbstractResourceThumbnail* const q;
    GenericImageStore* const imageStore;

    QnResourcePtr resource;
    nx::utils::ScopedConnections resourceConnections;
    int maximumSize = kUnlimitedSize;
    bool active = true;
    Status status;
    qreal aspectRatio = kDefaultAspectRatio.toFloat();
    bool aspectRatioPredefined = false; //< Whether AR is known before loading an image.
    int rotationQuadrants = 0;
    QImage image;
    QString imageId;
    QUrl url;
    microseconds imageTimestamp = 0us;
    bool isArmServer = false;

    bool obsolete = false;
    minutes obsolescenceDuration = 0min;

    std::unique_ptr<AsyncImageResult> asyncImageResult;

    nx::utils::PendingOperation updateOperation{[this]() { doUpdate(); }, kUpdateRequestDelay};
    bool forceRefresh = false;

    ~Private();

    void setStatus(Status value);
    void setImage(const QImage& value);
    void setAspectRatio(qreal value);
    void setRotation(qreal value);
    void setObsolete(bool value);
    void updateStoredImage();
    void updateObsolescenceWatch();
    void updateIsObsolete();
    bool calculateIsObsolete() const;
    bool supportsObsolescence() const;
    void updateIsArmServer();
    void setIsArmServer(bool value);

    static bool isResourceOnArmServer(const QnResourcePtr& resource);

    struct ObsolescenceWatcher;
    static ObsolescenceWatcher& obsolescenceWatcher();

    void updateAspectRatio();
    void updateRotation();

    void doUpdate();
};

struct AbstractResourceThumbnail::Private::ObsolescenceWatcher
{
    QSet<AbstractResourceThumbnail::Private*> subscribers;
    QTimer timer;

    ObsolescenceWatcher()
    {
        timer.start(kObsolescenceUpdatePeriod);
        timer.callOnTimeout(
            [this]()
            {
                for (auto subscriber: subscribers)
                    subscriber->updateIsObsolete();
            });
    }
};

QString toString(AbstractResourceThumbnail::Status value)
{
    return nx::utils::enumToShortString(value);
}

// ------------------------------------------------------------------------------------------------
// AbstractResourceThumbnail

AbstractResourceThumbnail::AbstractResourceThumbnail(QObject* parent):
    base_type(parent),
    d(new Private{this, imageStore()})
{
    d->updateOperation.setFlags(nx::utils::PendingOperation::NoFlags);
}

AbstractResourceThumbnail::~AbstractResourceThumbnail()
{
    if (!d->imageId.isEmpty())
        d->imageStore->removeImage(d->imageId);
}

QnResourcePtr AbstractResourceThumbnail::resource() const
{
    return d->resource;
}

void AbstractResourceThumbnail::setResource(const QnResourcePtr& value)
{
    if (d->resource == value)
        return;

    d->resourceConnections.reset();
    d->resource = value;
    reset();

    d->updateRotation();
    d->updateAspectRatio();
    d->updateObsolescenceWatch();
    d->updateIsArmServer();

    if (d->resource)
    {
        d->resourceConnections << connect(d->resource.get(), &QnResource::propertyChanged, this,
            [this](const QnResourcePtr& resource, const QString& key)
            {
                if (key == QnMediaResource::customAspectRatioKey())
                    d->updateAspectRatio();
            });

        d->resourceConnections << connect(d->resource.get(), &QnResource::rotationChanged, this,
            [this]() { d->updateRotation(); });

        d->resourceConnections << connect(d->resource.get(), &QnResource::parentIdChanged, this,
            [this]() { d->updateIsArmServer(); });

        // Safety measure. Actually this shall be done by the owner class, who calls setResource.
        // It should listen for resourcesRemoved instead.
        d->resourceConnections << connect(d->resource.get(), &QnResource::flagsChanged, this,
            [this]()
            {
                if (NX_ASSERT(d->resource) && d->resource->hasFlags(Qn::removed))
                    setResource({});
            });
    }

    emit resourceChanged();
    update();
}

QnResource* AbstractResourceThumbnail::rawResource() const
{
    return resource().get();
}

void AbstractResourceThumbnail::setRawResource(QnResource* value)
{
    setResource(value ? value->toSharedPointer() : QnResourcePtr());
}

int AbstractResourceThumbnail::maximumSize() const
{
    return d->maximumSize;
}

void AbstractResourceThumbnail::setMaximumSize(int value)
{
    if (d->maximumSize == value)
        return;

    d->maximumSize = value;
    reset();

    emit maximumSizeChanged();
    update();
}

bool AbstractResourceThumbnail::active() const
{
    return d->active;
}

void AbstractResourceThumbnail::setActive(bool value)
{
    if (d->active == value)
        return;

    d->active = value;
    emit activeChanged();

    if (d->active)
        update();
}

AbstractResourceThumbnail::Status AbstractResourceThumbnail::status() const
{
    return d->status;
}

QImage AbstractResourceThumbnail::image() const
{
    return d->image;
}

QUrl AbstractResourceThumbnail::url() const
{
    return d->url;
}

void AbstractResourceThumbnail::reset()
{
    d->asyncImageResult.reset();
    d->setStatus(Status::empty);
    d->setImage({});
    d->setObsolete(false);
}

void AbstractResourceThumbnail::update(bool forceRefresh)
{
    if (!d->resource || !d->active)
        return;

    d->forceRefresh |= forceRefresh;
    d->updateOperation.requestOperation();
}

void AbstractResourceThumbnail::setImage(const QImage& image)
{
    d->setImage(image);
}

qreal AbstractResourceThumbnail::aspectRatio() const
{
    return d->aspectRatio;
}

int AbstractResourceThumbnail::rotationQuadrants() const
{
    return d->rotationQuadrants;
}

bool AbstractResourceThumbnail::isObsolete() const
{
    return d->obsolete;
}

int AbstractResourceThumbnail::obsolescenceMinutes() const
{
    return d->obsolescenceDuration.count();
}

void AbstractResourceThumbnail::setObsolescenceMinutes(int value)
{
    if (obsolescenceMinutes() == value)
        return;

    d->obsolescenceDuration = minutes(value);
    emit obsolescenceMinutesChanged();

    d->updateObsolescenceWatch();
    d->updateIsObsolete();
}

bool AbstractResourceThumbnail::isArmServer() const
{
    return d->isArmServer;
}

QnAspectRatio AbstractResourceThumbnail::calculateAspectRatio(
    const QnResourcePtr& resource,
    const QnAspectRatio& defaultAspectRatio,
    bool* aspectRatioPredefined)
{
    auto ar = channelAspectRatio(resource);

    if (aspectRatioPredefined)
        *aspectRatioPredefined = ar.isValid();

    if (!ar.isValid())
        ar = defaultAspectRatio;

    if (resource.objectCast<QnAviResource>())
        return ar;

    QnConstResourceVideoLayoutPtr layout;
    if (const auto mediaResource = resource.dynamicCast<QnMediaResource>())
        layout = mediaResource->getVideoLayout();

    return layout ? ar.tiled(layout->size()) : ar;
}

void AbstractResourceThumbnail::registerQmlType()
{
    qmlRegisterUncreatableType<AbstractResourceThumbnail>("nx.vms.client.core", 1, 0,
        "AbstractResourceThumbnail", "Cannot create instance of AbstractResourceThumbnail");
}

// ------------------------------------------------------------------------------------------------
// AbstractResourceThumbnail::Private

AbstractResourceThumbnail::Private::~Private()
{
    obsolescenceWatcher().subscribers.remove(this);
}

void AbstractResourceThumbnail::Private::setStatus(Status value)
{
    if (status == value)
        return;

    NX_VERBOSE(q, "Status changed to \"%1\"", value);

    status = value;
    emit q->statusChanged();
}

void AbstractResourceThumbnail::Private::setImage(const QImage& value)
{
    if (image.constBits() == value.constBits())
        return;

    NX_VERBOSE(q, "Received new image %3x%4 for %1 (%2)",
        resource ? resource->getName() : QString("<no resource>"),
        resource ? resource->getId() : QnUuid(),
        value.width(),
        value.height());

    image = value;
    imageTimestamp = AsyncImageResult::timestamp(image);

    if (!aspectRatioPredefined && !image.isNull())
        setAspectRatio(qreal(image.width()) / image.height());

    updateStoredImage();
    updateIsObsolete();

    emit q->imageChanged();
}

void AbstractResourceThumbnail::Private::setAspectRatio(qreal value)
{
    if (qFuzzyIsNull(aspectRatio - value))
        return;

    aspectRatio = value;
    emit q->aspectRatioChanged();
}

void AbstractResourceThumbnail::Private::setRotation(qreal value)
{
    const int steps = ((qRound(value / 90.0) % 4) + 4) % 4;
    if (rotationQuadrants == steps)
        return;

    rotationQuadrants = steps;
    emit q->rotationChanged();
}

void AbstractResourceThumbnail::Private::setObsolete(bool value)
{
    if (obsolete == value)
        return;

    obsolete = value;
    emit q->obsoleteChanged();
}

void AbstractResourceThumbnail::Private::updateStoredImage()
{
    if (!imageId.isEmpty())
        imageStore->removeImage(imageId);

    if (image.isNull())
    {
        imageId = QString();
        url = QUrl();
    }
    else
    {
        imageId = imageStore->addImage(image);
        url = imageStore->makeUrl(imageId);
    }
}

void AbstractResourceThumbnail::Private::updateAspectRatio()
{
    setAspectRatio(
        calculateAspectRatio(resource, kDefaultAspectRatio, &aspectRatioPredefined).toFloat());
}

void AbstractResourceThumbnail::Private::updateRotation()
{
    const auto mediaResource = resource.dynamicCast<QnMediaResource>();
    const auto rotationDegrees = mediaResource ? mediaResource->forcedRotation().value_or(0) : 0.0;

    NX_ASSERT(std::fabs(std::fmod(rotationDegrees, 90.0)) < 1.0,
        "Resource default rotation is not a multiple of 90 degrees (%1)",
        resource ? resource->getId().toSimpleString() : QString());

    setRotation(rotationDegrees);
}

void AbstractResourceThumbnail::Private::updateObsolescenceWatch()
{
    if (const bool watched = obsolescenceDuration > 0min && supportsObsolescence())
        obsolescenceWatcher().subscribers.insert(this);
    else
        obsolescenceWatcher().subscribers.remove(this);
}

void AbstractResourceThumbnail::Private::updateIsObsolete()
{
    setObsolete(calculateIsObsolete());
}

bool AbstractResourceThumbnail::Private::calculateIsObsolete() const
{
    if (obsolescenceDuration <= 0min || !supportsObsolescence() || imageTimestamp == 0us)
        return false;

    const microseconds age = microseconds(qnSyncTime->currentUSecsSinceEpoch()) - imageTimestamp;
    return age > obsolescenceDuration;
}

bool AbstractResourceThumbnail::Private::supportsObsolescence() const
{
    return resource && resource->hasFlags(Qn::live_cam);
}

void AbstractResourceThumbnail::Private::updateIsArmServer()
{
    setIsArmServer(isResourceOnArmServer(resource));
}

void AbstractResourceThumbnail::Private::setIsArmServer(bool value)
{
    if (isArmServer == value)
        return;

    isArmServer = value;
    emit q->isArmServerChanged();
}

bool AbstractResourceThumbnail::Private::isResourceOnArmServer(const QnResourcePtr& resource)
{
    return resource && QnMediaServerResource::isArmServer(resource->getParentResource());
}

AbstractResourceThumbnail::Private::ObsolescenceWatcher&
    AbstractResourceThumbnail::Private::obsolescenceWatcher()
{
    static ObsolescenceWatcher instance;
    return instance;
}

void AbstractResourceThumbnail::Private::doUpdate()
{
    const bool forceRefreshRequested = forceRefresh;
    forceRefresh = false;

    if (!resource || !active)
        return;

    NX_VERBOSE(q, "Update for %1 (%2), forceRefresh=%3", resource->getName(),
        resource->getId(), forceRefreshRequested);

    asyncImageResult = std::move(q->getImageAsync(forceRefreshRequested));
    if (!asyncImageResult)
    {
        setStatus(Status::unavailable);
        return;
    }

    setStatus(Status::loading);

    const auto handleImageReceived =
        [this]()
        {
            // `This` is guarded by `asyncImageResult` being the signal sender, so the handler
            // won't be called when `this` (and thus `asyncImageResult` too) is destroyed.

            const auto newImage = asyncImageResult->image();
            if (!newImage.isNull())
                setImage(newImage);

            asyncImageResult.reset();
            setStatus(url.isValid() ? Status::ready : Status::unavailable);

            emit q->updated();
        };

    if (asyncImageResult->isReady())
        handleImageReceived();
    else
        connect(asyncImageResult.get(), &AsyncImageResult::ready, handleImageReceived);
}

} // namespace nx::vms::client::core
