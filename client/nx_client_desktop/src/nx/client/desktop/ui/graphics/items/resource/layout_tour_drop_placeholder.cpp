#include "layout_tour_drop_placeholder.h"

#include <QtGui/QPainter>

#include <QtWidgets/QGraphicsScale>
#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>

#include <ui/common/palette.h>
#include <ui/graphics/items/generic/viewport_bound_widget.h>
#include <ui/graphics/items/generic/masked_proxy_widget.h>

#include <utils/math/color_transformations.h>

#include <nx/utils/math/fuzzy.h>

namespace {

static const qreal kTextOpacity = 0.7;
static const qreal kFrameOpacity = 0.3;
static const int kFixedTextWidth = 110; //< pixels

void makeTransparentForMouse(QGraphicsItem* item)
{
    item->setAcceptedMouseButtons(Qt::NoButton);
    item->setAcceptHoverEvents(false);
}

QnMaskedProxyWidget* makeMaskedProxy(
    QWidget* source,
    QGraphicsItem* parentItem)
{
    const auto result = new QnMaskedProxyWidget(parentItem);
    result->setWidget(source);
    result->setAcceptDrops(false);
    result->setCacheMode(QGraphicsItem::NoCache);
    makeTransparentForMouse(result);
    return result;
}

QColor calculateFrameColor(const QPalette& palette)
{
    return toTransparent(palette.color(QPalette::WindowText), kFrameOpacity);
}

} // namespace

namespace nx {
namespace client {
namespace desktop {
namespace ui {

LayoutTourDropPlaceholder::LayoutTourDropPlaceholder(
    QGraphicsItem* parent,
    Qt::WindowFlags windowFlags)
    :
    base_type(parent, windowFlags),
    m_widget(new QnViewportBoundWidget(this))
{
    setAcceptedMouseButtons(0);
    setFrameShape(Qn::RoundedRectangularFrame);
    setFrameStyle(Qt::CustomDashLine);
    setDashPattern({10, 3});
    setFrameWidth(0); //< Cosmetic pen
    setRoundingRadius(50);
    setPaletteColor(this, QPalette::Window, Qt::transparent);
    setFrameBrush(calculateFrameColor(palette()));

    auto caption = new QLabel(tr("Drag layout or camera here to add it to the reel"));
    caption->setAlignment(Qt::AlignCenter);
    caption->setWordWrap(true);
    caption->setFixedWidth(kFixedTextWidth);

    const auto container = new QWidget();
    container->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    const auto layout = new QVBoxLayout(container);
    layout->setSpacing(0);
    layout->addWidget(caption, 0, Qt::AlignHCenter);

    const auto horizontalLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    horizontalLayout->addStretch(1);
    horizontalLayout->addItem(makeMaskedProxy(container, m_widget));
    horizontalLayout->addStretch(1);

    const auto verticalLayout = new QGraphicsLinearLayout(Qt::Vertical, m_widget);
    verticalLayout->addStretch(1);
    verticalLayout->addItem(horizontalLayout);
    verticalLayout->addStretch(1);

    m_widget->setOpacity(kTextOpacity);
    makeTransparentForMouse(m_widget);
    addOverlayWidget(m_widget, detail::OverlayParams(Visible));
}

const QRectF& LayoutTourDropPlaceholder::rect() const
{
    return m_rect;
}

void LayoutTourDropPlaceholder::setRect(const QRectF& rect)
{
    if (qFuzzyEquals(m_rect, rect))
        return;

    prepareGeometryChange();
    m_rect = rect;
    setGeometry(rect);
}

void LayoutTourDropPlaceholder::changeEvent(QEvent* event)
{
    base_type::changeEvent(event);

    if (event->type() == QEvent::PaletteChange)
        setFrameBrush(calculateFrameColor(palette()));
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
