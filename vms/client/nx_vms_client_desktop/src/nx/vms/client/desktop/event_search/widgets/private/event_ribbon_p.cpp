// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_ribbon_p.h"

#include <algorithm>
#include <chrono>

#include <QtCore/QAbstractListModel>
#include <QtCore/QScopedValueRollback>
#include <QtGui/QTextDocument>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenu>
#include <QtWidgets/QScrollBar>

#include <client/client_globals.h>
#include <common/common_module.h>
#include <core/resource/media_resource.h>
#include <core/resource/media_server_resource.h>
#include <nx/api/mediaserver/image_request.h>
#include <nx/utils/app_info.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/metatypes.h>
#include <nx/utils/range_adapters.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/common/utils/custom_painted.h>
#include <nx/vms/client/desktop/common/utils/progress_state.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/event_search/widgets/event_tile.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/style/private/style_private.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/utils/video_cache.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>
#include <nx/vms/client/desktop/workbench/extensions/local_notifications_manager.h>
#include <nx/vms/client/desktop/workbench/workbench_animations.h>
#include <nx/vms/common/html/html.h>
#include <recording/time_period.h>
#include <ui/common/notification_levels.h>
#include <ui/common/read_only.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <utils/common/delayed.h>
#include <utils/common/event_processors.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/math/color_transformations.h>

