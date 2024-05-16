// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "visible_item_data_decorator_model.h"

#include <deque>

#include <QtCore/QDeadlineTimer>
#include <QtCore/QScopedValueRollback>
#include <QtCore/QTimer>

#include <core/resource/media_resource.h>
#include <core/resource/media_server_resource.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/client/core/event_search/event_search_globals.h>
#include <nx/vms/client/core/image_providers/camera_thumbnail_provider.h>
#include <nx/vms/client/core/image_providers/resource_thumbnail_provider.h>
#include <nx/vms/client/core/thumbnails/abstract_resource_thumbnail.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/utils/log/log.h>
#include <nx/utils/pending_operation.h>
#include <nx/utils/range_adapters.h>
#include <utils/common/delayed.h>

#include "detail/preview_rect_calculator.h"

namespace nx::vms::client::core {

namespace {

using namespace std::chrono;

static constexpr milliseconds kPreviewCheckInterval = 100ms;
static constexpr milliseconds kPreviewLoadTimeout = 1min;
static constexpr int kDefaultThumbnailWidth = 224;
static constexpr int kSimultaneousPreviewLoadsLimit = 15;
static constexpr int kSimultaneousPreviewLoadsLimitArm = 5;

static const auto kBypassVideoCachePropertyName = "__qn_bypassVideoCache";

using PreviewProviderPtr = std::unique_ptr<ResourceThumbnailProvider>;

struct PreviewData
{
    PreviewProviderPtr provider;
    QDeadlineTimer reloadTimer;
    QRectF highlightRect;
};

struct PreviewLoadData
{
    QnMediaServerResourcePtr server;
    QDeadlineTimer timeout;
};

struct ServerLoadData
{
    int loadingCounter = 0;
};

enum class WaitingStatus
{
    idle,
    requiresLoading,
    awaitsReloading
};

WaitingStatus waitingStatus(const PreviewData& data)
{
    if (!data.provider)
        return WaitingStatus::idle;

    switch (data.provider->status())
    {
        case ThumbnailStatus::Invalid:
            return WaitingStatus::requiresLoading;

        case ThumbnailStatus::NoData:
            return data.reloadTimer.hasExpired()
                ? WaitingStatus::requiresLoading
                : WaitingStatus::awaitsReloading;

        default:
            return WaitingStatus::idle;
    }
}

void calculatePreviewRectsInternal(const QnResourcePtr& resource,
    const QRectF& highlightRect,
    QRectF& cropRect,
    QRectF& newHighlightRect)
{
    if (!highlightRect.isValid() || !NX_ASSERT(resource))
    {
        cropRect = {};
        newHighlightRect = {};
        return;
    }

    static QnAspectRatio kPreviewAr(16, 9);

    const auto resourceAr = AbstractResourceThumbnail::calculateAspectRatio(resource, kPreviewAr);
    calculatePreviewRects(resourceAr, highlightRect, kPreviewAr, cropRect, newHighlightRect);
}

int maxSimultaneousPreviewLoads(
    const VisibleItemDataDecoratorModel::Settings& settings,
    const QnMediaServerResourcePtr& server)
{
    if (QnMediaServerResource::isArmServer(server))
    {
        return std::clamp(settings.maxSimultaneousPreviewLoadsArm,
            1, kSimultaneousPreviewLoadsLimitArm);
    }

    return std::clamp(settings.maxSimultaneousPreviewLoadsArm,
        1, kSimultaneousPreviewLoadsLimit);
}

QnMediaServerResourcePtr previewServer(const ResourceThumbnailProvider* provider)
{
    const auto resource = provider->resource();
    return resource
        ? resource->getParentResource().dynamicCast<QnMediaServerResource>()
        : QnMediaServerResourcePtr();
}

bool isBusy(ThumbnailStatus status)
{
    return status == ThumbnailStatus::Loading || status == ThumbnailStatus::Refreshing;
}

qlonglong providerId(ResourceThumbnailProvider* provider)
{
    return qlonglong(provider);
}

QHash<intptr_t, QPointer<ResourceThumbnailProvider>>& previewsById()
{
    static QHash<intptr_t, QPointer<ResourceThumbnailProvider>> data;
    return data;
}

} // namespace

//-------------------------------------------------------------------------------------------------

struct VisibleItemDataDecoratorModel::Private
{
    VisibleItemDataDecoratorModel* const q;

