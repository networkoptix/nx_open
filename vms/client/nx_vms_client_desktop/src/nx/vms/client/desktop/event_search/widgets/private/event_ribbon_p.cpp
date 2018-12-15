#include "event_ribbon_p.h"

#include <algorithm>
#include <chrono>

#include <QtCore/QAbstractListModel>
#include <QtCore/QVariantAnimation>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QMenu>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QApplication>

#include <client/client_globals.h>
#include <core/resource/media_resource.h>
#include <recording/time_period.h>
#include <ui/common/notification_levels.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/style/nx_style_p.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <utils/common/event_processors.h>
#include <utils/common/scoped_value_rollback.h>

#include <nx/api/mediaserver/image_request.h>
#include <nx/vms/client/desktop/common/utils/custom_painted.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/event_search/widgets/event_tile.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/range_adapters.h>
#include <nx/utils/scoped_connections.h>

namespace nx::vms::client::desktop {

using namespace std::chrono;

namespace {

static constexpr int kDefaultTileSpacing = 1;
static constexpr int kScrollBarStep = 16;

static constexpr int kDefaultThumbnailWidth = 224;
static constexpr int kMaximumThumbnailWidth = 1024;

static constexpr auto kHighlightCurtainColor = Qt::white;
static constexpr qreal kHighlightCurtainOpacity = 0.25;
static constexpr milliseconds kHighlightDuration = 400ms;
static constexpr milliseconds kAnimationDuration = 250ms;

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

QSize minimumWidgetSize(QWidget* widget)
{
    return widget->minimumSizeHint()
        .expandedTo(widget->minimumSize())
        .expandedTo(QApplication::globalStrut());
}

bool shouldAnimateTile(const QModelIndex& index)
{
    return !index.data(Qt::DisplayRole).toString().isEmpty();
}

} // namespace

EventRibbon::Private::Private(EventRibbon* q):
    q(q),
    m_scrollBar(new QScrollBar(Qt::Vertical, q)),
    m_viewport(new QWidget(q)),
    m_autoCloseTimer(new QTimer())
{
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

    NX_ASSERT(Importance() == Importance::NoNotification);
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

    insertNewTiles(0, m_model->rowCount(), UpdateMode::instant);

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
            insertNewTiles(0, m_model->rowCount(), UpdateMode::instant);
        });

    m_modelConnections << connect(m_model, &QAbstractListModel::rowsInserted, this,
        [this](const QModelIndex& /*parent*/, int first, int last)
        {
            insertNewTiles(first, last - first + 1, UpdateMode::animated);
            NX_ASSERT(m_model->rowCount() == count());
        });

    m_modelConnections << connect(m_model, &QAbstractListModel::rowsAboutToBeRemoved, this,
        [this](const QModelIndex& /*parent*/, int first, int last)
        {
            for (int index = first; index <= last; ++index)
                handleItemAboutToBeRemoved(index);
        });

    m_modelConnections << connect(m_model, &QAbstractListModel::rowsRemoved, this,
        [this](const QModelIndex& /*parent*/, int first, int last)
        {
            removeTiles(first, last - first + 1, UpdateMode::animated);
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
            insertNewTiles(index, movedCount, UpdateMode::instant);

            updateGuard.rollback();
            updateView();

            NX_ASSERT(m_model->rowCount() == count());
        });
}

