#include "event_ribbon_p.h"

#include <algorithm>

#include <QtCore/QPointer>
#include <QtCore/QAbstractListModel>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QApplication>

#include <camera/single_thumbnail_loader.h>
#include <client/client_globals.h>
#include <core/resource/camera_resource.h>
#include <utils/common/event_processors.h>
#include <utils/multi_image_provider.h>
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

static constexpr int kDefaultThumbnailSize = 224;
static constexpr int kMultiThumbnailSpacing = 1;

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

    const auto updateViewportMargins =
        [this, viewportAnchor, guard = QPointer<Private>(this)]()
        {
            if (!guard)
                return;

            const auto margin = style::Metrics::kStandardPadding;
            const auto extra = m_scrollBar->isHidden() ? 0 : m_scrollBar->width();
            viewportAnchor->setMargins(margin, 0, margin + extra, 0);
        };

    installEventHandler(m_scrollBar, {QEvent::Show, QEvent::Hide},
        this, updateViewportMargins, Qt::QueuedConnection);

    updateViewportMargins();

    installEventHandler(m_viewport, {QEvent::Show, QEvent::Resize, QEvent::LayoutRequest},
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

    insertNewTiles(0, m_model->rowCount());

    m_modelConnections.reset(new QnDisconnectHelper());

    *m_modelConnections << connect(m_model, &QAbstractListModel::modelReset, this,
        [this]()
        {
            clear();
            insertNewTiles(0, m_model->rowCount());
        });

    *m_modelConnections << connect(m_model, &QAbstractListModel::rowsInserted, this,
        [this](const QModelIndex& /*parent*/, int first, int last)
        {
            insertNewTiles(first, last - first + 1);
        });

    *m_modelConnections << connect(m_model, &QAbstractListModel::rowsAboutToBeRemoved, this,
        [this](const QModelIndex& /*parent*/, int first, int last)
        {
            removeTiles(first, last - first + 1);
        });

    *m_modelConnections << connect(m_model, &QAbstractListModel::dataChanged, this,
        [this](const QModelIndex& first, const QModelIndex& last)
        {
            for (int i = first.row(); i <= last.row(); ++i)
                updateTile(m_tiles[i], first.sibling(i, 0));
        });

    *m_modelConnections << connect(m_model, &QAbstractListModel::rowsMoved, this,
        [this]()
        {
            NX_ASSERT(false, Q_FUNC_INFO, "EventRibbon doesn't support model rows moving!");
            clear(); // Handle as full reset.
            insertNewTiles(0, m_model->rowCount());
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

    tile->setTitle(index.data(Qt::DisplayRole).toString());
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
    tile->setPreview(new QnSingleThumbnailLoader(
        camera,
        previewTimeMs > 0 ? previewTimeMs : QnThumbnailRequestData::kLatestThumbnail,
        QnThumbnailRequestData::kDefaultRotation,
        QSize(kDefaultThumbnailSize, 0),
        QnThumbnailRequestData::JpgFormat,
        QnThumbnailRequestData::AspectRatio::AutoAspectRatio,
        QnThumbnailRequestData::RoundMethod::KeyFrameAfterMethod,
        tile));

    tile->preview()->loadAsync();
    tile->setPreviewHighlight(index.data(Qn::ItemZoomRectRole).value<QRectF>());
}

void EventRibbon::Private::debugCheckGeometries()
{
#if defined(_DEBUG)
    int pos = 0;
    for (int i = 0; i < m_tiles.size(); ++i)
    {
        NX_ASSERT(pos == m_positions[m_tiles[i]]);
        pos += m_tiles[i]->height() + kDefaultTileSpacing;
    }

    NX_ASSERT(pos == m_totalHeight);
#endif
}

void EventRibbon::Private::insertNewTiles(int index, int count)
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

        tile->setVisible(false);
        tile->resize(qMax(1, m_viewport->width()), kApproximateTileHeight);
        tile->setParent(m_viewport);
        m_tiles.insert(nextIndex++, tile);
        m_positions[tile] = currentPosition;
        currentPosition += kApproximateTileHeight + kDefaultTileSpacing;
    }

    const auto delta = currentPosition - position;

    for (int i = nextIndex; i < m_tiles.count(); ++i)
        m_positions[m_tiles[i]] += delta;

    m_totalHeight += delta;
    q->updateGeometry();

    debugCheckGeometries();

    updateView();

    if (position < m_scrollBar->value())
        m_scrollBar->setValue(m_scrollBar->value() + delta);
}

void EventRibbon::Private::removeTiles(int first, int count)
{
    NX_EXPECT(count);
    if (count == 0)
        return;

    if (first < 0 || count < 0 || first + count > m_tiles.count())
    {
        NX_ASSERT(false, Q_FUNC_INFO, "Removal range is invalid");
        return;
    }

    int delta = 0;

    const int last = first + count - 1;
    const int nextPosition = m_positions.value(m_tiles[last]) + m_tiles[last]->height()
        + kDefaultTileSpacing;

    for (int i = first; i <= last; ++i)
    {
        delta += m_tiles[first]->height() + kDefaultTileSpacing;
        m_tiles[first]->deleteLater();
        m_positions.remove(m_tiles[first]);
        m_tiles.removeAt(first);
    }

    auto newVisible = m_visible - nx::utils::IntegerRange(first, count);
    newVisible = newVisible.left | (newVisible.right - count);
    NX_EXPECT(!newVisible.right);
    m_visible = newVisible.left;

    for (int i = first; i < m_tiles.count(); ++i)
        m_positions[m_tiles[i]] -= delta;

    m_totalHeight -= delta;

    debugCheckGeometries();

    updateView();

    if (nextPosition < m_scrollBar->value())
        m_scrollBar->setValue(m_scrollBar->value() - delta);
}

void EventRibbon::Private::clear()
{
    for (auto& tile: m_tiles)
        tile->deleteLater();

    m_tiles.clear();
    m_positions.clear();
    m_totalHeight = 0;
    m_visible = nx::utils::IntegerRange();

    m_scrollBar->setVisible(false);
    m_scrollBar->setValue(0);

    q->updateGeometry();
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
    if (m_tiles.empty() || !q->isVisible())
    {
        m_scrollBar->setValue(0);
        m_scrollBar->setVisible(false);
        return;
    }

    const int base = m_scrollBar->isHidden() ? 0 : m_scrollBar->value();
    const int height = m_viewport->height();

    const auto first = std::upper_bound(m_tiles.cbegin(), m_tiles.cend(), base,
        [this](int left, EventTile* right) { return left < m_positions.value(right); }) - 1;

    auto iter = first;
    int currentPosition = m_positions.value(*first);
    const auto positionLimit = base + height;

    while (iter != m_tiles.end() && currentPosition < positionLimit)
    {
        m_positions[*iter] = currentPosition;
        (*iter)->setVisible(true);
        (*iter)->setGeometry(0, currentPosition - base,
            m_viewport->width(), calculateHeight(*iter));
        currentPosition += (*iter)->height() + kDefaultTileSpacing;
        ++iter;
    }

    const auto visibleCount = std::distance(first, iter);

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

    nx::utils::IntegerRange newVisible(std::distance(m_tiles.cbegin(), first), visibleCount);

    for (int i = m_visible.first(); i <= m_visible.last(); ++i)
    {
        if (!newVisible.contains(i))
            m_tiles[i]->setVisible(false);
    }

    m_visible = newVisible;
    m_viewport->update();

    updateScrollRange();
}

} // namespace
} // namespace client
} // namespace nx
