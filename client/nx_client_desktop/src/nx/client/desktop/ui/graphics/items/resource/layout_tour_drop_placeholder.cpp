#include "layout_tour_drop_placeholder.h"

#include <QtGui/QPainter>

#include <QtWidgets/QGraphicsScale>
#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>

#include <ui/utils/viewport_scale_watcher.h>
#include <ui/graphics/items/generic/viewport_bound_widget.h>
#include <ui/graphics/items/generic/masked_proxy_widget.h>
#include <ui/common/palette.h>

#include <utils/common/scoped_painter_rollback.h>

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
    const bool isDescription = false;
    const bool isError = false;

    auto font = label->font();
    const int pixelSize = (isDescription ? 36 : (isError ? 88 : 80));
    const int areaWidth = (isError ? 960 : 800);

    font.setPixelSize(pixelSize);
    font.setWeight(isDescription ? QFont::Normal : QFont::Light);
    label->setFont(font);

    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(isDescription);

    label->setFixedWidth(isDescription
        ? areaWidth
        : qMax(areaWidth, label->minimumSizeHint().width()));

    const auto color = Qt::red; //qnNxStyle->mainColor(QnNxStyle::Colors::kContrast));
    setPaletteColor(label, QPalette::WindowText, color);
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
    m_scaleWatcher(new QnViewportScaleWatcher(this)),
    m_widget(new QnViewportBoundWidget(this))
{
    setAcceptedMouseButtons(0);

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
    //m_widget->setFixedSize({300, 300});

}

QRectF LayoutTourDropPlaceholder::boundingRect() const
{
    return rect();
}

void LayoutTourDropPlaceholder::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* /*option*/,
    QWidget* /*widget*/)
{
    QPen pen;
    pen.setColor(Qt::red);
    pen.setStyle(Qt::DashLine);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCosmetic(true);

    QnScopedPainterPenRollback penRollback(painter, pen);
    painter->drawRect(m_rect);
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
    m_widget->setGeometry(rect);
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