void EventRibbon::Private::updateTile(int index)
{
    NX_CRITICAL(index >= 0 && index < count());
    NX_ASSERT(m_model);
    if (!m_model)
        return;

    const auto modelIndex = m_model->index(index);
    ensureWidget(index);

    auto widget = m_tiles[index]->widget.get();
    NX_ASSERT(widget);
    if (!widget)
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

    // Check whether the tile is a special progress bar tile.
    const auto progress = modelIndex.data(Qn::ProgressValueRole);
    if (progress.canConvert<qreal>())
    {
        widget->setProgressBarVisible(true);
        widget->setProgressValue(progress.value<qreal>());
        widget->setProgressTitle(modelIndex.data(Qt::DisplayRole).toString());
        widget->setDescription(modelIndex.data(Qn::DescriptionTextRole).toString());
        widget->setToolTip(modelIndex.data(Qn::DescriptionTextRole).toString());
        return;
    }

    // Check whether the tile is a special separator tile.
    const auto title = modelIndex.data(Qt::DisplayRole).toString();
    if (title.isEmpty())
        return;

    // Tile is a normal information tile.
    widget->setTitle(title);
    widget->setIcon(modelIndex.data(Qt::DecorationRole).value<QPixmap>());
    widget->setTimestamp(modelIndex.data(Qn::TimestampTextRole).toString());
    widget->setDescription(modelIndex.data(Qn::DescriptionTextRole).toString());
    widget->setFooterText(modelIndex.data(Qn::AdditionalTextRole).toString());
    widget->setToolTip(modelIndex.data(Qt::ToolTipRole).toString());
    widget->setCloseable(modelIndex.data(Qn::RemovableRole).toBool());
    widget->setAction(modelIndex.data(Qn::CommandActionRole).value<CommandActionPtr>());
    widget->setTitleColor(modelIndex.data(Qt::ForegroundRole).value<QColor>());
    widget->setFooterEnabled(m_footersEnabled);
    widget->setHeaderEnabled(m_headersEnabled);

    setHelpTopic(widget, modelIndex.data(Qn::HelpTopicIdRole).toInt());

    const auto resourceList = modelIndex.data(Qn::ResourceListRole);
    if (resourceList.isValid())
    {
        if (resourceList.canConvert<QnResourceList>())
            widget->setResourceList(resourceList.value<QnResourceList>());
        else if (resourceList.canConvert<QStringList>())
            widget->setResourceList(resourceList.value<QStringList>());
    }

    updateTilePreview(index);
}

void EventRibbon::Private::updateTilePreview(int index)
{
    NX_CRITICAL(index >= 0 && index < count());
    NX_ASSERT(m_model);

    auto widget = m_tiles[index]->widget.get();
    NX_ASSERT(widget);

    if (!m_model || !widget)
        return;

    widget->setPreviewEnabled(m_previewsEnabled);
    if (!m_previewsEnabled)
        return;

    const auto modelIndex = m_model->index(index);

    const auto previewResource = modelIndex.data(Qn::ResourceRole).value<QnResourcePtr>();
    if (!previewResource.dynamicCast<QnMediaResource>())
        return;

    const auto previewTime = modelIndex.data(Qn::PreviewTimeRole).value<microseconds>();
    const auto previewCropRect = modelIndex.data(Qn::ItemZoomRectRole).value<QRectF>();
    const auto thumbnailWidth = previewCropRect.isEmpty()
        ? kDefaultThumbnailWidth
        : qMin<int>(kDefaultThumbnailWidth / previewCropRect.width(), kMaximumThumbnailWidth);

    const bool precisePreview = !previewCropRect.isEmpty()
        || modelIndex.data(Qn::ForcePrecisePreviewRole).toBool();

    nx::api::ResourceImageRequest request;
    request.resource = previewResource;
    request.usecSinceEpoch =
        previewTime.count() > 0 ? previewTime.count() : nx::api::ImageRequest::kLatestThumbnail;
    request.rotation = nx::api::ImageRequest::kDefaultRotation;
    request.size = QSize(thumbnailWidth, 0);
    request.imageFormat = nx::api::ImageRequest::ThumbnailFormat::jpg;
    request.aspectRatio = nx::api::ImageRequest::AspectRatio::source;
    request.roundMethod = precisePreview
        ? nx::api::ImageRequest::RoundMethod::precise
        : nx::api::ImageRequest::RoundMethod::iFrameAfter;

    auto& previewProvider = m_tiles[index]->preview;
    if (previewProvider && (request.resource != previewProvider->requestData().resource
        || request.usecSinceEpoch != previewProvider->requestData().usecSinceEpoch))
    {
        previewProvider.reset();
    }

    if (!previewProvider)
        previewProvider.reset(new ResourceThumbnailProvider(request));

    widget->setPreview(previewProvider.get());
    widget->setPreviewCropRect(previewCropRect);
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

    widget->show();
    handleWidgetChanged(index);
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
    handleWidgetChanged(index);
}

