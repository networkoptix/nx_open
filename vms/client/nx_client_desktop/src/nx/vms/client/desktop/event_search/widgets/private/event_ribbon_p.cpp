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
#include <utils/common/event_processors.h>
#include <nx/vms/client/desktop/common/utils/custom_painted.h>
#include <ui/common/notification_levels.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/style/helper.h>
#include <ui/style/nx_style_p.h>
#include <ui/workaround/hidpi_workarounds.h>

#include <nx/api/mediaserver/image_request.h>
#include <nx/vms/client/desktop/event_search/widgets/event_tile.h>
#include <nx/vms/client/desktop/ui/actions/action.h>
#include <nx/vms/client/desktop/image_providers/camera_thumbnail_provider.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>

#include <nx/vms/client/desktop/ini.h>

namespace nx::vms::client::desktop {

namespace {

static constexpr int kDefaultTileSpacing = 1;
static constexpr int kScrollBarStep = 16;

// Approximate default height of an invisible tile.
// When tile becomes visible its true height can be computed and used.
static constexpr int kApproximateTileHeight = 40;

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
    QObject(),
    q(q),
    m_scrollBar(new QScrollBar(Qt::Vertical, q)),
    m_viewport(new QWidget(q))
{
    q->setAttribute(Qt::WA_Hover);
    m_viewport->setAttribute(Qt::WA_Hover);

    setScrollBarRelevant(false);
    m_scrollBar->setSingleStep(kScrollBarStep);
    m_scrollBar->setFixedWidth(m_scrollBar->sizeHint().width());
    anchorWidgetToParent(m_scrollBar, Qt::RightEdge | Qt::TopEdge | Qt::BottomEdge);

    const int mainPadding = q->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    anchorWidgetToParent(m_viewport, {mainPadding, 0, mainPadding * 2, 0});

    installEventHandler(m_viewport,
        {QEvent::Show, QEvent::Hide, QEvent::Resize, QEvent::LayoutRequest},
        this, &Private::updateView);

    connect(m_scrollBar, &QScrollBar::valueChanged, this, &Private::updateView);
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
            for (int i = first.row(); i <= last.row(); ++i)
                updateTile(m_tiles[i], first.sibling(i, 0));
        });

    *m_modelConnections << connect(m_model, &QAbstractListModel::rowsAboutToBeMoved, this,
        [this](const QModelIndex& /*sourceParent*/, int sourceFirst, int sourceLast)
        {
            // TODO: #vkutin Optimize.
            removeTiles(sourceFirst, sourceLast - sourceFirst + 1, UpdateMode::instant);
        });

    *m_modelConnections << connect(m_model, &QAbstractListModel::rowsMoved, this,
        [this](const QModelIndex& /*parent*/, int sourceFirst, int sourceLast,
            const QModelIndex& /*destinationParent*/, int destinationIndex)
        {
            // TODO: #vkutin Optimize.
            NX_ASSERT(destinationIndex < sourceFirst || destinationIndex > sourceLast + 1);
            const auto count = sourceLast - sourceFirst + 1;
            const auto position = destinationIndex < sourceFirst
                ? destinationIndex
                : destinationIndex - count;

            insertNewTiles(position, count, UpdateMode::instant);
        });
}

EventTile* EventRibbon::Private::createTile(const QModelIndex& index)
{
    auto tile = new EventTile(q);
    tile->setContextMenuPolicy(Qt::CustomContextMenu);
    tile->installEventFilter(this);
    tile->setPreviewEnabled(m_previewsEnabled);
    tile->setFooterEnabled(m_footersEnabled);
    updateTile(tile, index);

    const auto importance = index.data(Qn::NotificationLevelRole);

    if (tile->progressBarVisible() || !importance.canConvert<QnNotificationLevel::Value>())
        tile->setRead(true);
    else
        m_unread.insert(tile, importance.value<QnNotificationLevel::Value>());

    connect(tile, &EventTile::closeRequested, this,
        [this]()
        {
            if (m_model)
                m_model->removeRow(indexOf(static_cast<EventTile*>(sender())));
        });

    connect(tile, &EventTile::linkActivated, this,
        [this](const QString& link)
        {
            const auto tile = static_cast<EventTile*>(sender());
            if (m_model)
                m_model->setData(m_model->index(indexOf(tile)), link, Qn::ActivateLinkRole);
        });

    connect(tile, &EventTile::clicked, this,
        [this]()
        {
            const auto tile = static_cast<EventTile*>(sender());
            if (m_model)
                m_model->setData(m_model->index(indexOf(tile)), QVariant(), Qn::DefaultNotificationRole);
        });

    connect(tile, &QWidget::customContextMenuRequested, this,
        [this](const QPoint &pos) { showContextMenu(static_cast<EventTile*>(sender()), pos); });

    return tile;
}