namespace nx::vms::client::desktop {

using namespace std::chrono;

namespace {

static constexpr int kDefaultTileSpacing = 1;
static constexpr int kScrollBarStep = 16;

static constexpr int kDefaultThumbnailWidth = 214;
static constexpr int kAlternativeThumbnailWidth = 242;

static constexpr auto kFadeCurtainColorName = "dark3";

// There's a lot of code duplication of preview loading mechanics with RightPanelModelsAdapter.
// TODO: #vkutin Refactor it.

static constexpr milliseconds kPreviewCheckInterval = 100ms;

static constexpr milliseconds kPreviewLoadTimeout = 1min;

static constexpr int kFpsLimit = 60;
static constexpr milliseconds kTimeBetweenLayoutRequests = 1000ms / kFpsLimit;

/*
 * Tiles can have optional timed auto-close mode.
 * When such tile is first created, expiration timer is set to kInvisibleAutoCloseDelay.
 * Every time the tile becomes visible, remaining time is reset to kVisibleAutoCloseDelay.
 * When the tile becomes invisible, remaining time is not changed.
 * When the tile is hovered, the timer is ignored.
 * When the tile stops being hovered, remaining time is reset to kVisibleAutoCloseDelay.
 */
static constexpr milliseconds kVisibleAutoCloseDelay = 20s;
static constexpr milliseconds kInvisibleAutoCloseDelay = 80s;

static const auto kTilePreviewLoadInterval = milliseconds(ini().tilePreviewLoadIntervalMs);

static const int kMaximumThumbnailWidth = ini().rightPanelMaxThumbnailWidth;

static constexpr int kNumAnimatedTilesAtInsertion = 3;

static const auto kBypassVideoCachePropertyName = "__qn_bypassVideoCache";

static constexpr int kTileDescriptionLineLimit = 8;

QSize minimumWidgetSize(QWidget* widget)
{
    return widget->minimumSizeHint().expandedTo(widget->minimumSize());
}

bool shouldAnimateTile(const QModelIndex& index)
{
    return !index.data(Qt::DisplayRole).toString().isEmpty();
}

int maxSimultaneousPreviewLoads(const QnMediaServerResourcePtr& server)
{
    if (QnMediaServerResource::isArmServer(server))
    {
        return std::clamp(ini().maxSimultaneousTilePreviewLoadsArm,
            1, EventRibbon::kSimultaneousPreviewLoadsLimitArm);
    }

    return std::clamp(ini().maxSimultaneousTilePreviewLoads,
        1, EventRibbon::kSimultaneousPreviewLoadsLimit);
}

QnMediaServerResourcePtr previewServer(const ResourceThumbnailProvider* provider)
{
    const auto resource = provider->resource();
    return resource
        ? resource->getParentResource().dynamicCast<QnMediaServerResource>()
        : QnMediaServerResourcePtr();
}

} // namespace

EventRibbon::Private::Private(EventRibbon* q):
    q(q),
    m_scrollBar(new QScrollBar(Qt::Vertical, q)),
    m_viewport(new QWidget(q)),
    m_autoCloseTimer(new QTimer()),
    m_previewLoad(new nx::utils::PendingOperation()),
    m_layoutUpdate(new nx::utils::PendingOperation())
{
    NX_ASSERT(Importance() == Importance::NoNotification);

    q->setAttribute(Qt::WA_Hover);
    m_viewport->setAttribute(Qt::WA_Hover);

    setScrollBarRelevant(false);
    m_scrollBar->setSingleStep(kScrollBarStep);
    m_scrollBar->setFixedWidth(m_scrollBar->sizeHint().width());
    anchorWidgetToParent(m_scrollBar.get(), Qt::RightEdge | Qt::TopEdge | Qt::BottomEdge);

    const int mainPadding = q->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    anchorWidgetToParent(m_viewport.get(), {mainPadding, 0, mainPadding * 2, 0});

    installEventHandler(m_viewport.get(),
        {QEvent::Show, QEvent::Hide, QEvent::Resize, QEvent::LayoutRequest},
        this, &Private::updateView);

    connect(m_scrollBar.get(), &QScrollBar::valueChanged, this, &Private::updateView);

    m_autoCloseTimer->setInterval(1s);
    connect(m_autoCloseTimer.get(), &QTimer::timeout, this, &Private::closeExpiredTiles);

    const auto loadNextPreviewDelayed =
        [this]() { executeLater([this]() { loadNextPreview(); }, this); };

    m_previewLoad->setInterval(kPreviewCheckInterval);
    m_previewLoad->setFlags(nx::utils::PendingOperation::FireImmediately);
    m_previewLoad->setCallback(loadNextPreviewDelayed);

    // Limit the number of layout update requests to 60 per second.
    m_layoutUpdate->setInterval(kTimeBetweenLayoutRequests);
    m_layoutUpdate->setFlags(nx::utils::PendingOperation::NoFlags);
    m_layoutUpdate->setCallback([this]() { Private::updateView(); });
}

EventRibbon::Private::~Private()
{
}

QAbstractListModel* EventRibbon::Private::model() const
{
    return m_model;
}

void EventRibbon::Private::setModel(QAbstractListModel* model)
{
    if (m_model == model)
        return;

    m_modelConnections = {};
    clear();

    m_model = model;

    if (!m_model)
    {
        m_autoCloseTimer->stop();
        return;
    }

    m_autoCloseTimer->start();

    insertNewTiles(0, m_model->rowCount(), UpdateMode::instant, false);

    m_modelConnections << connect(m_model, &QObject::destroyed, this,
        [this]()
        {
            m_model = nullptr;
            clear();
        });

    m_modelConnections << connect(m_model, &QAbstractListModel::modelReset, this,
        [this]()
        {
            clear();
            insertNewTiles(0, m_model->rowCount(), UpdateMode::instant, false);
        });

    m_modelConnections << connect(m_model, &QAbstractListModel::rowsInserted, this,
        [this](const QModelIndex& /*parent*/, int first, int last)
        {
            insertNewTiles(first, last - first + 1, m_insertionMode, m_scrollDownAfterInsertion);
            NX_ASSERT(m_model->rowCount() == count());
        });

    m_modelConnections << connect(m_model, &QAbstractListModel::rowsAboutToBeRemoved, this,
        [this](const QModelIndex& /*parent*/, int first, int last)
        {
            for (int index = first; index <= last; ++index)
                m_deadlines.remove(m_model->index(index));
        });

    m_modelConnections << connect(m_model, &QAbstractListModel::rowsRemoved, this,
        [this](const QModelIndex& /*parent*/, int first, int last)
        {
            removeTiles(first, last - first + 1, m_removalMode);
            NX_ASSERT(m_model->rowCount() == count());
        });

    m_modelConnections << connect(m_model, &QAbstractListModel::dataChanged, this,
        [this](const QModelIndex& first, const QModelIndex& last)
        {
            const auto updateRange = m_visible.intersected({first.row(), last.row() + 1});
            for (int i = updateRange.lower(); i < updateRange.upper(); ++i)
                updateTile(i);
        });

    m_modelConnections << connect(m_model, &QAbstractListModel::rowsMoved, this,
        [this](const QModelIndex& /*parent*/, int sourceFirst, int sourceLast,
            const QModelIndex& /*destinationParent*/, int destinationIndex)
        {
            NX_ASSERT(destinationIndex < sourceFirst || destinationIndex > sourceLast + 1);
            const auto movedCount = sourceLast - sourceFirst + 1;
            const auto index = destinationIndex < sourceFirst
                ? destinationIndex
                : destinationIndex - movedCount;

            QnScopedValueRollback<bool> updateGuard(&m_updating, true);
            removeTiles(sourceFirst, movedCount, UpdateMode::instant);
            insertNewTiles(index, movedCount, UpdateMode::instant, false);

            updateGuard.rollback();
            updateView();

            NX_ASSERT(m_model->rowCount() == count());
        });
}

void EventRibbon::Private::updateTile(int index)
{
    NX_CRITICAL(index >= 0 && index < count());
    if (!NX_ASSERT(m_model))
        return;

    const auto modelIndex = m_model->index(index);
    ensureWidget(index);

    auto widget = m_tiles[index]->widget.get();
    if (!NX_ASSERT(widget))
        return;

    // Check whether the tile is a special busy indicator tile.
    const auto busyIndicatorVisibility = modelIndex.data(Qn::BusyIndicatorVisibleRole);
    if (busyIndicatorVisibility.isValid())
    {
        constexpr int kIndicatorHeight = 24;
        widget->setFixedHeight(kIndicatorHeight);
        widget->setBusyIndicatorVisible(busyIndicatorVisibility.toBool());
        return;
    }

    // Select tile color style.
    widget->setVisualStyle(modelIndex.data(Qn::AlternateColorRole).toBool()
        ? EventTile::Style::informer
        : EventTile::Style::standard);

    QString tileDescription = modelIndex.data(Qn::DescriptionTextRole).toString();

    // Limit the number of lines inside tile description.
    // Empty description should remain empty, without any invisible html.
    if (!tileDescription.isEmpty())
    {
        QTextDocument doc;
        doc.setHtml(nx::vms::common::html::toHtml(tileDescription));
        WidgetUtils::elideDocumentLines(&doc, kTileDescriptionLineLimit);
        tileDescription = doc.toHtml("utf-8");
    }

    // Check whether the tile is a special progress bar tile.
    const auto progressVariant = modelIndex.data(Qn::ProgressValueRole);
    if (progressVariant.canConvert<ProgressState>())
    {
        const auto progress = progressVariant.value<ProgressState>();

        if (progress.isIndefinite() || progress.value())
        {
            if (const auto value = progress.value())
                widget->setProgressValue(*value);
            else
                widget->setIndefiniteProgress();

            widget->setTitle({});
            widget->setProgressBarVisible(true);
            widget->setProgressTitle(modelIndex.data(Qt::DisplayRole).toString());
            widget->setProgressFormat(modelIndex.data(Qn::ProgressFormatRole).toString());
            widget->setDescription(tileDescription);
            widget->setToolTip(modelIndex.data(Qn::DescriptionTextRole).toString());
            widget->setCloseable(modelIndex.data(Qn::RemovableRole).toBool());
            return;
        }
    }

    // Check whether the tile is a special separator tile.
    const auto title = modelIndex.data(Qt::DisplayRole).toString();
    if (title.isEmpty())
        return;

    // Tile is a normal information tile.
    widget->setProgressBarVisible(false);
    widget->setTitle(title);
    widget->setIcon(modelIndex.data(Qt::DecorationRole).value<QPixmap>());
    widget->setTimestamp(modelIndex.data(Qn::TimestampTextRole).toString());
    widget->setDescription(tileDescription);
    widget->setFooterText(modelIndex.data(Qn::AdditionalTextRole).toString());
    widget->setAttributeList(modelIndex.data(Qn::AnalyticsAttributesRole)
        .value<analytics::AttributeList>());
    widget->setToolTip(modelIndex.data(Qt::ToolTipRole).toString());
    widget->setCloseable(modelIndex.data(Qn::RemovableRole).toBool());
    widget->setAction(modelIndex.data(Qn::CommandActionRole).value<CommandActionPtr>());
    widget->setTitleColor(modelIndex.data(Qt::ForegroundRole).value<QColor>());
    widget->setFooterEnabled(m_footersEnabled);
    widget->setHeaderEnabled(m_headersEnabled);

    setHelpTopic(widget, modelIndex.data(Qn::HelpTopicIdRole).toInt());

    const auto resourceList = modelIndex.data(Qn::DisplayedResourceListRole);
    const auto cloudSystemId = modelIndex.data(Qn::CloudSystemIdRole).toString();
    if (resourceList.isValid())
    {
        if (resourceList.canConvert<QnResourceList>())
            widget->setResourceList(resourceList.value<QnResourceList>(), cloudSystemId);
        else if (resourceList.canConvert<QStringList>())
            widget->setResourceList(resourceList.value<QStringList>(), cloudSystemId);
    }
    else
    {
        widget->setResourceList(QnResourceList(), cloudSystemId);
    }

    updateTilePreview(index);
}

void EventRibbon::Private::updateTilePreview(int index)
{
    NX_CRITICAL(index >= 0 && index < count());
    auto widget = m_tiles[index]->widget.get();
    if (!NX_ASSERT(m_model && widget))
        return;

    widget->setPreviewEnabled(m_previewsEnabled);
    if (!m_previewsEnabled)
        return;

    const auto modelIndex = m_model->index(index);

    const auto previewResource = modelIndex.data(Qn::ResourceRole).value<QnResourcePtr>();
    const auto mediaResource = previewResource.dynamicCast<QnMediaResource>();
    if (!mediaResource)
        return;

    const bool forcePreviewLoader = modelIndex.data(Qn::ForcePreviewLoaderRole).toBool();
    widget->setForcePreviewLoader(forcePreviewLoader);
    if (forcePreviewLoader)
        return;

    if (previewResource->hasFlags(Qn::ResourceFlag::fake))
    {
        widget->setPlaceholder(tr("INFORMATION REQUIRED"));
        return;
    }

    const auto defaultThumbnailWidth = headersEnabled()
        ? kDefaultThumbnailWidth
        : kAlternativeThumbnailWidth;

    const auto previewTime = modelIndex.data(Qn::PreviewTimeRole).value<microseconds>();

    const auto rotation = mediaResource->forcedRotation().value_or(0);
    const auto previewCropRect = nx::vms::client::core::Geometry::rotatedRelativeRectangle(
        modelIndex.data(Qn::ItemZoomRectRole).value<QRectF>(), -rotation / 90);

    const auto thumbnailWidth = previewCropRect.isEmpty()
        ? defaultThumbnailWidth
        : qMin<int>(defaultThumbnailWidth / previewCropRect.width(), kMaximumThumbnailWidth);

    const bool precisePreview = !previewCropRect.isEmpty()
        || modelIndex.data(Qn::ForcePrecisePreviewRole).toBool();

    const auto objectTrackId = modelIndex.data(Qn::ObjectTrackIdRole).value<QnUuid>();

    nx::api::ResourceImageRequest request;
    request.resource = previewResource;
    request.timestampUs =
        previewTime.count() > 0 ? previewTime : nx::api::ImageRequest::kLatestThumbnail;
    request.rotation = rotation;
    request.size = QSize(thumbnailWidth, 0);
    request.format = nx::api::ImageRequest::ThumbnailFormat::jpg;
    request.aspectRatio = nx::api::ImageRequest::AspectRatio::auto_;
    request.roundMethod = precisePreview
        ? nx::api::ImageRequest::RoundMethod::precise
        : nx::api::ImageRequest::RoundMethod::iFrameAfter;
    request.streamSelectionMode = modelIndex.data(Qn::PreviewStreamSelectionRole)
        .value<nx::api::ImageRequest::StreamSelectionMode>();
    request.objectTrackId = objectTrackId;

    // If previewCropRect is known, the cropping is done on the client side and a whole frame
    // preview if required. For that we must ensure the server does no cropping on its own
    // by explicitly specifying a unit rect (a zero rect means automatic cropping).
    if (!previewCropRect.isEmpty())
        request.crop = QRectF(0.0, 0.0, 1.0, 1.0); //< Whole frame.

    bool forceUpdate = true;

    auto& previewProvider = m_tiles[index]->preview;
    if (!previewProvider || request.resource != previewProvider->requestData().resource)
    {
        previewProvider.reset(createPreviewProvider(request));
    }
    else
    {
        const auto oldRequest = previewProvider->requestData();
        forceUpdate = oldRequest.timestampUs != request.timestampUs
            || oldRequest.streamSelectionMode != request.streamSelectionMode;

        previewProvider->setRequestData(request);
    }
    previewProvider->setProperty(kBypassVideoCachePropertyName,
        modelIndex.data(Qn::HasExternalBestShotRole).toBool());

    widget->setPlaceholder({});
    widget->setPreview(previewProvider.get(), forceUpdate);
    widget->setPreviewHighlightRect(previewCropRect);
}

ResourceThumbnailProvider* EventRibbon::Private::createPreviewProvider(
    const nx::api::ResourceImageRequest& request)
{
    auto provider = new ResourceThumbnailProvider(request);

    connect(provider, &QObject::destroyed, this,
        [this, provider]() { handleLoadingEnded(provider); },
        Qt::QueuedConnection);

    connect(provider, &ImageProvider::statusChanged, this,
        [this, provider](Qn::ThumbnailStatus status)
        {
            if (provider == m_providerLoadingFromCache)
                return;

            switch (status)
            {
                case Qn::ThumbnailStatus::Loading:
                case Qn::ThumbnailStatus::Refreshing:
                {
                    const QSharedPointer<QTimer> timeoutTimer(new QTimer(),
                        [](QTimer* timer)
                        {
                            if (!timer) // TODO: #vkutin Remove these lines in 4.2
                                return; // - they are totally not necessary.

                            timer->stop();
                            timer->deleteLater();
                        });

                    connect(timeoutTimer.get(), &QTimer::timeout, this,
                        [this, provider]() { handleLoadingEnded(provider); });

                    timeoutTimer->setSingleShot(true);
                    timeoutTimer->start(kPreviewLoadTimeout);

                    const auto server = previewServer(provider);
                    m_loadingByProvider.insert(provider, {server, timeoutTimer});

                    auto& serverData = m_loadingByServer[server];
                    ++serverData.loadingCounter;

                    NX_ASSERT(serverData.loadingCounter <= maxSimultaneousPreviewLoads(server));
                    break;
                }

                default:
                {
                    handleLoadingEnded(provider);
                    break;
                }
            }
        });

    return provider;
}

void EventRibbon::Private::handleLoadingEnded(ResourceThumbnailProvider* provider)
{
    const auto previewData = m_loadingByProvider.find(provider);
    if (previewData == m_loadingByProvider.end())
        return;

    const auto serverData = m_loadingByServer.find(previewData->server);
    if (NX_ASSERT(serverData != m_loadingByServer.end()))
    {
        --serverData->loadingCounter;
        NX_ASSERT(serverData->loadingCounter >= 0);
        if (serverData->loadingCounter <= 0)
            m_loadingByServer.erase(serverData);
    }

    m_loadingByProvider.erase(previewData);
    loadNextPreview();
}

bool EventRibbon::Private::isNextPreviewLoadAllowed(const ResourceThumbnailProvider* provider) const
{
    if (!NX_ASSERT(provider))
        return false;

    const auto server = previewServer(provider);
    return m_loadingByServer.value(server).loadingCounter < maxSimultaneousPreviewLoads(server)
        && (kTilePreviewLoadInterval <= 0ms || m_sinceLastPreviewRequest.hasExpired(
            kTilePreviewLoadInterval));
}

void EventRibbon::Private::ensureWidget(int index)
{
    NX_CRITICAL(index >= 0 && index < count());

    auto& widget = m_tiles[index]->widget;
    if (widget)
        return;

    if (m_reserveWidgets.empty())
    {
        widget.reset(new EventTile(m_viewport.get()));
        widget->setContextMenuPolicy(Qt::CustomContextMenu);
        widget->installEventFilter(this);

        widget->setAutomaticPreviewLoad(false);

        connect(widget.get(), &EventTile::needsPreviewLoad,
            this, &Private::loadNextPreview);

        connect(widget.get(), &EventTile::closeRequested, this,
            [this]()
            {
                const int index = indexOf(static_cast<EventTile*>(sender()));
                if (m_model && index >= 0)
                    m_model->removeRow(index);
            });

        connect(widget.get(), &EventTile::linkActivated, this,
            [this](const QString& link)
            {
                const int index = indexOf(static_cast<EventTile*>(sender()));
                if (m_model && index >= 0)
                    emit q->linkActivated(m_model->index(index), link);
            });

        connect(widget.get(), &EventTile::clicked, this,
            [this](Qt::MouseButton button, Qt::KeyboardModifiers modifiers)
            {
                const int index = indexOf(static_cast<EventTile*>(sender()));
                if (m_model && index >= 0)
                    emit q->clicked(m_model->index(index), button, modifiers);
            });

        connect(widget.get(), &EventTile::doubleClicked, this,
            [this]()
            {
                const int index = indexOf(static_cast<EventTile*>(sender()));
                if (m_model && index >= 0)
                    emit q->doubleClicked(m_model->index(index));
            });

        connect(widget.get(), &EventTile::dragStarted, this,
            [this](const QPoint& pos, const QSize& size)
            {
                const int index = indexOf(static_cast<EventTile*>(sender()));
                if (m_model && index >= 0)
                    emit q->dragStarted(m_model->index(index), pos, size);
            });

        connect(widget.get(), &QWidget::customContextMenuRequested, this,
            [this](const QPoint &pos)
            {
                const int index = indexOf(static_cast<EventTile*>(sender()));
                if (m_model && index >= 0)
                    showContextMenu(static_cast<EventTile*>(sender()), pos);
            });
    }
    else
    {
        widget = std::move(m_reserveWidgets.top());
        m_reserveWidgets.pop();
    }

    const auto deadline = m_deadlines.find(m_model->index(index));
    if (deadline != m_deadlines.end())
        deadline->setRemainingTime(kVisibleAutoCloseDelay);
}

void EventRibbon::Private::reserveWidget(int index)
{
    NX_CRITICAL(index >= 0 && index < count());

    auto& widget = m_tiles[index]->widget;
    if (!widget)
        return;

    widget->hide();
    widget->clear();
    m_reserveWidgets.emplace(widget.release());
}

void EventRibbon::Private::showContextMenu(EventTile* tile, const QPoint& posRelativeToTile)
{
    if (!m_model)
        return;

    const auto index = m_model->index(indexOf(tile));
    const auto globalPos = QnHiDpiWorkarounds::safeMapToGlobal(tile, posRelativeToTile);
    emit q->contextMenuRequested(index, globalPos, /*withStandardInteraction*/ true, q);
}

void EventRibbon::Private::closeExpiredTiles()
{
    if (!m_model)
        return;

    QList<QPersistentModelIndex> expired;
    const int oldDeadlineCount = m_deadlines.size();

    for (const auto& [index, deadline]: nx::utils::constKeyValueRange(m_deadlines))
    {
        if (index != m_hoveredIndex && deadline.hasExpired() && index.isValid())
            expired.push_back(index);
    }

    if (expired.empty())
        return;

    const auto unreadCountGuard = makeUnreadCountGuard();

    for (const auto& index: std::as_const(expired))
        m_model->removeRows(index.row(), 1);

    NX_VERBOSE(q, "Expired %1 tiles", expired.size());
    NX_ASSERT(expired.size() == (oldDeadlineCount - m_deadlines.size()));
};

nx::utils::Guard EventRibbon::Private::makeUnreadCountGuard()
{
    return nx::utils::Guard(
        [this, oldUnreadCount = m_totalUnreadCount]()
        {
            if (m_totalUnreadCount != oldUnreadCount)
                emit q->unreadCountChanged(m_totalUnreadCount, highestUnreadImportance(), {});
        });
}

int EventRibbon::Private::scrollValue() const
{
    return m_scrollBarRelevant ? m_scrollBar->value() : 0;
}

int EventRibbon::Private::totalTopMargin() const
{
    return m_viewportHeader
        ? m_topMargin + m_viewportHeader->height()
        : m_topMargin;
}

int EventRibbon::Private::calculatePosition(int index) const
{
    if (!NX_ASSERT(m_model && index >= 0))
        return 0;

    if (index == 0)
        return 0;

    const auto& previousTile = m_tiles[index - 1];
    return previousTile->position + previousTile->animatedHeight() + kDefaultTileSpacing;
}

void EventRibbon::Private::insertNewTiles(
    int index, int count, UpdateMode updateMode, bool scrollDown)
{
    if (!m_model || count == 0)
        return;

    if (!NX_ASSERT(index >= 0 && index <= this->count(), "Insertion index is out of range"))
        return;

    QnScopedValueRollback<bool> updateGuard(&m_updating, true);

    const auto position = calculatePosition(index);

    const auto unreadCountGuard = makeUnreadCountGuard();
    const bool viewportVisible = m_viewport->isVisible() && m_viewport->width() > 0;

    if (position < scrollValue())
        scrollDown = true;

    if (!viewportVisible || scrollDown || (m_scrollBarRelevant && !m_visible.contains(index)))
        updateMode = UpdateMode::instant;

    const int end = index + count;
    int currentPosition = position;

    const bool animated = updateMode == UpdateMode::animated;
    const int animatedEnd = animated
        ? qMin(index + kNumAnimatedTilesAtInsertion, end)
        : index;

    for (int i = index; i < end; ++i)
    {
        const auto modelIndex = m_model->index(i);
        const auto closeable = modelIndex.data(Qn::RemovableRole).toBool();
        const auto timeout = modelIndex.data(Qn::TimeoutRole).value<milliseconds>();

        TilePtr tile(new Tile());
        tile->position = currentPosition;
        tile->importance = modelIndex.data(Qn::NotificationLevelRole).value<Importance>();
        tile->animated = shouldAnimateTile(modelIndex);

        if (closeable && timeout > 0ms)
            m_deadlines[modelIndex] = QDeadlineTimer(kInvisibleAutoCloseDelay);

        currentPosition += kDefaultTileSpacing;
        if (!tile->animated || i >= animatedEnd)
            currentPosition += tile->height;

        if (tile->importance != Importance())
        {
            ++m_unreadCounts[int(tile->importance)];
            ++m_totalUnreadCount;
        }

        m_tiles.insert(m_tiles.begin() + i, std::move(tile));
    }

    NX_ASSERT(m_totalUnreadCount <= this->count());

    if (!m_visible.isEmpty() && index < m_visible.upper())
    {
        if (index <= m_visible.lower()) //< Insertion before visible range.
            m_visible = m_visible.shifted(count);
        else if (index < m_visible.upper()) //< Insertion inside visible range.
            m_visible = Interval(m_visible.lower(), m_visible.upper() + count);
    }

    const auto delta = currentPosition - position;

    for (int i = end; i < this->count(); ++i)
        m_tiles[i]->position += delta;

    m_endPosition += delta;
    q->updateGeometry();

    m_scrollBar->setMaximum(m_scrollBar->maximum() + delta);
    if (scrollDown)
        m_scrollBar->setValue(m_scrollBar->value() + delta);

    // Animated shift of subsequent tiles.
    for (int i = index; i < animatedEnd; ++i)
    {
        const auto& tile = m_tiles[i];
        if (!tile->animated)
            continue;

        // This is to prevent all inserted tiles appearing on the screen at once.
        static constexpr qreal kStartingFraction = 0.25;

        auto animator = new QVariantAnimation();
        static const auto kAnimationId = ui::workbench::Animations::Id::RightPanelTileInsertion;
        animator->setEasingCurve(qnWorkbenchAnimations->easing(kAnimationId));
        animator->setDuration(qnWorkbenchAnimations->timeLimit(kAnimationId));
        animator->setStartValue(kStartingFraction);
        animator->setEndValue(1.0);

        connect(animator, &QObject::destroyed, this,
            [this, animator]() { m_animations.remove(animator); });

        m_animations.insert(animator, m_model->index(i));
        tile->insertAnimation.reset(animator);
        animator->start(QAbstractAnimation::DeleteWhenStopped);
    }

    updateGuard.rollback();

    doUpdateView();

    if (animated)
    {
        const auto highlightRange = m_visible.intersected({index, index + count});
        for (int i = highlightRange.lower(); i < highlightRange.upper(); ++i)
        {
            const auto& tile = m_tiles[i];
            if (tile->animated && tile->widget)
                fadeIn(tile->widget.get());
        }
    }

    NX_VERBOSE(q, "%1 tiles inserted at position %2, new count is %3, update %4, scrollDown is %5",
        count, index, m_tiles.size(), updateMode, scrollDown);

    if (m_updating)
        return;

    emit q->countChanged(this->count());
    emit q->visibleRangeChanged(m_visible, {});
}

void EventRibbon::Private::removeTiles(int first, int count, UpdateMode updateMode)
{
    if (!NX_ASSERT(first >= 0 && count > 0 && first + count <= this->count(),
        "Removal range is invalid"))
    {
        return;
    }

    QnScopedValueRollback<bool> updateGuard(&m_updating, true);

    const auto unreadCountGuard = makeUnreadCountGuard();

    const int end = first + count;
    const Interval removalInterval(first, end);

    const bool scrollUp = !m_visible.isEmpty() && removalInterval.contains(m_visible.lower());
    const bool untilTheEnd = end == this->count();

    if (!m_visible.intersects({first, end}))
        updateMode = UpdateMode::instant;

    Interval newVisible(m_visible);
    if (!m_visible.isEmpty() && first < m_visible.upper())
    {
        if (end <= m_visible.lower())
            newVisible = m_visible.shifted(-count);
        else if (first <= m_visible.lower())
            newVisible = m_visible.truncatedLeft(end).shifted(-count);
        else if (end >= m_visible.upper())
            newVisible = m_visible.truncatedRight(first);
        else
            newVisible = Interval(m_visible.lower(), m_visible.upper() - count);
    }

    m_visible = newVisible;

    int instantDelta = 0;
    int animatedDelta = 0;
    int& delta = (updateMode == UpdateMode::animated) ? animatedDelta : instantDelta;
    delta += m_tiles[first]->position - calculatePosition(first);

    int nextPosition = untilTheEnd ? m_endPosition : m_tiles[end]->position;
    for (int index = end - 1; index >= first; --index)
    {
        const auto& tile = m_tiles[index];
        if (tile->animated && updateMode == UpdateMode::animated && tile->widget)
            fadeOut(tile->widget.release());
        else
            reserveWidget(index);

        (tile->animated ? delta : instantDelta) += nextPosition - tile->position;
        nextPosition = tile->position;

        const auto importance = tile->importance;
        if (importance != Importance())
        {
            --m_unreadCounts[int(importance)];
            --m_totalUnreadCount;
        }
    }

    m_tiles.erase(m_tiles.begin() + first, m_tiles.begin() + end);

    if (scrollUp)
        m_scrollBar->setValue(m_scrollBar->value() - (instantDelta + animatedDelta));

    if (animatedDelta > 0)
    {
        auto animator = new QVariantAnimation();
        static const auto kAnimationId = ui::workbench::Animations::Id::RightPanelTileRemoval;
        animator->setEasingCurve(qnWorkbenchAnimations->easing(kAnimationId));
        animator->setDuration(qnWorkbenchAnimations->timeLimit(kAnimationId));
        animator->setStartValue(qreal(animatedDelta));
        animator->setEndValue(0.0);

        connect(animator, &QObject::destroyed, this,
            [this, animator]() { m_animations.remove(animator); });

        if (first < this->count())
        {
            m_animations.insert(animator, m_model->index(first));
            m_tiles[first]->removeAnimation.reset(animator);
        }
        else
        {
            m_animations.insert(animator, {});
            m_endAnimation.reset(animator);
        }

        animator->start(QAbstractAnimation::DeleteWhenStopped);
    }

    updateGuard.rollback();

    doUpdateView();

    NX_VERBOSE(q, "%1 tiles removed at position %2, new count is %3, updateMode %4",
        count, first, m_tiles.size(), updateMode);

    if (m_updating)
        return;

    emit q->countChanged(this->count());
    emit q->visibleRangeChanged(m_visible, {});
}

void EventRibbon::Private::clear()
{
    const auto unreadCountGuard = makeUnreadCountGuard();
    m_unreadCounts = {};
    m_totalUnreadCount = 0;
    m_deadlines = {};

    const auto oldCount = count();

    for (int index = m_visible.lower(); index != m_visible.upper(); ++index)
        reserveWidget(index);

    m_tiles.clear();
    m_visible = {};
    m_hoveredIndex = QPersistentModelIndex();
    m_tileHovered = false;
    m_endPosition = 0;

    m_endAnimation.reset();

    setScrollBarRelevant(false);

    q->updateGeometry();

    if (oldCount <= 0)
        return;

    emit q->countChanged(0);
    emit q->visibleRangeChanged({}, {});
}

int EventRibbon::Private::indexOf(const EventTile* widget) const
{
    if (m_visible.isEmpty() || !widget)
        return -1;

    const auto iter = std::find_if(
        m_tiles.cbegin() + m_visible.lower(),
        m_tiles.cbegin() + m_visible.upper(),
        [widget](const TilePtr& tile) { return tile->widget.get() == widget; });

    return iter != m_tiles.cbegin() + m_visible.upper()
        ? std::distance(m_tiles.cbegin(), iter)
        : -1;
}

int EventRibbon::Private::totalHeight() const
{
    const int animatedShift = m_endAnimation ? m_endAnimation->currentValue().toInt() : 0;
    return totalTopMargin() + m_endPosition + animatedShift + m_bottomMargin;
}

QScrollBar* EventRibbon::Private::scrollBar() const
{
    return m_scrollBar.get();
}

bool EventRibbon::Private::showDefaultToolTips() const
{
    return m_showDefaultToolTips;
}

void EventRibbon::Private::setShowDefaultToolTips(bool value)
{
    m_showDefaultToolTips = value;
}

bool EventRibbon::Private::previewsEnabled() const
{
    return m_previewsEnabled;
}

void EventRibbon::Private::setPreviewsEnabled(bool value)
{
    if (m_previewsEnabled == value)
        return;

    m_previewsEnabled = value;

    for (int i = m_visible.lower(); i < m_visible.upper(); ++i)
        updateTilePreview(i);
}

bool EventRibbon::Private::footersEnabled() const
{
    return m_footersEnabled;
}

void EventRibbon::Private::setFootersEnabled(bool value)
{
    if (m_footersEnabled == value)
        return;

    m_footersEnabled = value;

    for (int i = m_visible.lower(); i < m_visible.upper(); ++i)
    {
        const auto& widget = m_tiles[i]->widget;
        if (NX_ASSERT(widget))
            widget->setFooterEnabled(m_footersEnabled);
    }
}

bool EventRibbon::Private::headersEnabled() const
{
    return m_headersEnabled;
}

void EventRibbon::Private::setHeadersEnabled(bool value)
{
    if (m_headersEnabled == value)
        return;

    m_headersEnabled = value;

    for (int i = m_visible.lower(); i < m_visible.upper(); ++i)
    {
        const auto& widget = m_tiles[i]->widget;
        if (NX_ASSERT(widget))
            widget->setHeaderEnabled(m_headersEnabled);
    }
}

int EventRibbon::Private::calculateHeight(QWidget* widget) const
{
    if (!NX_ASSERT(widget))
        return 0;

    return widget->hasHeightForWidth()
        ? widget->heightForWidth(m_viewport->width())
        : widget->sizeHint().expandedTo(minimumWidgetSize(widget)).height();
}

void EventRibbon::Private::setScrollBarRelevant(bool value)
{
    if (m_scrollBarRelevant == value)
        return;

    m_scrollBarRelevant = value;
    if (!m_scrollBarRelevant)
        m_scrollBar->setValue(0);

    m_scrollBar->setEnabled(m_scrollBarRelevant);
    updateScrollBarVisibility();
}

void EventRibbon::Private::updateScrollBarVisibility()
{
    switch (m_scrollBarPolicy)
    {
        case Qt::ScrollBarAlwaysOn:
            m_scrollBar->show();
            break;
        case Qt::ScrollBarAlwaysOff:
            m_scrollBar->hide();
            break;
        default:
            m_scrollBar->setVisible(m_scrollBarRelevant);
            break;
    }
}

void EventRibbon::Private::updateHighlightedTiles()
{
    if (m_visible.isEmpty())
        return;

    const auto shouldHighlightTile =
        [this](int index) -> bool
        {
            if (m_highlightedTimestamp <= 0ms || m_highlightedResources.empty())
                return false;

            const auto modelIndex = m_model->index(index);
            const auto timestamp = modelIndex.data(Qn::TimestampRole).value<microseconds>();
            if (timestamp <= 0us)
                return false;

            const auto duration = modelIndex.data(Qn::DurationRole).value<microseconds>();
            if (duration <= 0us)
                return false;

            const auto isHighlightedResource =
                [this](const QnResourcePtr& res) { return m_highlightedResources.contains(res); };

            const auto resources = modelIndex.data(Qn::ResourceListRole).value<QnResourceList>();
            if (std::none_of(resources.cbegin(), resources.cend(), isHighlightedResource))
                return false;

            return m_highlightedTimestamp >= timestamp
                && (duration.count() == QnTimePeriod::kInfiniteDuration
                    || m_highlightedTimestamp <= (timestamp + duration));
        };

    for (int i = m_visible.lower(); i < m_visible.upper(); ++i)
    {
        const auto& widget = m_tiles[i]->widget;
        if (NX_ASSERT(widget))
            widget->setHighlighted(shouldHighlightTile(i));
    }
}

Qt::ScrollBarPolicy EventRibbon::Private::scrollBarPolicy() const
{
    return m_scrollBarPolicy;
}

void EventRibbon::Private::setScrollBarPolicy(Qt::ScrollBarPolicy value)
{
    if (m_scrollBarPolicy == value)
        return;

    m_scrollBarPolicy = value;
    updateScrollBarVisibility();
}

microseconds EventRibbon::Private::highlightedTimestamp() const
{
    return m_highlightedTimestamp;
}

void EventRibbon::Private::setHighlightedTimestamp(microseconds value)
{
    if (m_highlightedTimestamp == value)
        return;

    m_highlightedTimestamp = value;
    if (!m_updating)
        updateHighlightedTiles();
}

QSet<QnResourcePtr> EventRibbon::Private::highlightedResources() const
{
    return m_highlightedResources;
}

void EventRibbon::Private::setHighlightedResources(const QSet<QnResourcePtr>& value)
{
    if (m_highlightedResources == value)
        return;

    m_highlightedResources = value;
    if (!m_updating)
        updateHighlightedTiles();
}

void EventRibbon::Private::setInsertionMode(UpdateMode updateMode, bool scrollDown)
{
    m_insertionMode = updateMode;
    m_scrollDownAfterInsertion = scrollDown;
}

void EventRibbon::Private::setRemovalMode(UpdateMode updateMode)
{
    m_removalMode = updateMode;
}

void EventRibbon::Private::updateScrollRange()
{
    const auto viewHeight = m_viewport->height();
    const auto totalHeight = this->totalHeight();
    m_scrollBar->setMaximum(qMax(totalHeight - viewHeight, 1));
    m_scrollBar->setPageStep(viewHeight);
    setScrollBarRelevant(totalHeight > viewHeight);
}

void EventRibbon::Private::setViewportMargins(int top, int bottom)
{
    if (m_topMargin == top && m_bottomMargin == bottom)
        return;

    m_topMargin = top;
    m_bottomMargin = bottom;

    updateView();
}

QWidget* EventRibbon::Private::viewportHeader() const
{
    return m_viewportHeader.data();
}

void EventRibbon::Private::setViewportHeader(QWidget* value)
{
    if (m_viewportHeader == value)
        return;

    delete m_viewportHeader;
    m_viewportHeader = value;

    if (m_viewportHeader)
        m_viewportHeader->setParent(m_viewport.get());

    updateView();
}

void EventRibbon::Private::updateView()
{
    const auto unreadCountGuard = makeUnreadCountGuard();

    const auto visibleRangeGuard = nx::utils::Guard(
        [this, oldVisibleRange = m_visible]()
        {
            if (m_visible != oldVisibleRange)
                emit q->visibleRangeChanged(m_visible, {});
        });

    doUpdateView();
}

void EventRibbon::Private::doUpdateView()
{
    if (m_updating)
        return;

    if (m_tiles.empty())
    {
        clear();
        updateHover();
        return;
    }

    if (!q->isVisible())
    {
        const auto animators = m_animations.keys();
        for (auto animator: animators)
            delete animator;

        // After insertion inside visible range it is in temporarily incorrect state:
        // widgets for newly inserted tiles are not created, it's like (w w 0 0 0 0 w w w).
        // If the ribbon is invisible, just clip the range, leaving only significant beginning.
        for (int index = m_visible.lower(); index < m_visible.upper(); ++index)
        {
            if (!m_tiles[index]->widget)
            {
                for (int i = index; i < m_visible.upper(); ++i)
                    reserveWidget(i);

                m_visible = m_visible.truncatedRight(index);
                break;
            }
        }

        updateHover();
        return;
    }

    const auto totalHeightGuard = nx::utils::Guard(
        [this, oldTotalHeight = totalHeight()]()
        {
            if (totalHeight() != oldTotalHeight)
                q->updateGeometry();
        });

    const int scrollPosition = scrollValue();
    if (m_viewportHeader)
    {
        const int headerWidth = m_viewport->width();
        const int headerHeight = m_viewportHeader->hasHeightForWidth()
            ? m_viewportHeader->heightForWidth(headerWidth)
            : m_viewportHeader->sizeHint().height();

        m_viewportHeader->setGeometry(0, m_topMargin - scrollPosition, headerWidth, headerHeight);
    }

    const int topPosition = viewportTopPosition();

    const auto secondInView = std::upper_bound(m_tiles.cbegin(), m_tiles.cend(), topPosition,
        [](int left, const TilePtr& right) { return left < right->position; });

    const int firstToUpdate = qMax<int>(0, secondInView - m_tiles.cbegin() - 1);

    const int firstAnimated =
        std::accumulate(m_animations.cbegin(), m_animations.cend(), firstToUpdate,
            [](int left, const QPersistentModelIndex& right)
            {
                return right.isValid() ? qMin(left, right.row()) : left;
            });

    for (int i = firstAnimated; i < firstToUpdate; ++i)
    {
        const auto& tile = m_tiles[i];
        tile->insertAnimation.reset();
        tile->removeAnimation.reset();
        tile->position = calculatePosition(i);
    }

    const auto positionLimit = topPosition + m_viewport->height();

    static constexpr int kWidthThreshold = 400;
    const auto mode = m_viewport->width() > kWidthThreshold
        ? EventTile::Mode::wide
        : EventTile::Mode::standard;

    int index = firstToUpdate;
    int firstVisible = firstToUpdate;

    for (; index < count(); ++index)
    {
        const int currentPosition = calculatePosition(index);
        if (currentPosition >= positionLimit)
            break;

        const auto& tile = m_tiles[index];
        tile->position = currentPosition + tile->animatedShift();

        if (!tile->widget)
            updateTile(index);

        tile->widget->setMode(mode);
        // Set geometry before show() to avoid visual artifacts.
        tile->widget->setGeometry(0, tile->position - topPosition, 1, 1);
        // For calculateHeight() to work correctly the widget should be visible.
        tile->widget->show();
        tile->widget->raise();
        tile->height = calculateHeight(tile->widget.get());
        tile->widget->resize(m_viewport->width(), tile->height);

        if (tile->widget->geometry().bottom() <= 0)
        {
            ++firstVisible;
            reserveWidget(index);
            tile->insertAnimation.reset();
            tile->removeAnimation.reset();
            tile->position = currentPosition;
        }
        else if (tile->importance != Importance())
        {
            --m_totalUnreadCount;
            --m_unreadCounts[int(tile->importance)];
            tile->importance = Importance();
        }
    }

    Interval newVisible(firstVisible, index);

    for (; index < count(); ++index)
    {
        const auto& tile = m_tiles[index];
        tile->insertAnimation.reset();
        tile->position = calculatePosition(index);
    }

    for (int i = m_visible.lower(); i < m_visible.upper(); ++i)
    {
        if (!newVisible.contains(i))
            reserveWidget(i);
    }

    m_endPosition = calculatePosition(count());
    m_visible = newVisible;
    m_viewport->update();

    updateScrollRange();
    updateHover();
    updateHighlightedTiles();

    if (!m_animations.empty())
        m_layoutUpdate->requestOperation();
}

void EventRibbon::Private::fadeIn(EventTile* widget)
{
    static const auto kAnimationId = ui::workbench::Animations::Id::RightPanelTileFadeIn;
    auto animator = new QVariantAnimation(this);
    animator->setStartValue(1.0);
    animator->setEndValue(0.0);
    animator->setEasingCurve(qnWorkbenchAnimations->easing(kAnimationId));
    animator->setDuration(qnWorkbenchAnimations->timeLimit(kAnimationId));

    createFadeCurtain(widget, animator);
    animator->start(QAbstractAnimation::DeleteWhenStopped);
}

void EventRibbon::Private::fadeOut(EventTile* widget)
{
    static const auto kAnimationId = ui::workbench::Animations::Id::RightPanelTileFadeOut;
    auto animator = new QVariantAnimation(this);
    animator->setStartValue(0.0);
    animator->setEndValue(1.0);
    animator->setEasingCurve(qnWorkbenchAnimations->easing(kAnimationId));
    animator->setDuration(qnWorkbenchAnimations->timeLimit(kAnimationId));

    connect(animator, &QObject::destroyed, widget,
        [this, widget]()
        {
            widget->hide();
            widget->clear();
            setReadOnly(widget, false);
            m_reserveWidgets.emplace(widget);
        });

    const int base = widget->y() + m_scrollBar->value();

    connect(m_scrollBar.get(), &QScrollBar::valueChanged, animator, nx::utils::guarded(widget,
        [widget, base](int value) { widget->move(widget->x(), base - value); }));

    createFadeCurtain(widget, animator);
    setReadOnly(widget, true);

    animator->start(QAbstractAnimation::DeleteWhenStopped);
}

QWidget* EventRibbon::Private::createFadeCurtain(EventTile* widget, QVariantAnimation* animator)
{
    if (!NX_ASSERT(widget && animator))
        return nullptr;

    auto curtain = new CustomPainted<QWidget>(widget);
    anchorWidgetToParent(curtain);

    const auto color = colorTheme()->color(kFadeCurtainColorName);

    curtain->setCustomPaintFunction(nx::utils::guarded(animator, true,
        [animator, curtain, color](QPainter* painter, const QStyleOption* /*option*/,
            const QWidget* /*widget*/)
        {
            const auto currentColor = toTransparent(color, animator->currentValue().toReal());
            painter->fillRect(curtain->rect(), currentColor);
            return true;
        }));

    connect(animator, &QVariantAnimation::valueChanged, curtain, qOverload<>(&QWidget::update));
    connect(animator, &QObject::destroyed, curtain, &QObject::deleteLater);
    installEventHandler(curtain, QEvent::Hide, animator, &QAbstractAnimation::stop);

    curtain->show();
    return curtain;
}

int EventRibbon::Private::count() const
{
    return int(m_tiles.size());
}

int EventRibbon::Private::unreadCount() const
{
    return m_totalUnreadCount;
}

nx::utils::Interval<int> EventRibbon::Private::visibleRange() const
{
    return m_visible;
}

QnNotificationLevel::Value EventRibbon::Private::highestUnreadImportance() const
{
    if (m_totalUnreadCount == 0)
        return Importance();

    for (int i = int(Importance::LevelCount) - 1; i >= 0; --i)
    {
        if (m_unreadCounts[i] > 0)
            return Importance(i);
    }

    NX_ASSERT(false);
    return Importance();
}

void EventRibbon::Private::updateHover()
{
    const bool hoverEnabled = !qApp->activeModalWidget() && !qApp->activePopupWidget();
    const auto pos = WidgetUtils::mapFromGlobal(q, QCursor::pos());
    if (q->isVisible() && q->rect().contains(pos) && hoverEnabled)
    {
        const int index = indexAtPos(pos);
        if ((index < 0 && !m_tileHovered) || (index >= 0 && m_hoveredIndex.row() == index))
            return;

        if (m_hoveredIndex.isValid() && m_deadlines.contains(m_hoveredIndex))
            m_deadlines[m_hoveredIndex].setRemainingTime(kVisibleAutoCloseDelay);

        m_tileHovered = index >= 0 && NX_ASSERT(m_model);
        if (m_tileHovered)
        {
            const auto widget = m_tiles[index]->widget.get();
            NX_ASSERT(widget);

            m_hoveredIndex = m_model->index(index);
            emit q->hovered(m_hoveredIndex, widget);
        }
        else
        {
            m_hoveredIndex = QPersistentModelIndex();
            emit q->hovered(QModelIndex(), nullptr);
        }
    }
    else
    {
        if (!m_tileHovered)
            return;

        m_tileHovered = false;
        m_hoveredIndex = QPersistentModelIndex();
        emit q->hovered(QModelIndex(), nullptr);
    }
}

bool EventRibbon::Private::eventFilter(QObject* object, QEvent* event)
{
    const auto tile = qobject_cast<EventTile*>(object);
    if (!tile || tile->parent() != m_viewport.get())
        return QObject::eventFilter(object, event);

    bool ignoreEvent = false;
    switch (event->type())
    {
        case QEvent::Enter:
        case QEvent::Leave:
            ignoreEvent = qApp->activePopupWidget() || qApp->activeModalWidget();
            break;
        case QEvent::ToolTip:
            ignoreEvent = !m_showDefaultToolTips;
            break;
        default:
            break;
    }

    if (ignoreEvent)
    {
        event->ignore();
        return true;
    }

    return QObject::eventFilter(object, event);
}

int EventRibbon::Private::indexAtPos(const QPoint& pos) const
{
    if (m_visible.isEmpty())
        return -1;

    const auto viewportPos = m_viewport->mapFrom(q, pos);

    const auto begin = std::make_reverse_iterator(m_tiles.cbegin() + m_visible.upper());
    const auto end = std::make_reverse_iterator(m_tiles.cbegin() + m_visible.lower());

    const auto iter = std::find_if(begin, end,
        [&viewportPos](const TilePtr& tile) -> bool
        {
            return tile->widget && tile->widget->geometry().contains(viewportPos);
        });

    return iter != end
        ? m_visible.lower() + std::distance(iter, end) - 1
        : -1;
}

void EventRibbon::Private::loadNextPreview()
{
    if (!m_previewsEnabled)
        return;

    for (int i = m_visible.lower(); i < m_visible.upper(); ++i)
    {
        const auto& tile = m_tiles[i];
        if (!tile->widget || !tile->widget->isPreviewLoadNeeded() || !NX_ASSERT(tile->preview))
            continue;

        if (!tile->preview->property(kBypassVideoCachePropertyName).toBool())
        {
            QScopedValueRollback<ResourceThumbnailProvider*> loadingFromCacheGuard(
                m_providerLoadingFromCache, tile->preview.get());

            if (tile->preview->tryLoad())
            {
                NX_VERBOSE(this, "Loaded preview from videocache (timestamp=%1, objectTrackId=%2",
                    tile->preview->requestData().timestampUs,
                    tile->preview->requestData().objectTrackId);

                continue;
            }
        }

        if (isNextPreviewLoadAllowed(tile->preview.get()))
        {
            NX_VERBOSE(this, "Requesting preview from server (timestamp=%1, objectTrackId=%2",
                tile->preview->requestData().timestampUs,
                tile->preview->requestData().objectTrackId);

            tile->preview->loadAsync();
            m_sinceLastPreviewRequest.restart();
        }
        else if (m_loadingByProvider.empty())
        {
            m_previewLoad->requestOperation();
            break;
        }
    }
}

void EventRibbon::Private::ensureVisible(int row)
{
    if (!NX_ASSERT(row >= 0 && row <= m_tiles.size()) || !m_scrollBarRelevant)
        return;

    const auto& tile = m_tiles[row];
    m_scrollBar->setValue(tile->position + totalTopMargin());
}

} // namespace nx::vms::client::desktop
