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
#include <core/resource/camera_resource.h>
#include <ui/common/notification_levels.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/style/helper.h>
#include <ui/style/nx_style_p.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <utils/common/event_processors.h>

#include <nx/api/mediaserver/image_request.h>
#include <nx/vms/client/desktop/common/utils/custom_painted.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/event_search/widgets/event_tile.h>
#include <nx/vms/client/desktop/ui/actions/action.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>

#include <nx/vms/client/desktop/ini.h>

namespace nx::vms::client::desktop {

namespace {

static constexpr int kDefaultTileSpacing = 1;
static constexpr int kScrollBarStep = 16;

static constexpr int kDefaultThumbnailWidth = 224;
static constexpr int kMaximumThumbnailWidth = 1024;

static const auto kHighlightCurtainColor = QColor(Qt::white);
static const qreal kHighlightCurtainOpacity = 0.25;
static const auto kHighlightDuration = std::chrono::milliseconds(400);
static const auto kAnimationDuration = std::chrono::milliseconds(250);

QSize minimumWidgetSize(QWidget* widget)
{
    return widget->minimumSizeHint()
        .expandedTo(widget->minimumSize())
        .expandedTo(QApplication::globalStrut());
}

} // namespace

EventRibbon::Private::Private(EventRibbon* q):
    QnWorkbenchContextAware(q),
    q(q),
    m_scrollBar(new QScrollBar(Qt::Vertical, q)),
    m_viewport(new QWidget(q))
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

    NX_ASSERT(Importance() == Importance::NoNotification);
}

EventRibbon::Private::~Private()
{
    m_modelConnections.reset();
}

QAbstractListModel* EventRibbon::Private::model() const
{
    return m_model;
}

