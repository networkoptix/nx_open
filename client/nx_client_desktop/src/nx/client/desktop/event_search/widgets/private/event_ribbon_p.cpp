#include "event_ribbon_p.h"

#include <algorithm>

#include <QtCore/QPointer>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QApplication>

#include <utils/common/event_processors.h>
#include <ui/common/widget_anchor.h>
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
            viewportAnchor->setMargins(margin, 1, margin + extra, 0);
        };

    installEventHandler(m_scrollBar, {QEvent::Show, QEvent::Hide}, this, updateViewportMargins);
    updateViewportMargins();

    installEventHandler(m_viewport, QEvent::Resize, this,
        [this]()
        {
            if (m_currentWidth != m_viewport->width())
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
}

void EventRibbon::Private::addTile(EventTile* tileWidget, const QnUuid& id)
{
    NX_EXPECT(tileWidget);
    if (!tileWidget)
        return;

    tileWidget->setParent(m_viewport);
    tileWidget->setVisible(true);

    tileWidget->setGeometry(0, kOutside, m_currentWidth, calculateHeight(tileWidget));
    const auto delta = tileWidget->height() + kDefaultTileSpacing;

    for (auto& tile: m_tiles)
        tile.position += delta;

    // TODO: #vkutin Fast lookup by id? Checking for duplicates?
    m_tiles.push_front(Tile(tileWidget, id));
    m_totalHeight += delta;

    updateScrollRange();

    if (m_scrollBar->value() > delta)
        m_scrollBar->setValue(m_scrollBar->value() + delta);

    m_scrollBar->setVisible(m_totalHeight > m_viewport->height());

    updateView();

    q->updateGeometry();
}

void EventRibbon::Private::removeTile(const QnUuid& id)
{
    // TODO: #vkutin Implement me!
}

void EventRibbon::Private::updateAllPositions()
{
    // TODO: #vkutin Preserve view?
    // Not sure it's needed because under normal circumstances tiles can change size
    // only interactively when they're visible in the viewport,
    // or when scroll bar shows/hides - but then there's no need to preserve view

    int position = 0;
    for (auto& tile: m_tiles)
    {
        tile.widget->resize(m_currentWidth, calculateHeight(tile.widget));
        tile.position = position;
        position += tile.widget->height() + kDefaultTileSpacing;
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
}

void EventRibbon::Private::updateView()
{
    if (m_tiles.empty())
        return;

    const int position = m_scrollBar->isHidden() ? 0 : m_scrollBar->value();
    const int height = m_viewport->height();

    const auto first = std::upper_bound(m_tiles.cbegin(), m_tiles.cend(), position,
        [](int left, const Tile& right) { return left < right.position; }) - 1;

    QSet<const Tile*> newVisibleTiles;

    int currentPosition = first->position - position;
    for (auto iter = first; iter != m_tiles.end(); ++iter)
    {
        iter->widget->move(0, currentPosition);
        newVisibleTiles.insert(&*iter);

        currentPosition += iter->widget->height() + kDefaultTileSpacing;
        if (currentPosition >= height)
            break;
    }

    // We cannot hide tiles because their lazy layouts would stop telling us correct size hints.
    for (auto tile: m_visibleTiles)
    {
        if (!newVisibleTiles.contains(tile))
            tile->widget->move(0, kOutside);
    }

    m_visibleTiles = newVisibleTiles;
    m_viewport->update();
}

} // namespace
} // namespace client
} // namespace nx