void EventRibbon::Private::showContextMenu(EventTile* tile, const QPoint& posRelativeToTile)
{
    if (!m_model)
        return;

    const auto index = m_model->index(indexOf(tile));
    const auto menu = index.data(Qn::ContextMenuRole).value<QSharedPointer<QMenu>>();

    if (!menu)
        return;

    menu->setWindowFlags(menu->windowFlags() | Qt::BypassGraphicsProxyWidget);

    const auto globalPos = QnHiDpiWorkarounds::safeMapToGlobal(tile, posRelativeToTile);
    menu->exec(globalPos);
}

void EventRibbon::Private::handleItemAboutToBeRemoved(int index)
{
    m_deadlines.remove(m_model->index(index));

    if (m_hoveredIndex.row() == index)
        m_hoveredIndex = QPersistentModelIndex();
}

void EventRibbon::Private::handleWidgetChanged(int index)
{
    if (!m_tiles[index]->widget)
        return;

    const auto deadline = m_deadlines.find(m_model->index(index));
    if (deadline != m_deadlines.end())
        deadline->setRemainingTime(kVisibleAutoCloseDelay);
}

void EventRibbon::Private::closeExpiredTiles()
{
    if (!m_model)
        return;

    QList<QPersistentModelIndex> expired;
    const int oldDeadlineCount = m_deadlines.size();

    for (const auto& [index, deadline]: nx::utils::keyValueRange(m_deadlines))
    {
        if (index != m_hoveredIndex && deadline.hasExpired() && index.isValid())
            expired.push_back(index);
    }

    if (expired.empty())
        return;

    const auto unreadCountGuard = makeUnreadCountGuard();

    for (const auto index: expired)
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

void EventRibbon::Private::insertNewTiles(int index, int count, UpdateMode updateMode)
{
    if (!m_model || count == 0)
        return;

    if (index < 0 || index > this->count())
    {
        NX_ASSERT(false, Q_FUNC_INFO, "Insertion index is out of range");
        return;
    }

    QnScopedValueRollback<bool> updateGuard(&m_updating, true);

    const auto position = (index > 0)
        ? m_tiles[index - 1]->position + m_tiles[index - 1]->height + kDefaultTileSpacing
        : 0;

    const auto unreadCountGuard = makeUnreadCountGuard();
    const bool viewportVisible = m_viewport->isVisible() && m_viewport->width() > 0;

    if (!viewportVisible)
        updateMode = UpdateMode::instant;

    const int endIndex = index + count;
    int currentPosition = position;

    for (int i = index; i < endIndex; ++i)
    {
        const auto modelIndex = m_model->index(i);
        const auto closeable = modelIndex.data(Qn::RemovableRole).toBool();
        const auto timeout = modelIndex.data(Qn::TimeoutRole).value<milliseconds>();
        const auto importance = modelIndex.data(Qn::NotificationLevelRole).value<Importance>();

        TilePtr tile(new Tile());
        tile->position = currentPosition;
        tile->importance = importance;
        tile->animated = shouldAnimateTile(modelIndex);

        if (closeable && timeout > 0ms)
            m_deadlines[modelIndex] = QDeadlineTimer(kInvisibleAutoCloseDelay);

        m_tiles.insert(m_tiles.begin() + i, std::move(tile));
        currentPosition += kApproximateTileHeight + kDefaultTileSpacing;

        if (importance != Importance())
        {
            ++m_unreadCounts[int(importance)];
            ++m_totalUnreadCount;
        }
    }

    NX_ASSERT(m_totalUnreadCount <= this->count());

    if (!m_visible.isEmpty() && index < m_visible.upper())
    {
        if (index <= m_visible.lower())
            m_visible = m_visible.shifted(count);
        else if (index < m_visible.upper())
            m_visible = Interval(m_visible.lower(), m_visible.upper() + count);
    }

    const auto delta = currentPosition - position;

    for (int i = endIndex; i < this->count(); ++i)
        m_tiles[i]->position += delta;

    m_totalHeight += delta;
    q->updateGeometry();

    m_scrollBar->setMaximum(m_scrollBar->maximum() + delta);

    const int kThreshold = 100;
    const int kLiveThreshold = 10;

    const bool live = m_live && index == 0 && m_scrollBar->value() < kLiveThreshold;

    if (!live && position < m_scrollBar->value() + kThreshold)
    {
        m_scrollBar->setValue(m_scrollBar->value() + delta);
        updateMode = UpdateMode::instant;
    }

    // Correct current animations.
    for (auto& animatedIndex: m_itemShiftAnimations)
    {
        if (animatedIndex >= index)
            animatedIndex += count;
    }

    // Animated shift of subsequent tiles.
    if (updateMode == UpdateMode::animated && endIndex < this->count())
    {
        if (m_tiles[endIndex - 1]->animated)
            addAnimatedShift(endIndex, -m_tiles[endIndex - 1]->height);
    }

    updateGuard.rollback();

    doUpdateView();

    if (updateMode == UpdateMode::animated)
    {
        const auto highlightRange = m_visible.intersected({index, index + count});
        for (int i = highlightRange.lower(); i < highlightRange.upper(); ++i)
        {
            if (m_tiles[i]->animated)
               highlightAppearance(m_tiles[i]->widget.get());
        }
    }

    NX_VERBOSE(q, "%1 tiles inserted at position %2, new count is %3", count, index, m_tiles.size());

    if (m_updating)
        return;

    emit q->countChanged(this->count());
    emit q->visibleRangeChanged(m_visible, {});
}

void EventRibbon::Private::removeTiles(int first, int count, UpdateMode updateMode)
{
    NX_ASSERT(count);
    if (count == 0)
        return;

    if (first < 0 || count < 0 || first + count > this->count())
    {
        NX_ASSERT(false, Q_FUNC_INFO, "Removal range is invalid");
        return;
    }

    QnScopedValueRollback<bool> updateGuard(&m_updating, true);

    const auto unreadCountGuard = makeUnreadCountGuard();

    const int end = first + count;
    const int last = end - 1;
    const int nextPosition = m_tiles[last]->position + m_tiles[last]->height + kDefaultTileSpacing;

    if (end == this->count())
        updateMode = UpdateMode::instant;

    int delta = 0;
    const bool topmostTileWasVisible = m_visible.contains(first);

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

    for (int i = first; i < end; ++i)
    {
        reserveWidget(i);
        delta += m_tiles[i]->height + kDefaultTileSpacing;

        const auto importance = m_tiles[i]->importance;
        if (importance != Importance())
        {
            --m_unreadCounts[int(importance)];
            --m_totalUnreadCount;
        }
    }

    m_tiles.erase(m_tiles.begin() + first, m_tiles.begin() + end);

    for (int i = first; i < this->count(); ++i)
        m_tiles[i]->position -= delta;

    m_totalHeight -= delta;

    if (nextPosition < m_scrollBar->value())
        m_scrollBar->setValue(m_scrollBar->value() - delta);

    // Correct current animations.
    for (auto& animatedIndex: m_itemShiftAnimations)
    {
        if (animatedIndex > first)
            animatedIndex = qMax(first, animatedIndex - count);
    }

    if (first != this->count())
    {
        if (first == 0)
            m_tiles[0]->position = 0; //< Keep integrity: positions must start from 0.

        // In case of several tiles removing, animate only the topmost tile collapsing.
        if (topmostTileWasVisible && updateMode == UpdateMode::animated && m_tiles[first]->animated)
            addAnimatedShift(first, delta);
    }

    updateGuard.rollback();

    doUpdateView();

    NX_VERBOSE(q, "%1 tiles removed at position %2, new count is %3", count, first, m_tiles.size());

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
    m_totalHeight = 0;
    m_live = true;

    clearShiftAnimations();
    setScrollBarRelevant(false);

    q->updateGeometry();

    if (oldCount <= 0)
        return;

    emit q->countChanged(0);
    emit q->visibleRangeChanged({}, {});
}

void EventRibbon::Private::clearShiftAnimations()
{
    for (auto animator: m_itemShiftAnimations.keys())
    {
        animator->stop();
        animator->deleteLater();
    }

    m_itemShiftAnimations.clear();
    m_currentShifts.clear();
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
    return m_totalHeight;
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
    NX_ASSERT(widget);
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
        NX_ASSERT(widget);
        if (widget)
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
    updateHighlightedTiles();
}

bool EventRibbon::Private::live() const
{
    return m_live;
}

void EventRibbon::Private::setLive(bool value)
{
    m_live = value;
}

void EventRibbon::Private::updateScrollRange()
{
    const auto viewHeight = m_viewport->height();
    m_scrollBar->setMaximum(qMax(m_totalHeight - viewHeight, 1));
    m_scrollBar->setPageStep(viewHeight);
    setScrollBarRelevant(m_totalHeight > viewHeight);
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
        clearShiftAnimations();
        updateHover();
        return;
    }

    updateCurrentShifts();

    const int scrollPosition = m_scrollBarRelevant ? m_scrollBar->value() : 0;
    const int height = m_viewport->height();

    const auto secondInView = std::upper_bound(m_tiles.cbegin(), m_tiles.cend(), scrollPosition,
        [](int left, const TilePtr& right) { return left < right->position; });

    int firstIndexToUpdate = qMax<int>(0, secondInView - m_tiles.cbegin() - 1);

    if (!m_currentShifts.empty())
        firstIndexToUpdate = qMin(firstIndexToUpdate, m_currentShifts.begin().key());

    int currentPosition = m_topMargin;
    if (m_viewportHeader)
    {
        const int headerWidth = m_viewport->width();
        const int headerHeight = m_viewportHeader->hasHeightForWidth()
            ? m_viewportHeader->heightForWidth(headerWidth)
            : m_viewportHeader->sizeHint().height();

        m_viewportHeader->setGeometry(0, m_topMargin - scrollPosition, headerWidth, headerHeight);
        currentPosition += m_viewportHeader->height();
    }

    if (firstIndexToUpdate > 0)
    {
        const auto& prevTile = m_tiles[firstIndexToUpdate - 1];
        currentPosition = prevTile->position + prevTile->height + kDefaultTileSpacing;
    }

    const auto positionLimit = scrollPosition + height;

    static constexpr int kWidthThreshold = 400;
    const auto mode = m_viewport->width() > kWidthThreshold
        ? EventTile::Mode::wide
        : EventTile::Mode::standard;

    auto iter = m_tiles.cbegin() + firstIndexToUpdate;
    int firstVisible = firstIndexToUpdate;

    while (iter != m_tiles.end() && currentPosition < positionLimit)
    {
        const auto& tile = *iter;
        currentPosition += m_currentShifts.value(iter - m_tiles.cbegin());
        tile->position = currentPosition;
        if (!tile->widget)
            updateTile(iter - m_tiles.cbegin());

        tile->height = calculateHeight(tile->widget.get());
        tile->widget->setGeometry(
            0, currentPosition - scrollPosition, m_viewport->width(), tile->height);
        tile->widget->setMode(mode);
        const auto bottom = currentPosition + tile->height;
        currentPosition = bottom + kDefaultTileSpacing;

        if (bottom <= 0)
        {
            ++firstVisible;
            reserveWidget(iter - m_tiles.cbegin());
        }
        else
        {
            if (tile->importance != Importance())
            {
                --m_totalUnreadCount;
                --m_unreadCounts[int(tile->importance)];
                tile->importance = Importance();
            }
        }

        ++iter;
    }

    Interval newVisible(firstVisible, iter - m_tiles.cbegin());

    while (iter != m_tiles.end())
    {
        currentPosition += m_currentShifts.value(iter - m_tiles.cbegin());
        (*iter)->position = currentPosition;
        currentPosition += (*iter)->height + kDefaultTileSpacing;
        ++iter;
    }

    currentPosition += m_bottomMargin;

    if (m_totalHeight != currentPosition)
    {
        m_totalHeight = currentPosition;
        q->updateGeometry();
    }

    for (int i = firstIndexToUpdate; i < firstVisible; ++i)
        reserveWidget(i);

    for (int i = m_visible.lower(); i < m_visible.upper(); ++i)
    {
        if (!newVisible.contains(i))
            reserveWidget(i);
    }

    m_visible = newVisible;
    m_viewport->update();

    updateScrollRange();
    updateHover();
    updateHighlightedTiles();

    if (!m_currentShifts.empty()) //< If has running animations.
        qApp->postEvent(m_viewport.get(), new QEvent(QEvent::LayoutRequest));
}