void EventRibbon::Private::setModel(QAbstractListModel* model)
{
    if (m_model == model)
        return;

    m_modelConnections.reset();
    clear();

    m_model = model;

    if (!m_model)
        return;

    insertNewTiles(0, m_model->rowCount(), UpdateMode::instant);

    m_modelConnections.reset(new QnDisconnectHelper());

    *m_modelConnections << connect(m_model, &QObject::destroyed, this,
        [this]()
        {
            m_model = nullptr;
            clear();
        });

    *m_modelConnections << connect(m_model, &QAbstractListModel::modelReset, this,
        [this]()
        {
            clear();
            insertNewTiles(0, m_model->rowCount(), UpdateMode::instant);
        });

    *m_modelConnections << connect(m_model, &QAbstractListModel::rowsInserted, this,
        [this](const QModelIndex& /*parent*/, int first, int last)
        {
            insertNewTiles(first, last - first + 1, UpdateMode::animated);
        });

    *m_modelConnections << connect(m_model, &QAbstractListModel::rowsAboutToBeRemoved, this,
        [this](const QModelIndex& /*parent*/, int first, int last)
        {
            removeTiles(first, last - first + 1, UpdateMode::animated);
        });

    *m_modelConnections << connect(m_model, &QAbstractListModel::dataChanged, this,
        [this](const QModelIndex& first, const QModelIndex& last)
        {
            const auto updateRange = m_visible.intersected({first.row(), last.row() + 1});
            for (int i = updateRange.lower(); i < updateRange.upper(); ++i)
                updateTile(i);
        });

    *m_modelConnections << connect(m_model, &QAbstractListModel::rowsAboutToBeMoved, this,
        [this](const QModelIndex& /*sourceParent*/, int sourceFirst, int sourceLast)
        {
            removeTiles(sourceFirst, sourceLast - sourceFirst + 1, UpdateMode::instant);
        });

    *m_modelConnections << connect(m_model, &QAbstractListModel::rowsMoved, this,
        [this](const QModelIndex& /*parent*/, int sourceFirst, int sourceLast,
            const QModelIndex& /*destinationParent*/, int destinationIndex)
        {
            NX_ASSERT(destinationIndex < sourceFirst || destinationIndex > sourceLast + 1);
            const auto count = sourceLast - sourceFirst + 1;
            const auto index = destinationIndex < sourceFirst
                ? destinationIndex
                : destinationIndex - count;

            insertNewTiles(index, count, UpdateMode::instant);
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
        widget->setRead(true);
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
    widget->setAutoCloseTime(modelIndex.data(Qn::TimeoutRole).value<std::chrono::milliseconds>());
    widget->setAction(modelIndex.data(Qn::CommandActionRole).value<CommandActionPtr>());

    const auto resourceList = modelIndex.data(Qn::ResourceListRole);
    if (resourceList.isValid())
    {
        if (resourceList.canConvert<QnResourceList>())
            widget->setResourceList(resourceList.value<QnResourceList>());
        else if (resourceList.canConvert<QStringList>())
            widget->setResourceList(resourceList.value<QStringList>());
    }

    setHelpTopic(widget, modelIndex.data(Qn::HelpTopicIdRole).toInt());

    const auto color = modelIndex.data(Qt::ForegroundRole).value<QColor>();
    if (color.isValid())
        widget->setTitleColor(color);

    widget->setFooterEnabled(m_footersEnabled);
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

    const auto previewCamera = modelIndex.data(Qn::ResourceRole).value<QnResourcePtr>()
        .dynamicCast<QnVirtualCameraResource>();

    if (!previewCamera)
        return;

    const auto previewTime = modelIndex.data(Qn::PreviewTimeRole).value<std::chrono::microseconds>();
    const auto previewCropRect = modelIndex.data(Qn::ItemZoomRectRole).value<QRectF>();
    const auto thumbnailWidth = previewCropRect.isEmpty()
        ? kDefaultThumbnailWidth
        : qMin(kDefaultThumbnailWidth / previewCropRect.width(), kMaximumThumbnailWidth);

    const bool precisePreview = !previewCropRect.isEmpty()
        || modelIndex.data(Qn::ForcePrecisePreviewRole).toBool();

    nx::api::CameraImageRequest request;
    request.camera = previewCamera;
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
    if (previewProvider && (request.camera != previewProvider->requestData().camera
        || request.usecSinceEpoch != previewProvider->requestData().usecSinceEpoch))
    {
        previewProvider.reset();
    }

    if (!previewProvider)
        previewProvider.reset(new CameraThumbnailProvider(request, widget));

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
                    m_model->setData(m_model->index(index), link, Qn::ActivateLinkRole);
            });

        connect(widget.get(), &EventTile::clicked, this,
            [this]()
            {
                const int index = indexOf(static_cast<EventTile*>(sender()));
                if (!m_model || index < 0)
                    return;

                const auto modelIndex = m_model->index(index);
                if (m_model->setData(modelIndex, QVariant(), Qn::DefaultNotificationRole))
                    return;

                const auto timestamp = modelIndex.data(Qn::TimestampRole);
                if (!timestamp.isValid())
                    return;

                const auto cameraList = modelIndex.data(Qn::ResourceListRole).value<QnResourceList>()
                    .filtered<QnVirtualCameraResource>();

                const auto camera = cameraList.size() == 1
                    ? cameraList.back()
                    : QnVirtualCameraResourcePtr();

                using namespace ui::action;

                if (camera)
                {
                    menu()->triggerIfPossible(GoToResourceAction, Parameters(camera)
                        .withArgument(Qn::ForceRole, ini().raiseCameraFromClickedTile));
                }

                menu()->triggerIfPossible(JumpToTimeAction,
                    Parameters().withArgument(Qn::TimestampRole, timestamp));
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
}

void EventRibbon::Private::reserveWidget(int index)
{
    NX_CRITICAL(index >= 0 && index < count());

    auto& widget = m_tiles[index]->widget;
    if (widget)
    {
        widget->hide();
        widget->clear();
        m_reserveWidgets.emplace(widget.release());
    }
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

void EventRibbon::Private::insertNewTiles(int index, int count, UpdateMode updateMode)
{
    if (!m_model || count == 0)
        return;

    if (index < 0 || index > this->count())
    {
        NX_ASSERT(false, Q_FUNC_INFO, "Insertion index is out of range");
        return;
    }

    const auto position = (index > 0)
        ? m_tiles[index - 1]->position + m_tiles[index - 1]->height + kDefaultTileSpacing
        : 0;

    const auto oldUnreadCount = unreadCount();
    const bool viewportVisible = m_viewport->isVisible() && m_viewport->width() > 0;

    if (!viewportVisible)
        updateMode = UpdateMode::instant;

    const int endIndex = index + count;
    int currentPosition = position;

    for (int i = index; i < endIndex; ++i)
    {
        const auto importance = m_model->index(i).data(Qn::NotificationLevelRole).value<Importance>();
        m_tiles.insert(m_tiles.begin() + i, TilePtr(new Tile(currentPosition, importance)));
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
        if (m_model->data(m_model->index(endIndex - 1), Qn::AnimatedRole).toBool())
            addAnimatedShift(endIndex, -m_tiles[endIndex - 1]->height);
    }

    doUpdateView();

    if (updateMode == UpdateMode::animated)
    {
        const auto highlightRange = m_visible.intersected({index, index + count});
        for (int i = highlightRange.lower(); i < highlightRange.upper(); ++i)
        {
            if (m_model->data(m_model->index(i), Qn::AnimatedRole).toBool())
               highlightAppearance(m_tiles[i]->widget.get());
        }
    }

    if (unreadCount() != oldUnreadCount)
        emit q->unreadCountChanged(unreadCount(), highestUnreadImportance(), PrivateSignal());

    emit q->countChanged(this->count());
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

    const int end = first + count;
    const int last = end - 1;
    const int nextPosition = m_tiles[last]->position + m_tiles[last]->height + kDefaultTileSpacing;

    const auto reserveRange = m_visible.intersected({first, end});
    for (int i = reserveRange.lower(); i != reserveRange.upper(); ++i)
        reserveWidget(i);

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

    const auto oldUnreadCount = unreadCount();
    for (int i = first; i < end; ++i)
    {
        const auto importance = m_tiles[i]->importance;
        if (importance != Importance())
        {
            --m_unreadCounts[int(importance)];
            --m_totalUnreadCount;
        }

        delta += m_tiles[i]->height + kDefaultTileSpacing;
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

    if (first != this->count() && m_model->data(m_model->index(first), Qn::AnimatedRole).toBool())
    {
        if (first == 0)
            m_tiles[0]->position = 0; //< Keep integrity: positions must start from 0.

        // In case of several tiles removing, animate only the topmost tile collapsing.
        if (topmostTileWasVisible && updateMode == UpdateMode::animated)
            addAnimatedShift(first, delta);
    }

    doUpdateView();

    if (unreadCount() != oldUnreadCount)
        emit q->unreadCountChanged(unreadCount(), highestUnreadImportance(), PrivateSignal());

    emit q->countChanged(this->count());
}

void EventRibbon::Private::clear()
{
    const auto hadUnreadTiles = unreadCount() != 0;
    m_unreadCounts = {};
    m_totalUnreadCount = 0;

    m_tiles.clear();
    m_visible = {};
    m_hoveredWidget = nullptr;
    m_totalHeight = 0;
    m_live = true;

    clearShiftAnimations();
    setScrollBarRelevant(false);

    q->updateGeometry();

    if (hadUnreadTiles)
        emit q->unreadCountChanged(0, Importance(), PrivateSignal());

    emit q->countChanged(count());
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
        NX_ASSERT(widget);
        if (widget)
            widget->setFooterEnabled(m_footersEnabled);
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

    using namespace std::chrono;
    using namespace std::literals::chrono_literals;

    const auto shouldHighlightTile =
        [this](int index) -> bool
        {
            if (m_highlightedTimestamp <= 0ms)
                return false;

            const auto modelIndex = m_model->index(index);
            const auto timestamp = modelIndex.data(Qn::TimestampRole).value<microseconds>();
            if (timestamp <= 0us)
                return false;

            const auto duration = modelIndex.data(Qn::DurationRole).value<microseconds>();
            if (duration <= 0us)
                return false;

            return m_highlightedTimestamp >= timestamp
                && (duration.count() == QnTimePeriod::infiniteDuration()
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

std::chrono::microseconds EventRibbon::Private::highlightedTimestamp() const
{
    return m_highlightedTimestamp;
}

void EventRibbon::Private::setHighlightedTimestamp(std::chrono::microseconds value)
{
    if (m_highlightedTimestamp == value)
        return;

    m_highlightedTimestamp = value;
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

void EventRibbon::Private::updateView()
{
    const auto oldUnreadCount = unreadCount();
    doUpdateView();

    if (unreadCount() != oldUnreadCount)
        emit q->unreadCountChanged(unreadCount(), highestUnreadImportance(), PrivateSignal());
}

void EventRibbon::Private::doUpdateView()
{
    if (m_tiles.empty())
    {
        clear();
        updateHover(false, QPoint());
        return;
    }

    if (!q->isVisible())
    {
        clearShiftAnimations();
        updateHover(false, QPoint());
        return;
    }

    updateCurrentShifts();

    const int base = m_scrollBarRelevant ? m_scrollBar->value() : 0;
    const int height = m_viewport->height();

    const auto secondInView = std::upper_bound(m_tiles.cbegin(), m_tiles.cend(), base,
        [this](int left, const TilePtr& right) { return left < right->position; });

    int firstIndexToUpdate = qMax<int>(0, secondInView - m_tiles.cbegin() - 1);

    if (!m_currentShifts.empty())
        firstIndexToUpdate = qMin(firstIndexToUpdate, m_currentShifts.begin().key());

    int currentPosition = m_topMargin;
    if (firstIndexToUpdate > 0)
    {
        const auto& prevTile = m_tiles[firstIndexToUpdate - 1];
        currentPosition = prevTile->position + prevTile->height + kDefaultTileSpacing;
    }

    const auto positionLimit = base + height;

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
        tile->widget->setGeometry(0, currentPosition - base, m_viewport->width(), tile->height);
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
            if (tile->importance != Importance() && shouldSetTileRead(tile->widget.get()))
            {
                --m_totalUnreadCount;
                --m_unreadCounts[int(tile->importance)];
                tile->importance = Importance();
                tile->widget->setRead(true);
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

    const auto pos = WidgetUtils::mapFromGlobal(q, QCursor::pos());
    updateHover(q->rect().contains(pos), pos);

    updateHighlightedTiles();

    if (!m_currentShifts.empty()) //< If has running animations.
        qApp->postEvent(m_viewport.get(), new QEvent(QEvent::LayoutRequest));
}

bool EventRibbon::Private::shouldSetTileRead(const EventTile* tile) const
{
    const auto rect = tile->geometry();
    const auto height = m_viewport->height();

    return rect.bottom() < height
        ? (rect.top() >= 0)
        : (rect.top() <= 0); //< Case for hypothetical tiles bigger than viewport.
}

void EventRibbon::Private::highlightAppearance(EventTile* tile)
{
    if (!tile->isVisible())
        return;

    using namespace std::chrono;

    auto animation = new QVariantAnimation(tile);
    animation->setStartValue(kHighlightCurtainOpacity);
    animation->setEndValue(0.0);
    animation->setDuration(duration_cast<milliseconds>(kHighlightDuration).count());
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

    using namespace std::chrono;

    auto animator = new QVariantAnimation(this);
    animator->setStartValue(qreal(shift));
    animator->setEndValue(0.0);
    animator->setEasingCurve(QEasingCurve::OutCubic);
    animator->setDuration(duration_cast<milliseconds>(kAnimationDuration).count());

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

void EventRibbon::Private::updateHover(bool hovered, const QPoint& mousePos)
{
    if (hovered)
    {
        if (m_hoveredWidget && m_hoveredWidget->underMouse()) //< Nothing changed.
            return;

        const int index = indexAtPos(mousePos);
        const auto widget = index >= 0 ? m_tiles[index]->widget.get() : nullptr;

        if (widget == m_hoveredWidget)
            return;

        m_hoveredWidget = widget;

        const auto modelIndex = (m_model && widget) ? m_model->index(index) : QModelIndex();
        emit q->tileHovered(modelIndex, widget);
    }
    else
    {
        if (!m_hoveredWidget)
            return;

        m_hoveredWidget = nullptr;
        emit q->tileHovered(QModelIndex(), nullptr);
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
    const int base = m_scrollBarRelevant ? m_scrollBar->value() : 0;

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
