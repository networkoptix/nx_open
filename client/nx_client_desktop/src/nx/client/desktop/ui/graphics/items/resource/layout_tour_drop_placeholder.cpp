#include "layout_tour_drop_placeholder.h"

#include <QtGui/QPainter>

#include <QtWidgets/QGraphicsScale>
#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>

#include <ui/animation/rect_animator.h>
#include <ui/common/palette.h>
#include <ui/graphics/items/generic/viewport_bound_widget.h>
#include <ui/graphics/items/generic/masked_proxy_widget.h>

#include <utils/math/color_transformations.h>

#include <nx/utils/math/fuzzy.h>

namespace {

void makeTransparentForMouse(QGraphicsItem* item)
{
    item->setAcceptedMouseButtons(Qt::NoButton);
    item->setAcceptHoverEvents(false);
}

QnMaskedProxyWidget* makeMaskedProxy(
    QWidget* source,
    QGraphicsItem* parentItem,
    bool transparent)
{
    const auto result = new QnMaskedProxyWidget(parentItem);
    result->setWidget(source);
    result->setAcceptDrops(false);
    result->setCacheMode(QGraphicsItem::NoCache);

    if (transparent)
        makeTransparentForMouse(result);

    return result;
}

void setupLabel(QLabel* label)
{
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    label->setFixedWidth(200);
}

QColor calculateFrameColor(const QPalette& palette)
{
    static const qreal kFrameOpacity = 0.5;
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
    m_widget(new QnViewportBoundWidget(this)),
    m_geometryAnimator(new RectAnimator(this))
{
    setAcceptedMouseButtons(0);
    setFrameShape(Qn::RoundedRectangularFrame);
    setFrameStyle(Qt::CustomDashLine);
    setDashPattern({10, 3});
    setFrameWidth(0); //< Cosmetic pen
    setRoundingRadius(50);
    setPaletteColor(this, QPalette::Window, Qt::transparent);
    setFrameBrush(calculateFrameColor(palette()));

    const QString message = tr("Drag layout here to add it to the tour");
    auto caption = new QLabel(message);
    setupLabel(caption);
    caption->setVisible(true);

    const auto container = new QWidget();
    container->setObjectName(lit("centralContainer"));
    container->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setPaletteColor(container, QPalette::Window, Qt::transparent);

    const auto layout = new QVBoxLayout(container);
    layout->setSpacing(0);
    layout->addWidget(caption, 0, Qt::AlignHCenter);

    const auto horizontalLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    horizontalLayout->addStretch(1);
    horizontalLayout->addItem(makeMaskedProxy(container, m_widget, true));
    horizontalLayout->addStretch(1);

    const auto verticalLayout = new QGraphicsLinearLayout(Qt::Vertical, m_widget);
    verticalLayout->setContentsMargins(16, 60, 16, 60);
    verticalLayout->addStretch(1);
    verticalLayout->addItem(horizontalLayout);
    verticalLayout->addStretch(1);

    m_widget->setOpacity(0.7);
    makeTransparentForMouse(m_widget);
    addOverlayWidget(m_widget, detail::OverlayParams(Visible));

    m_geometryAnimator->setTargetObject(this);
    m_geometryAnimator->setAccessor(new PropertyAccessor("geometry"));
    m_geometryAnimator->setTimeLimit(100);
}

QRectF LayoutTourDropPlaceholder::boundingRect() const
{
    return rect();
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

    m_geometryAnimator->pause();
    m_geometryAnimator->setTargetValue(rect);
    m_geometryAnimator->start();
}

AnimationTimer* LayoutTourDropPlaceholder::animationTimer() const
{
    return m_geometryAnimator->timer();
}

void LayoutTourDropPlaceholder::setAnimationTimer(AnimationTimer* timer)
{
    m_geometryAnimator->setTimer(timer);
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