    VisibleItemDataDecoratorModel::Settings settings;
    std::deque<PreviewData> previews;
    QHash<ResourceThumbnailProvider*, PreviewLoadData> loadingByProvider;
    QHash<QnMediaServerResourcePtr, ServerLoadData> loadingByServer;
    ResourceThumbnailProvider* providerLoadingFromCache = nullptr;
    QTimer hangedPreviewRequestsChecker;
    nx::utils::ElapsedTimer sinceLastPreviewRequest;
    nx::utils::PendingOperation previewLoadOperation;
    bool previewsEnabled = true;

    QSet<QPersistentModelIndex> visibleItems;


    void handlePreviewLoadingEnded(ResourceThumbnailProvider* provider);
    void createPreviews(int first, int count);
    void updatePreviewProvider(int row);
    void requestPreview(int row);
    void loadNextPreview();
    bool isNextPreviewLoadAllowed(const ResourceThumbnailProvider* provider) const;
    QString previewId(int row) const;
    qreal previewAspectRatio(int row) const;
    QRectF previewHighlightRect(int row) const;
    EventSearch::PreviewState previewState(int row) const;

    bool setVisible(const QModelIndex& index, bool value);


};

void VisibleItemDataDecoratorModel::Private::handlePreviewLoadingEnded(ResourceThumbnailProvider* provider)
{
    const auto previewData = loadingByProvider.find(provider);
    if (previewData == loadingByProvider.end())
        return;

    const auto serverData = loadingByServer.find(previewData->server);
    if (NX_ASSERT(serverData != loadingByServer.end()))
    {
        --serverData->loadingCounter;
        NX_ASSERT(serverData->loadingCounter >= 0);
        if (serverData->loadingCounter <= 0)
            loadingByServer.erase(serverData);
    }

    loadingByProvider.erase(previewData);
    previewLoadOperation.requestOperation();
}

void VisibleItemDataDecoratorModel::Private::createPreviews(int first, int count)
{
    if (!NX_ASSERT(first >= 0 && first <= (int) previews.size()))
        return;

    std::vector<PreviewData> inserted(count);
    previews.insert(previews.begin() + first,
        std::make_move_iterator(inserted.begin()), std::make_move_iterator(inserted.end()));

    for (int i = 0; i < count; ++i)
        updatePreviewProvider(first + i);
}

void VisibleItemDataDecoratorModel::Private::updatePreviewProvider(int row)
{
    if (!NX_ASSERT(row >= 0
        && row < (int) previews.size()
        && (int) previews.size() == q->rowCount()))
    {
        return;
    }

    const auto index = q->index(row, 0);
    const auto previewResource = index.data(ResourceRole).value<QnResourcePtr>();
    const auto mediaResource = dynamic_cast<QnMediaResource*>(previewResource.get());

    if (!mediaResource || !previewResource->hasFlags(Qn::video))
    {
        previews[row] = {};
        return;
    }

    const auto previewTimeMs =
        duration_cast<milliseconds>(index.data(PreviewTimeRole).value<microseconds>());

    const auto rotation = mediaResource->forcedRotation().value_or(0);
    const auto highlightRect = Geometry::rotatedRelativeRectangle(
        index.data(ItemZoomRectRole).value<QRectF>(), -rotation / 90);

    const bool precisePreview = !highlightRect.isEmpty()
        || index.data(ForcePrecisePreviewRole).toBool();

    const auto objectTrackId = index.data(ObjectTrackIdRole).value<nx::Uuid>();

    nx::api::ResourceImageRequest request;
    request.resource = previewResource;
    request.timestampMs =
        previewTimeMs.count() > 0 ? previewTimeMs : nx::api::ImageRequest::kLatestThumbnail;
    request.rotation = rotation;
    request.size = QSize(kDefaultThumbnailWidth, 0);
    request.format = nx::api::ImageRequest::ThumbnailFormat::jpg;
    request.aspectRatio = nx::api::ImageRequest::AspectRatio::auto_;
    request.roundMethod = precisePreview
        ? nx::api::ImageRequest::RoundMethod::precise
        : nx::api::ImageRequest::RoundMethod::iFrameAfter;
    request.streamSelectionMode = index.data(PreviewStreamSelectionRole)
                                      .value<nx::api::ImageRequest::StreamSelectionMode>();
    request.objectTrackId = objectTrackId;

    calculatePreviewRectsInternal(request.resource, highlightRect, request.crop,
        previews[row].highlightRect);

    auto& previewProvider = previews[row].provider;
    if (!previewProvider || request.resource != previewProvider->requestData().resource)
    {
        previewProvider.reset(new ResourceThumbnailProvider(request));
        const auto id = providerId(previewProvider.get());
        previewsById()[id] = previewProvider.get();

        connect(previewProvider.get(), &QObject::destroyed, q,
            [this, provider = previewProvider.get()]() { handlePreviewLoadingEnded(provider); },
            Qt::QueuedConnection);

        connect(previewProvider.get(), &ImageProvider::statusChanged, q,
            [this, provider = previewProvider.get()](ThumbnailStatus status)
            {
                if (provider == providerLoadingFromCache)
                    return;

                switch (status)
                {
                    case ThumbnailStatus::Loading:
                    case ThumbnailStatus::Refreshing:
                    {
                        const auto server = previewServer(provider);
                        loadingByProvider.insert(provider, {server, kPreviewLoadTimeout});

                        auto& serverData = loadingByServer[server];
                        ++serverData.loadingCounter;

                        NX_ASSERT(serverData.loadingCounter
                            <= maxSimultaneousPreviewLoads(settings, server));
                        break;
                    }

                    default:
                    {
                        handlePreviewLoadingEnded(provider);
                        break;
                    }
                }
            });
    }
    else
    {
        const auto oldRequest = previewProvider->requestData();
        const bool forceUpdate = oldRequest.timestampMs != request.timestampMs
            || oldRequest.streamSelectionMode != request.streamSelectionMode;

        previewProvider->setRequestData(request, forceUpdate);
    }

    previewProvider->setProperty(kBypassVideoCachePropertyName,
        index.data(HasExternalBestShotRole).toBool());

    if (q->previewsEnabled() && waitingStatus(previews[row]) != WaitingStatus::idle)
        previewLoadOperation.requestOperation();
}

void VisibleItemDataDecoratorModel::Private::requestPreview(int row)
{
    const QPersistentModelIndex index(q->index(row, 0));
    auto& preview = previews[row];

    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = connect(preview.provider.get(), &ImageProvider::statusChanged, q,
        [this, connection, index](ThumbnailStatus status)
        {
            if (isBusy(status))
                return;

            QObject::disconnect(*connection);
            if (!index.isValid())
                return;

            static const QVector<int> roles({
                PreviewIdRole,
                PreviewStateRole,
                PreviewAspectRatioRole,
                PreviewHighlightRectRole});

            emit q->dataChanged(index, index, roles);
        });

    NX_VERBOSE(q, "Load preview for row=%1", row);

    preview.provider->loadAsync();
    preview.reloadTimer.setRemainingTime(settings.previewReloadDelayMs);
}

void VisibleItemDataDecoratorModel::Private::loadNextPreview()
{
    if (!q->previewsEnabled())
        return;

    NX_VERBOSE(q, "Trying to load next preview if needed");

    std::set<int> rowsToLoad;
    bool awaitingReload = false;

    for (const auto& index: visibleItems)
    {
        if (!NX_ASSERT(index.isValid()))
            continue;

        const auto status = waitingStatus(previews[index.row()]);
        if (status == WaitingStatus::requiresLoading)
            rowsToLoad.insert(index.row());
        else if (status == WaitingStatus::awaitsReloading)
            awaitingReload = true;
    }

    int loadedFromCache = 0;
    for (int row: rowsToLoad)
    {
        const auto provider = previews[row].provider.get();

        if (!provider->property(kBypassVideoCachePropertyName).toBool())
        {
            QScopedValueRollback<ResourceThumbnailProvider*> loadingFromCacheGuard(
                providerLoadingFromCache, provider);

            if (provider->tryLoad())
            {
                NX_VERBOSE(q, "Loaded preview from videocache (timestamp=%1, objectTrackId=%2",
                    provider->requestData().timestampMs,
                    provider->requestData().objectTrackId);

                ++loadedFromCache;
                continue;
            }
        }

        if (!isNextPreviewLoadAllowed(provider))
            continue;

        NX_VERBOSE(q, "Requesting preview from server (timestamp=%1, objectTrackId=%2",
            provider->requestData().timestampMs,
            provider->requestData().objectTrackId);

        requestPreview(row);
        sinceLastPreviewRequest.restart();
    }

    if (awaitingReload
        || (((int) rowsToLoad.size() > loadedFromCache) && loadingByProvider.empty()))
    {
        previewLoadOperation.requestOperation();
    }
}

bool VisibleItemDataDecoratorModel::Private::isNextPreviewLoadAllowed(
    const ResourceThumbnailProvider* provider) const
{
    if (!NX_ASSERT(provider))
        return false;

    const auto server = previewServer(provider);
    const auto maxPreviewLoads = maxSimultaneousPreviewLoads(settings, server);
    return (loadingByServer.value(server).loadingCounter < maxPreviewLoads)
        && (settings.tilePreviewLoadIntervalMs <= 0ms
            || sinceLastPreviewRequest.hasExpired(settings.tilePreviewLoadIntervalMs));
}


QString VisibleItemDataDecoratorModel::Private::previewId(int row) const
{
    const auto& provider = previews[row].provider;
    return provider && !provider->image().isNull()
        ? QString::number(providerId(provider.get()))
        : QString{};
}

qreal VisibleItemDataDecoratorModel::Private::previewAspectRatio(int row) const
{
    const auto& provider = previews[row].provider;
    const QSizeF size = provider ? provider->sizeHint() : QSizeF();
    return (size.height()) > 0 ? (size.width() / size.height()) : 1.0;
}

QRectF VisibleItemDataDecoratorModel::Private::previewHighlightRect(int row) const
{
    const auto& provider = previews[row].provider;
    if (provider && QnLexical::deserialized<bool>(
        provider->image().text(CameraThumbnailProvider::kFrameFromPluginKey)))
    {
        return QRectF{};
    }

    return previews[row].highlightRect;
}

EventSearch::PreviewState VisibleItemDataDecoratorModel::Private::previewState(int row) const
{
    if (const auto& provider = previews[row].provider)
    {
        if (!provider->image().isNull())
            return EventSearch::PreviewState::ready;

        switch (provider->status())
        {
            case ThumbnailStatus::Invalid:
                return EventSearch::PreviewState::initial;

            case ThumbnailStatus::Loading:
            case ThumbnailStatus::Refreshing:
                return EventSearch::PreviewState::busy;

            case ThumbnailStatus::Loaded:
                return EventSearch::PreviewState::ready;

            case ThumbnailStatus::NoData:
                return EventSearch::PreviewState::missing;
        }
    }

    return EventSearch::PreviewState::missing;
}

bool VisibleItemDataDecoratorModel::Private::setVisible(const QModelIndex& index, bool value)
{
    if (!NX_ASSERT(index.isValid()) || q->isVisible(index) == value)
        return false;

    if (value)
    {
        visibleItems.insert(index);
        if (previewsEnabled && waitingStatus(previews[index.row()]) == WaitingStatus::requiresLoading)
            previewLoadOperation.requestOperation();
    }
    else
    {
        visibleItems.remove(index);
    }

    emit q->dataChanged(index, index, {IsVisibleRole});

    return true;
}

//-------------------------------------------------------------------------------------------------

VisibleItemDataDecoratorModel::VisibleItemDataDecoratorModel(const Settings& settings,
    QObject* parent)
    :
    base_type(parent),
    d(new Private{.q = this, .settings = settings, .hangedPreviewRequestsChecker = QTimer{}})
{
    connect(this, &QAbstractListModel::rowsAboutToBeRemoved, this,
        [this](const QModelIndex& /*parent*/, int first, int last)
        {
            const auto beginIt = d->previews.begin();
            d->previews.erase(beginIt + first, beginIt + (last + 1));

            QSet<QPersistentModelIndex> toRemove;
            for (const auto& index: d->visibleItems)
            {
                if (index.row() >= first && index.row() <= last)
                    toRemove.insert(index);
            }

            d->visibleItems -= toRemove;
        });

    connect(this, &QAbstractListModel::modelReset, this,
        [this]
        {
            d->createPreviews(0, rowCount());
        });

    connect(this, &QAbstractListModel::modelAboutToBeReset, this,
        [this]
        {
            d->previews.clear();
            d->visibleItems.clear();
        });

    connect(this, &QAbstractListModel::rowsInserted, this,
        [this](const QModelIndex& parent, int first, int last)
        {
            if (NX_ASSERT(!parent.isValid()))
                d->createPreviews(first, last - first + 1);
        });

    connect(this, &QAbstractItemModel::dataChanged, this,
        [this](const QModelIndex& topLeft, const QModelIndex& bottomRight)
        {
            for (int row = topLeft.row(); row <= bottomRight.row(); ++row)
                d->updatePreviewProvider(row);
        });

    d->hangedPreviewRequestsChecker.callOnTimeout(this,
        [this]()
        {
            QSet<ResourceThumbnailProvider*> hangedProviders;
            for (const auto& [provider, data]: nx::utils::constKeyValueRange(d->loadingByProvider))
            {
                if (data.timeout.hasExpired())
                    hangedProviders.insert(provider);
            }

            for (auto provider: hangedProviders)
                d->handlePreviewLoadingEnded(provider);
        });

    d->hangedPreviewRequestsChecker.start(10s);

    const auto loadNextPreviewDelayed =
        [this]() { executeLater([this]() { d->loadNextPreview(); }, this); };

    d->previewLoadOperation.setFlags(nx::utils::PendingOperation::FireImmediately);
    d->previewLoadOperation.setInterval(kPreviewCheckInterval);
    d->previewLoadOperation.setCallback(loadNextPreviewDelayed);
}

VisibleItemDataDecoratorModel::~VisibleItemDataDecoratorModel()
{
}

bool VisibleItemDataDecoratorModel::setData(const QModelIndex& index,
    const QVariant& value,
    int role)
{
    if (role != IsVisibleRole || !index.isValid() || !NX_ASSERT(checkIndex(index)))
        return false;

    return d->setVisible(index, value.toBool());
}

QVariant VisibleItemDataDecoratorModel::data(const QModelIndex& index, int role) const
{
    if (!NX_ASSERT(checkIndex(index)))
        return {};

    switch (role)
    {
        case PreviewIdRole:
            return d->previewId(index.row());

        case PreviewStateRole:
            return QVariant::fromValue(d->previewState(index.row()));

        case PreviewAspectRatioRole:
            return d->previewAspectRatio(index.row());

        case PreviewHighlightRectRole:
            return d->previewHighlightRect(index.row());

        case IsVisibleRole:
            return isVisible(index);

        case PreviewTimeMsRole:
        {
            const auto time = data(index, PreviewTimeRole).value<microseconds>();
            return QVariant::fromValue(duration_cast<milliseconds>(time).count());
        }

        default:
            return base_type::data(index, role);
    }
}

void VisibleItemDataDecoratorModel::setSourceModel(QAbstractItemModel* value)
{
    if (value == sourceModel())
        return;

    while (!d->visibleItems.empty())
        setData(*d->visibleItems.begin(), false, IsVisibleRole);
    d->previews.clear();

    base_type::setSourceModel(value);
}

bool VisibleItemDataDecoratorModel::previewsEnabled() const
{
    return d->previewsEnabled;
}

void VisibleItemDataDecoratorModel::setPreviewsEnabled(bool value)
{
    if (value == d->previewsEnabled)
        return;

    d->previewsEnabled = value;
    emit previewsEnabledChanged();

    if (d->previewsEnabled)
        d->previewLoadOperation.requestOperation();
}

bool VisibleItemDataDecoratorModel::isVisible(const QModelIndex& index) const
{
    return d->visibleItems.contains(index);
}

//-------------------------------------------------------------------------------------------------

VisibleItemDataDecoratorModel::PreviewProvider::PreviewProvider():
    QQuickImageProvider(Image)
{
}

QImage VisibleItemDataDecoratorModel::PreviewProvider::requestImage(const QString& id,
    QSize* size,
    const QSize& /*requestedSize*/)
{
    const auto provider = previewsById().value(id.toLongLong());
    return provider ? provider->image() : QImage();
}

} // namespace nx::vms::client::core
