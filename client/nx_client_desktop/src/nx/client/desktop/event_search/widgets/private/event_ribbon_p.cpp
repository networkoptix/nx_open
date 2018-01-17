#include "event_ribbon_p.h"

#include <algorithm>
#include <chrono>

#include <QtCore/QPointer>
#include <QtCore/QAbstractListModel>
#include <QtCore/QVariantAnimation>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QApplication>

#include <nx/client/desktop/image_providers/single_thumbnail_loader.h>
#include <client/client_globals.h>
#include <core/resource/camera_resource.h>
#include <utils/common/event_processors.h>
#include <ui/common/custom_painted.h>
#include <ui/common/widget_anchor.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/style/helper.h>

#include <nx/client/desktop/event_search/widgets/event_tile.h>
#include <nx/client/desktop/ui/actions/action.h>
#include <nx/utils/log/assert.h>

namespace nx {
namespace client {
namespace desktop {

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
    m_scrollBar->setHidden(true);
    m_scrollBar->setSingleStep(kScrollBarStep);
    m_scrollBar->setFixedWidth(m_scrollBar->sizeHint().width());

    auto scrollBarAnchor = new QnWidgetAnchor(m_scrollBar);
    scrollBarAnchor->setEdges(Qt::RightEdge | Qt::TopEdge | Qt::BottomEdge);

    auto viewportAnchor = new QnWidgetAnchor(m_viewport);
    viewportAnchor->setEdges(Qt::LeftEdge | Qt::RightEdge | Qt::TopEdge | Qt::BottomEdge);

    const int mainPadding = q->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    static constexpr int kExtraPadding = 1; //< Gap between scrollbar and tiles.
    viewportAnchor->setMargins(mainPadding, 0, mainPadding + kExtraPadding, 0);

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
    updateTile(tile, index);

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