void EventRibbon::Private::highlightAppearance(EventTile* tile)
{
    if (!tile->isVisible())
        return;

    auto animation = new QVariantAnimation(tile);
    animation->setStartValue(kHighlightCurtainOpacity);
    animation->setEndValue(0.0);
    animation->setDuration(kHighlightDuration.count());
    animation->setEasingCurve(QEasingCurve::InCubic);

    auto curtain = new CustomPainted<QWidget>(tile);
    anchorWidgetToParent(curtain);

    curtain->setCustomPaintFunction(nx::utils::guarded(animation, true,
        [animation, curtain](QPainter* painter, const QStyleOption* /*option*/,
            const QWidget* /*widget*/)
        {
            QColor color = kHighlightCurtainColor;
            color.setAlphaF(animation->currentValue().toReal());
            painter->fillRect(curtain->rect(), color);
            curtain->update();
            return true;
        }));

    connect(animation, &QObject::destroyed, curtain, &QObject::deleteLater);
    installEventHandler(curtain, QEvent::Hide, animation, &QAbstractAnimation::stop);

    animation->start(QAbstractAnimation::DeleteWhenStopped);
    curtain->setVisible(true);
}

void EventRibbon::Private::addAnimatedShift(int index, int shift)
{
    if (shift == 0)
        return;

    auto animator = new QVariantAnimation(this);
    animator->setStartValue(qreal(shift));
    animator->setEndValue(0.0);
    animator->setEasingCurve(QEasingCurve::OutCubic);
    animator->setDuration(kAnimationDuration.count());

    connect(animator, &QObject::destroyed, this,
        [this, animator]() { m_itemShiftAnimations.remove(animator); });

    m_itemShiftAnimations[animator] = index;
    animator->start(QAbstractAnimation::DeleteWhenStopped);
}

