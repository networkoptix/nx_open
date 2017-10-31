#include "event_ribbon_p.h"

#include <algorithm>

#include <QtCore/QPointer>
#include <QtCore/QAbstractItemModel>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QApplication>

#include <client/client_globals.h>
#include <utils/common/event_processors.h>
#include <ui/common/widget_anchor.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/style/helper.h>

#include <nx/client/desktop/event_search/widgets/event_tile.h>
#include <nx/utils/log/assert.h>


namespace nx {
namespace client {
namespace desktop {

namespace {

static constexpr int kDefaultTileSpacing = 1;
static constexpr int kScrollBarStep = 16;

static constexpr int kOutside = -10000;

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
        [this, viewportAnchor = QPointer<QnWidgetAnchor>(viewportAnchor)]()
        {
            if (!viewportAnchor)
                return;

            const auto margin = style::Metrics::kStandardPadding;
            const auto extra = m_scrollBar->isHidden() ? 0 : m_scrollBar->width();
            viewportAnchor->setMargins(margin, 0, margin + extra, 0);
        };

    installEventHandler(m_scrollBar, {QEvent::Show, QEvent::Hide}, this, updateViewportMargins);
    updateViewportMargins();

    installEventHandler(m_viewport, {QEvent::Show, QEvent::Resize}, this,
        [this](QObject* /*watched*/, QEvent* event)
        {
            if (m_currentWidth != m_viewport->width() || event->type() == QEvent::Show)
            {
                m_currentWidth = m_viewport->width();
                updateAllPositions();
            }
            updateScrollRange();
            updateView();
        });