    return tile;
}

void EventRibbon::Private::updateTile(EventTile* tile, const QModelIndex& index)
{
    NX_EXPECT(tile && index.isValid());

    // Check whether the tile is a special busy indicator tile.
    const auto busyIndicatorVisibility = index.data(Qn::BusyIndicatorVisibleRole);
    if (busyIndicatorVisibility.isValid())
    {
        tile->setBusyIndicatorVisible(busyIndicatorVisibility.toBool());
        return;
    }

    // Check whether the tile is a special progress bar tile.
    const auto progress = index.data(Qn::ProgressValueRole);
    if (progress.canConvert<qreal>())
    {
        tile->setProgressBarVisible(true);
        tile->setProgressValue(progress.value<qreal>());
        tile->setProgressTitle(index.data(Qt::DisplayRole).toString());
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
    tile->setToolTip(index.data(Qt::ToolTipRole).toString());
    tile->setCloseable(index.data(Qn::RemovableRole).toBool());
    tile->setAutoCloseTimeMs(index.data(Qn::TimeoutRole).toInt());
    tile->setAction(index.data(Qn::CommandActionRole).value<CommandActionPtr>());

    setHelpTopic(tile, index.data(Qn::HelpTopicIdRole).toInt());

    const auto color = index.data(Qt::ForegroundRole).value<QColor>();
    if (color.isValid())
        tile->setTitleColor(color);

    if (tile->preview())
        return; //< Don't ever update existing previews.

    const auto camera = index.data(Qn::ResourceRole).value<QnResourcePtr>()
        .dynamicCast<QnVirtualCameraResource>();

    if (!camera)
        return;

    const auto previewTimeMs = index.data(Qn::PreviewTimeRole).value<qint64>();
    const auto previewCropRect = index.data(Qn::ItemZoomRectRole).value<QRectF>();
    const auto thumbnailWidth = previewCropRect.isEmpty()
        ? kDefaultThumbnailWidth
        : qMin(kDefaultThumbnailWidth / previewCropRect.width(), kMaximumThumbnailWidth);

    tile->setPreview(new QnSingleThumbnailLoader(
        camera,
        previewTimeMs > 0 ? previewTimeMs : QnThumbnailRequestData::kLatestThumbnail,
        QnThumbnailRequestData::kDefaultRotation,
        QSize(thumbnailWidth, 0),
        QnThumbnailRequestData::JpgFormat,
        QnThumbnailRequestData::AspectRatio::AutoAspectRatio,
        QnThumbnailRequestData::RoundMethod::KeyFrameAfterMethod,
        tile));

    tile->preview()->loadAsync();
    tile->setPreviewCropRect(previewCropRect);
}

void EventRibbon::Private::debugCheckGeometries()
{
#if defined(_DEBUG)
    int pos = 0;
    for (int i = 0; i < m_tiles.size(); ++i)
    {
        NX_ASSERT(pos == m_positions[m_tiles[i]]);
        pos += m_currentShifts.value(i);
        pos += m_tiles[i]->height() + kDefaultTileSpacing;
    }

    NX_ASSERT(pos == m_totalHeight);
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

    for (int i = 0; i < count; ++i)
    {
        const auto modelIndex = m_model->index(index + i);
        auto tile = createTile(modelIndex);
        NX_ASSERT(tile);
        if (!tile)
            continue;

        if (m_viewport->isVisible() && m_viewport->width() > 0)
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
            updateMode = UpdateMode::instant;
        }

        m_tiles.insert(nextIndex++, tile);
        m_positions[tile] = currentPosition;
        currentPosition += kApproximateTileHeight + kDefaultTileSpacing;
    }

    const auto delta = currentPosition - position;

    for (int i = nextIndex; i < m_tiles.count(); ++i)
        m_positions[m_tiles[i]] += delta;

    m_totalHeight += delta;
    q->updateGeometry();

    m_scrollBar->setMaximum(m_scrollBar->maximum() + delta);

    if (position < m_scrollBar->value())
        m_scrollBar->setValue(m_scrollBar->value() + delta);

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

    updateView();

    if (updateMode == UpdateMode::animated)
    {
        for (int i = 0; i < count; ++i)
        {
            if (m_model->data(m_model->index(index + i), Qn::AnimatedRole).toBool())
               highlightAppearance(m_tiles[index + i]);
        }
    }
}

void EventRibbon::Private::removeTiles(int first, int count, UpdateMode updateMode)
{
    NX_EXPECT(count);
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

    for (int i = first; i <= last; ++i)
    {
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

    if (first != m_tiles.size())
    {
        if (first == 0)
            m_positions[m_tiles[0]] = 0; //< Keep integrity: positions must start from 0.

        // In case of several tiles removing, animate only the topmost tile collapsing.
        if (topmostTileWasVisible && updateMode == UpdateMode::animated)
            addAnimatedShift(first, delta);
    }

    updateView();
}

void EventRibbon::Private::clear()
{
    for (auto& tile: m_tiles)
        tile->deleteLater();

    m_tiles.clear();
    m_positions.clear();
    m_visible.clear();
    m_totalHeight = 0;

    clearShiftAnimations();

    m_scrollBar->setVisible(false);
    m_scrollBar->setValue(0);

    q->updateGeometry();
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

int EventRibbon::Private::calculateHeight(QWidget* widget) const
{
    NX_EXPECT(widget);
    return widget->hasHeightForWidth()
        ? widget->heightForWidth(m_viewport->width())
        : widget->sizeHint().expandedTo(minimumWidgetSize(widget)).height();
}

void EventRibbon::Private::updateScrollRange()
{
    const auto viewHeight = m_viewport->height();
    m_scrollBar->setMaximum(qMax(m_totalHeight - viewHeight, 1));
    m_scrollBar->setPageStep(viewHeight);
    m_scrollBar->setVisible(m_totalHeight > viewHeight);
    if (m_scrollBar->isHidden())
        m_scrollBar->setValue(0);
}

void EventRibbon::Private::updateView()
{
    if (m_tiles.empty())
    {
        clear();
        return;
    }

    if (!q->isVisible())
    {
        clearShiftAnimations();
        return;
    }

    const int base = m_scrollBar->isHidden() ? 0 : m_scrollBar->value();
    const int height = m_viewport->height();

    const auto first = std::upper_bound(m_tiles.cbegin(), m_tiles.cend(), base,
        [this](int left, EventTile* right) { return left < m_positions.value(right); }) - 1;

    auto iter = first;
    int currentPosition = m_positions.value(*first);
    const auto positionLimit = base + height;

    QSet<EventTile*> newVisible;

    updateCurrentShifts();

    while (iter != m_tiles.end() && currentPosition < positionLimit)
    {
        m_positions[*iter] = currentPosition;
        currentPosition += m_currentShifts.value(iter - m_tiles.cbegin());
        (*iter)->setVisible(true);
        (*iter)->setGeometry(0, currentPosition - base,
            m_viewport->width(), calculateHeight(*iter));
        newVisible.insert((*iter));
        currentPosition += (*iter)->height() + kDefaultTileSpacing;
        ++iter;
    }

    while (iter != m_tiles.end())
    {
        m_positions[*iter] = currentPosition;
        currentPosition += (*iter)->height() + kDefaultTileSpacing;
        ++iter;
    }

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

    if (!m_currentShifts.empty()) //< If has running animations.
        qApp->postEvent(m_viewport, new QEvent(QEvent::LayoutRequest));
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
    new QnWidgetAnchor(curtain);

    curtain->setCustomPaintFunction(
        [animation, curtain, guard = QPointer<QVariantAnimation>(animation)]
            (QPainter* painter, const QStyleOption* /*option*/, const QWidget* /*widget*/)
        {
            if (!guard)
                return true;

            QColor color = kHighlightCurtainColor;
            color.setAlphaF(animation->currentValue().toReal());
            painter->fillRect(curtain->rect(), color);
            curtain->update();
            return true;
        });

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

} // namespace desktop
} // namespace client
} // namespace nx