void EventRibbon::Private::updateCurrentShifts()
{
    m_currentShifts.clear();

    for (auto iter = m_itemShiftAnimations.begin(); iter != m_itemShiftAnimations.end(); ++iter)
    {
        const auto shift = iter.key()->currentValue().toInt();
        if (shift != 0)
            m_currentShifts[iter.value()] += shift;
    }
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
    const auto pos = WidgetUtils::mapFromGlobal(q, QCursor::pos());
    if (q->rect().contains(pos))
    {
        const int index = indexAtPos(pos);

        auto tile = index >= 0 ? m_tiles[index].get() : nullptr;
        const auto widget = tile ? tile->widget.get() : nullptr;

        NX_ASSERT(!tile || widget);
        if (tile && !widget)
            tile = nullptr;

        if ((!m_hoveredIndex.isValid() && index < 0) || m_hoveredIndex.row() == index)
            return;

        if (m_hoveredIndex.isValid() && m_deadlines.contains(m_hoveredIndex))
            m_deadlines[m_hoveredIndex].setRemainingTime(kVisibleAutoCloseDelay);

        if (index < 0)
        {
            m_hoveredIndex = QPersistentModelIndex();
            emit q->hovered(QModelIndex(), nullptr);
        }
        else if (NX_ASSERT(m_model))
        {
            m_hoveredIndex = m_model->index(index);
            emit q->hovered(m_hoveredIndex, widget);
        }
    }
    else
    {
        if (!m_hoveredIndex.isValid())
            return;

        m_hoveredIndex = QPersistentModelIndex();
        emit q->hovered(QModelIndex(), nullptr);
    }
}

bool EventRibbon::Private::eventFilter(QObject* object, QEvent* event)
{
    const auto tile = qobject_cast<EventTile*>(object);
    if (!tile || tile->parent() != m_viewport.get())
        return QObject::eventFilter(object, event);

    if (m_showDefaultToolTips || event->type() != QEvent::ToolTip)
        return QObject::eventFilter(object, event);

    event->ignore();
    return true; //< Ignore tooltip events.
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

} // namespace nx::vms::client::desktop