    connect(m_scrollBar, &QScrollBar::valueChanged, this, &Private::updateView);
}

EventRibbon::Private::~Private()
{
    m_modelConnections.reset();
}

QAbstractItemModel* EventRibbon::Private::model() const
{
    return m_model;
}

void EventRibbon::Private::setModel(QAbstractItemModel* model)
{
    if (m_model == model)
        return;

    m_modelConnections.reset();
    clear();

    m_model = model;

    if (!m_model)
        return;

    insertNewTiles(0, m_model->rowCount(m_rootIndex));

    m_modelConnections.reset(new QnDisconnectHelper());

    *m_modelConnections << connect(m_model, &QAbstractListModel::modelReset, this,
        [this]()
        {
            clear();
            insertNewTiles(0, m_model->rowCount(m_rootIndex));
        });

    *m_modelConnections << connect(m_model, &QAbstractListModel::rowsInserted, this,
        [this](const QModelIndex& parent, int first, int last)
        {
            if (m_rootIndex.isValid() != parent.isValid())
                return;

            if (m_rootIndex.isValid() && m_rootIndex != parent)
                return;

            insertNewTiles(first, last - first + 1);
        });

    *m_modelConnections << connect(m_model, &QAbstractListModel::rowsAboutToBeRemoved, this,
        [this](const QModelIndex& parent, int first, int last)
        {
            if (m_rootIndex.isValid() != parent.isValid())
                return;

            if (m_rootIndex.isValid() && m_rootIndex != parent)
                return;

            removeTiles(first, last - first + 1);
        });

    *m_modelConnections << connect(m_model, &QAbstractListModel::dataChanged, this,
        [this](const QModelIndex& first, const QModelIndex& last)
        {
            if (first.column() > m_modelColumn || last.column() < m_modelColumn)
                return;

            if (m_rootIndex.isValid() != first.parent().isValid())
                return;

            if (m_rootIndex.isValid() && m_rootIndex != first.parent())
                return;

            for (int i = first.row(); i <= last.row(); ++i)
                updateTile(m_tiles[i].widget, first.sibling(i, m_modelColumn));
        });

    *m_modelConnections << connect(m_model, &QAbstractListModel::rowsMoved, this,
        [this]()
        {
            NX_ASSERT(false, Q_FUNC_INFO, "EventRibbon doesn't support model rows moving!");
            clear(); // Handle as full reset.
            insertNewTiles(0, m_model->rowCount(m_rootIndex));
        });
}

void EventRibbon::Private::insertNewTiles(int first, int count)
{
    if (!m_model)
        return;

    for (int i = first; i < first + count; ++i)
    {
        const auto index = m_model->index(i, m_modelColumn, m_rootIndex);
        auto tile = createTile(index);
        NX_EXPECT(tile);
        if (tile)
            insertTile(i, tile);
    }
}

EventTile* EventRibbon::Private::createTile(const QModelIndex& index)
{
    const auto id = index.data(Qn::UuidRole).value<QnUuid>();
    if (id.isNull())
        return nullptr;

    auto tile = new EventTile(id, q);
    updateTile(tile, index);

    connect(tile, &EventTile::closeRequested, this,
        [this]()
        {
            emit q->closeRequested(static_cast<EventTile*>(sender())->id());
        });

    connect(tile, &EventTile::linkActivated, this,
        [this](const QString& link)
        {
            emit q->linkActivated(static_cast<EventTile*>(sender())->id(), link);
        });

    connect(tile, &EventTile::clicked, this,
        [this]()
        {
            emit q->clicked(static_cast<EventTile*>(sender())->id());
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

    setHelpTopic(tile, index.data(Qn::HelpTopicIdRole).toInt());

    const auto color = index.data(Qt::ForegroundRole).value<QColor>();
    if (color.isValid())
        tile->setTitleColor(color);

    // TODO: #vkutin Preview?
}

void EventRibbon::Private::insertTile(int index, EventTile* tileWidget)
{
    NX_EXPECT(tileWidget);
    if (!tileWidget)
        return;

    if (index < 0 || index > m_tiles.count())
    {
        NX_ASSERT(false, Q_FUNC_INFO, "Insertion index is out of range");
        return;
    }

    tileWidget->setParent(m_viewport);
    tileWidget->setVisible(true);

    tileWidget->setGeometry(0, kOutside, m_currentWidth, calculateHeight(tileWidget));
    const auto delta = tileWidget->height() + kDefaultTileSpacing;

    const auto position = (index > 0)
        ? m_tiles[index - 1].position + m_tiles[index - 1].widget->height() + kDefaultTileSpacing
        : 0;

    for (int i = index; i < m_tiles.count(); ++i)
        m_tiles[i].position += delta;

    m_tiles.insert(index, Tile(tileWidget, position));

    m_totalHeight += delta;
    updateScrollRange();

    // TODO: #vkutin This won't work if entire ribbon is invisible at the moment. Think about it.
    if (position + delta < m_scrollBar->value())
        m_scrollBar->setValue(m_scrollBar->value() + delta);

    updateView();
    q->updateGeometry();
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
    const int nextPosition = m_tiles[last].position + m_tiles[last].widget->height()
        + kDefaultTileSpacing;

    for (int i = first; i <= last; ++i)
    {
        delta += m_tiles[first].widget->height() + kDefaultTileSpacing;
        m_tiles[first].widget->deleteLater();
        m_tiles.removeAt(first);
    }

    auto newVisible = m_visible - nx::utils::IntegerRange(first, count);
    newVisible = newVisible.left | (newVisible.right - count);
    NX_EXPECT(!newVisible.right);
    m_visible = newVisible.left;

    m_totalHeight -= delta;

    for (int i = first; i < m_tiles.count(); ++i)
        m_tiles[i].position -= delta;

    if (nextPosition < m_scrollBar->value())
        m_scrollBar->setValue(m_scrollBar->value() - delta);

    updateScrollRange();

    updateView();
    q->updateGeometry();
}

void EventRibbon::Private::clear()
{
    for (auto& tile: m_tiles)
        tile.widget->deleteLater();

    m_tiles.clear();
    m_totalHeight = 0;
    m_visible = nx::utils::IntegerRange();

    updateScrollRange();
    q->updateGeometry();
}

void EventRibbon::Private::updateAllPositions()
{
    // TODO: #vkutin Preserve view?
    // Not sure it's needed because under normal circumstances tiles can change size
    // only interactively when they're visible in the viewport,
    // or when scroll bar shows/hides - but then there's no need to preserve view

    m_totalHeight = 0;
    for (auto& tile: m_tiles)
    {
        tile.widget->resize(m_currentWidth, calculateHeight(tile.widget));
        tile.position = m_totalHeight;
        m_totalHeight += tile.widget->height() + kDefaultTileSpacing;
    }
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
        ? widget->heightForWidth(m_currentWidth)
        : widget->sizeHint().expandedTo(minimumWidgetSize(widget)).height();
}

void EventRibbon::Private::updateScrollRange()
{
    const auto viewHeight = m_viewport->height();
    m_scrollBar->setMaximum(qMax(m_totalHeight - viewHeight, 1));
    m_scrollBar->setPageStep(viewHeight);
    m_scrollBar->setVisible(m_totalHeight > viewHeight);
}

void EventRibbon::Private::updateView()
{
    if (m_tiles.empty())
        return;

    const int position = m_scrollBar->isHidden() ? 0 : m_scrollBar->value();
    const int height = m_viewport->height();

    const auto first = std::upper_bound(m_tiles.cbegin(), m_tiles.cend(), position,
        [](int left, const Tile& right) { return left < right.position; }) - 1;

    int visibleCount = 0;
    int currentPosition = first->position - position;

    for (auto iter = first; iter != m_tiles.end() && currentPosition < height; ++iter, ++visibleCount)
    {
        iter->widget->move(0, currentPosition);
        currentPosition += iter->widget->height() + kDefaultTileSpacing;
    }

    nx::utils::IntegerRange newVisible(std::distance(m_tiles.cbegin(), first), visibleCount);

    // We cannot hide tiles because their lazy layouts would stop telling us correct size hints.
    for (int i = m_visible.first(); i <= m_visible.last(); ++i)
    {
        if (!newVisible.contains(i))
            m_tiles[i].widget->move(0, kOutside);
    }

    m_visible = newVisible;
    m_viewport->update();
}

} // namespace
} // namespace client
} // namespace nx