void EventRibbon::Private::updateTile(EventTile* tile, const QModelIndex& index)
{
    NX_ASSERT(tile && index.isValid());

    // Check whether the tile is a special busy indicator tile.
    const auto busyIndicatorVisibility = index.data(Qn::BusyIndicatorVisibleRole);
    if (busyIndicatorVisibility.isValid())
    {
        constexpr int kIndicatorHeight = 24;
        tile->setFixedHeight(kIndicatorHeight);
        tile->setBusyIndicatorVisible(busyIndicatorVisibility.toBool());
        return;
    }

    // Select tile color style.
    tile->setVisualStyle(index.data(Qn::AlternateColorRole).toBool()
        ? EventTile::Style::informer
        : EventTile::Style::standard);

    // Check whether the tile is a special progress bar tile.
    const auto progress = index.data(Qn::ProgressValueRole);
    if (progress.canConvert<qreal>())
    {
        tile->setProgressBarVisible(true);
        tile->setProgressValue(progress.value<qreal>());
        tile->setProgressTitle(index.data(Qt::DisplayRole).toString());
        tile->setDescription(index.data(Qn::DescriptionTextRole).toString());
        tile->setToolTip(index.data(Qn::DescriptionTextRole).toString());
        return;
    }

    // Check whether the tile is a special separator tile.
    const auto title = index.data(Qt::DisplayRole).toString();
    if (title.isEmpty())
        return;

    tile->setTitle(title);
    tile->setIcon(index.data(Qt::DecorationRole).value<QPixmap>());
    tile->setTimestamp(index.data(Qn::TimestampTextRole).toString());
    tile->setDescription(index.data(Qn::DescriptionTextRole).toString());
    tile->setFooterText(index.data(Qn::AdditionalTextRole).toString());
    tile->setToolTip(index.data(Qt::ToolTipRole).toString());
    tile->setCloseable(index.data(Qn::RemovableRole).toBool());
    tile->setAutoCloseTimeMs(index.data(Qn::TimeoutRole).toInt());
    tile->setAction(index.data(Qn::CommandActionRole).value<CommandActionPtr>());

    const auto resourceList = index.data(Qn::ResourceListRole);
    if (resourceList.isValid())
    {
        if (resourceList.canConvert<QnResourceList>())
            tile->setResourceList(resourceList.value<QnResourceList>());
        else if (resourceList.canConvert<QStringList>())
            tile->setResourceList(resourceList.value<QStringList>());
    }

    setHelpTopic(tile, index.data(Qn::HelpTopicIdRole).toInt());

    const auto color = index.data(Qt::ForegroundRole).value<QColor>();
    if (color.isValid())
        tile->setTitleColor(color);

    const auto camera = index.data(Qn::ResourceRole).value<QnResourcePtr>()
        .dynamicCast<QnVirtualCameraResource>();

    if (!camera)
        return;

    const auto previewTimeUs = index.data(Qn::PreviewTimeRole).value<qint64>();
    const auto previewCropRect = index.data(Qn::ItemZoomRectRole).value<QRectF>();
    const auto thumbnailWidth = previewCropRect.isEmpty()
        ? kDefaultThumbnailWidth
        : qMin(kDefaultThumbnailWidth / previewCropRect.width(), kMaximumThumbnailWidth);

    const bool precisePreview = !previewCropRect.isEmpty()
        || index.data(Qn::ForcePrecisePreviewRole).toBool();

    nx::api::CameraImageRequest request;
    request.camera = camera;
    request.usecSinceEpoch =
        previewTimeUs > 0 ? previewTimeUs : nx::api::ImageRequest::kLatestThumbnail;
    request.rotation = nx::api::ImageRequest::kDefaultRotation;
    request.size = QSize(thumbnailWidth, 0);
    request.imageFormat = nx::api::ImageRequest::ThumbnailFormat::jpg;
    request.aspectRatio = nx::api::ImageRequest::AspectRatio::source;
    request.roundMethod = precisePreview
        ? nx::api::ImageRequest::RoundMethod::precise
        : nx::api::ImageRequest::RoundMethod::iFrameAfter;

    const auto showPreviewTimestamp =
        [tile](CameraThumbnailProvider* provider)
        {
            if (provider->status() == Qn::ThumbnailStatus::Loaded)
            {
                tile->setDescription(lit("%1<br>Preview: %2 us").arg(tile->description())
                    .arg(provider->timestampUs()));
            }
        };

    if (tile->preview())
    {
        auto provider = qobject_cast<CameraThumbnailProvider*>(tile->preview());
        NX_ASSERT(provider);

        if (!provider)
            return;

        if (request.usecSinceEpoch == provider->requestData().usecSinceEpoch)
        {
            if (ini().showDebugTimeInformationInRibbon)
                showPreviewTimestamp(provider);
        }
        else
        {
            provider->setRequestData(request);
            provider->loadAsync();
            tile->setPreviewCropRect(previewCropRect);
        }
    }
    else
    {
        auto provider = new CameraThumbnailProvider(request, tile);
        tile->setPreview(provider);

        if (ini().showDebugTimeInformationInRibbon)
        {
            connect(provider, &ImageProvider::statusChanged, tile,
                [showPreviewTimestamp, provider]() { showPreviewTimestamp(provider); });
        }

        tile->preview()->loadAsync();
        tile->setPreviewCropRect(previewCropRect);
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

void EventRibbon::Private::debugCheckGeometries()
{
#if 0
#if defined(_DEBUG)
    int pos = 0;
    for (int i = 0; i < m_tiles.size(); ++i)
    {
        pos += m_currentShifts.value(i);
        NX_ASSERT(pos == m_positions[m_tiles[i]]);
        pos += m_tiles[i]->height() + kDefaultTileSpacing;
    }

    NX_ASSERT(pos == m_totalHeight);
#endif
#endif
}

void EventRibbon::Private::debugCheckVisibility()
{
#if defined(_DEBUG)
    if (!q->isVisible())
        return;

    for (int i = 0; i < m_tiles.size(); ++i)
    {
        NX_ASSERT(m_tiles[i]->isHidden() == !m_visible.contains(m_tiles[i]));
    }
#endif
}

void EventRibbon::Private::insertNewTiles(int index, int count, UpdateMode updateMode)
{
    if (!m_model || count == 0)
        return;

    if (index < 0 || index > m_tiles.count())
    {
        NX_ASSERT(false, Q_FUNC_INFO, "Insertion index is out of range");
        return;
    }

    const auto position = (index > 0)
        ? m_positions.value(m_tiles[index - 1]) + m_tiles[index - 1]->height() + kDefaultTileSpacing
        : 0;

    int currentPosition = position;
    int nextIndex = index;

    const auto oldUnreadCount = unreadCount();
    const bool viewportVisible = m_viewport->isVisible() && m_viewport->width() > 0;

    if (!viewportVisible)
        updateMode = UpdateMode::instant;

    for (int i = 0; i < count; ++i)
    {
        const auto modelIndex = m_model->index(index + i);
        auto tile = createTile(modelIndex);
        NX_ASSERT(tile);
        if (!tile)
            continue;

        if (viewportVisible)
        {
            static const QPoint kOutside(-10000, 0); //< Somewhere outside of visible area.
            tile->move(kOutside);
            tile->setParent(m_viewport);
            tile->setVisible(true); //< For sizeHint calculation.
            tile->resize(m_viewport->width(), calculateHeight(tile));
            tile->setVisible(false);
        }
        else
        {
            tile->setVisible(false);
            tile->resize(qMax(1, m_viewport->width()), kApproximateTileHeight);
            tile->setParent(m_viewport);
        }

        m_tiles.insert(nextIndex++, tile);
        m_positions[tile] = currentPosition;
        currentPosition += tile->height() + kDefaultTileSpacing;
    }

    const auto delta = currentPosition - position;

    for (int i = nextIndex; i < m_tiles.count(); ++i)
        m_positions[m_tiles[i]] += delta;

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
    if (updateMode == UpdateMode::animated && nextIndex < m_tiles.size())
    {
        if (m_model->data(m_model->index(nextIndex - 1), Qn::AnimatedRole).toBool())
            addAnimatedShift(nextIndex, -m_tiles[nextIndex - 1]->height());
    }

    doUpdateView();

    if (updateMode == UpdateMode::animated)
    {
        for (int i = 0; i < count; ++i)
        {
            if (m_model->data(m_model->index(index + i), Qn::AnimatedRole).toBool())
               highlightAppearance(m_tiles[index + i]);
        }
    }

    if (unreadCount() != oldUnreadCount)
        emit q->unreadCountChanged(unreadCount(), highestUnreadImportance(), PrivateSignal());

    emit q->countChanged(m_tiles.size());
}

void EventRibbon::Private::removeTiles(int first, int count, UpdateMode updateMode)
{
    NX_ASSERT(count);
    if (count == 0)
        return;

    if (first < 0 || count < 0 || first + count > m_tiles.count())
    {
        NX_ASSERT(false, Q_FUNC_INFO, "Removal range is invalid");
        return;
    }

    const int last = first + count - 1;
    const int nextPosition = m_positions.value(m_tiles[last]) + m_tiles[last]->height()
        + kDefaultTileSpacing;

    int delta = 0;
    const bool topmostTileWasVisible = m_tiles[first]->isVisible();

    const auto oldUnreadCount = unreadCount();
    for (int i = first; i <= last; ++i)
    {
        if (!m_tiles[first]->isRead())
            m_unread.remove(m_tiles[first]);
        delta += m_tiles[first]->height() + kDefaultTileSpacing;
        m_tiles[first]->deleteLater();
        m_positions.remove(m_tiles[first]);
        m_visible.remove(m_tiles[first]);
        m_tiles.removeAt(first);
    }

    for (int i = first; i < m_tiles.count(); ++i)
        m_positions[m_tiles[i]] -= delta;

    m_totalHeight -= delta;

    if (nextPosition < m_scrollBar->value())
        m_scrollBar->setValue(m_scrollBar->value() - delta);

    // Correct current animations.
    for (auto& animatedIndex: m_itemShiftAnimations)
    {
        if (animatedIndex > first)
            animatedIndex = qMax(first, animatedIndex - count);
    }

    if (first != m_tiles.size() && m_model->data(m_model->index(first), Qn::AnimatedRole).toBool())
    {
        if (first == 0)
            m_positions[m_tiles[0]] = 0; //< Keep integrity: positions must start from 0.

        // In case of several tiles removing, animate only the topmost tile collapsing.
        if (topmostTileWasVisible && updateMode == UpdateMode::animated)
            addAnimatedShift(first, delta);
    }

    doUpdateView();

    if (unreadCount() != oldUnreadCount)
        emit q->unreadCountChanged(unreadCount(), highestUnreadImportance(), PrivateSignal());

    emit q->countChanged(m_tiles.size());
}

void EventRibbon::Private::clear()
{
    for (auto& tile: m_tiles)
        tile->deleteLater();

    const auto hadUnreadTiles = !m_unread.empty();
    m_unread.clear();

    m_tiles.clear();
    m_positions.clear();
    m_visible.clear();
    m_totalHeight = 0;
    m_live = true;

    clearShiftAnimations();
    setScrollBarRelevant(false);

    q->updateGeometry();

    if (hadUnreadTiles)
        emit q->unreadCountChanged(0, QnNotificationLevel::Value::NoNotification, PrivateSignal());

    emit q->countChanged(m_tiles.size());
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

int EventRibbon::Private::indexOf(EventTile* tile) const
{
    const auto position = m_positions.value(tile);
    const auto first = std::lower_bound(m_tiles.cbegin(), m_tiles.cend(), position,
        [this](EventTile* left, int right) { return m_positions.value(left) < right; });

    return first != m_tiles.cend() && m_positions.value(*first) == position
        ? std::distance(m_tiles.cbegin(), first)
        : -1;
}

int EventRibbon::Private::totalHeight() const
{
    return m_totalHeight;
}

QScrollBar* EventRibbon::Private::scrollBar() const
{
    return m_scrollBar;
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

    for (auto tile: m_tiles)
        tile->setPreviewEnabled(m_previewsEnabled);
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

    for (auto tile: m_tiles)
        tile->setFooterEnabled(m_footersEnabled);
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

QnNotificationLevel::Value EventRibbon::Private::highestUnreadImportance() const
{
    QnNotificationLevel::Value result = QnNotificationLevel::Value::NoNotification;

    // TODO: #vkutin Redo it differently if it visibly impacts performance.
    static constexpr auto kMaxNotificationLevel = int(QnNotificationLevel::Value::LevelCount) - 1;
    for (const auto importance: m_unread)
    {
        if (importance < result)
            continue;

        result = importance;
        if (int(result) == kMaxNotificationLevel)
            break;
    }

    qDebug() << "Highest level is" << int(result);
    return result;
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
        [this](int left, EventTile* right) { return left < m_positions.value(right); });

    int firstIndexToUpdate = qMax(0, secondInView - m_tiles.cbegin() - 1);

    if (!m_currentShifts.empty())
        firstIndexToUpdate = qMin(firstIndexToUpdate, m_currentShifts.begin().key());

    int currentPosition = m_topMargin;
    if (firstIndexToUpdate > 0)
    {
        const auto prevTile = m_tiles[firstIndexToUpdate - 1];
        currentPosition = m_positions.value(prevTile) + prevTile->height() + kDefaultTileSpacing;
    }

    const auto positionLimit = base + height;

    static constexpr int kWidthThreshold = 400;
    const auto mode = m_viewport->width() > kWidthThreshold
        ? EventTile::Mode::wide
        : EventTile::Mode::standard;

    QSet<EventTile*> newVisible;

    auto iter = m_tiles.cbegin() + firstIndexToUpdate;
    while (iter != m_tiles.end() && currentPosition < positionLimit)
    {
        const auto tile = *iter;
        currentPosition += m_currentShifts.value(iter - m_tiles.cbegin());
        m_positions[tile] = currentPosition;
        tile->setGeometry(0, currentPosition - base, m_viewport->width(), calculateHeight(tile));
        tile->setMode(mode);
        const auto bottom = currentPosition + tile->height();
        currentPosition = bottom + kDefaultTileSpacing;

        if (bottom > 0)
        {
            tile->setVisible(true);
            newVisible.insert(tile);

            if (!tile->isRead() && shouldSetTileRead(tile))
            {
                tile->setRead(true);
                m_unread.remove(tile);
            }
        }

        ++iter;
    }

    while (iter != m_tiles.end())
    {
        currentPosition += m_currentShifts.value(iter - m_tiles.cbegin());
        m_positions[*iter] = currentPosition;
        currentPosition += (*iter)->height() + kDefaultTileSpacing;
        ++iter;
    }

    currentPosition += m_bottomMargin;

    if (m_totalHeight != currentPosition)
    {
        m_totalHeight = currentPosition;
        q->updateGeometry();
    }

    debugCheckGeometries();

    for (auto& oldVisibleTile: m_visible)
    {
        if (!newVisible.contains(oldVisibleTile))
            oldVisibleTile->setVisible(false);
    }

    m_visible = newVisible;
    m_viewport->update();

    updateScrollRange();
    debugCheckVisibility();

    const auto pos = WidgetUtils::mapFromGlobal(q, QCursor::pos());
    updateHover(q->rect().contains(pos), pos);

    if (!m_currentShifts.empty()) //< If has running animations.
        qApp->postEvent(m_viewport, new QEvent(QEvent::LayoutRequest));
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
    return m_tiles.size();
}

int EventRibbon::Private::unreadCount() const
{
    return m_unread.size();
}

void EventRibbon::Private::updateHover(bool hovered, const QPoint& mousePos)
{
    if (hovered)
    {
        if (m_hoveredTile && m_hoveredTile->underMouse()) //< Nothing changed.
            return;

        const int index = indexAtPos(mousePos);
        const auto tile = index >= 0 ? m_tiles[index] : nullptr;

        if (tile == m_hoveredTile)
            return;

        m_hoveredTile = tile;

        const auto modelIndex = (m_model && tile) ? m_model->index(index) : QModelIndex();
        emit q->tileHovered(modelIndex, tile);
    }
    else
    {
        if (m_hoveredTile)
        {
            m_hoveredTile = nullptr;
            emit q->tileHovered(QModelIndex(), nullptr);
        }
    }
}

int EventRibbon::Private::indexAtPos(const QPoint& pos) const
{
    const auto viewportPos = m_viewport->mapFrom(q, pos);
    const int base = m_scrollBarRelevant ? m_scrollBar->value() : 0;

    const auto next = std::upper_bound(m_tiles.cbegin(), m_tiles.cend(), base + viewportPos.y(),
        [this](int left, EventTile* right) { return left < m_positions.value(right); });

    if (next == m_tiles.cbegin())
        return -1;

    const auto candidate = next - 1;

    return (*candidate)->geometry().contains(viewportPos)
        ? std::distance(m_tiles.cbegin(), candidate)
        : -1;
}

bool EventRibbon::Private::eventFilter(QObject* object, QEvent* event)
{
    if (qobject_cast<EventTile*>(object) && object->parent() == m_viewport)
    {
        if (!m_showDefaultToolTips && event->type() == QEvent::ToolTip)
        {
            event->ignore();
            return true; //< Ignore tooltip events.
        }
    }

    return QObject::eventFilter(object, event);
}

} // namespace nx::vms::client::desktop
