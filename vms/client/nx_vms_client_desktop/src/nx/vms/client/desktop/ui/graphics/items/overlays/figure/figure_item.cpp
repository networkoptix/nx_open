// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "figure_item.h"

#include "figure.h"
#include "renderer.h"

#include <QtGui/QPolygonF>
#include <QtWidgets/QGraphicsWidget>
#include <QtWidgets/QGraphicsSceneMouseEvent>
#include <nx/vms/client/core/utils/geometry.h>
#include <QPainter>

namespace nx::vms::client::desktop {

FigureItem::FigureItem(
    const figure::FigurePtr& figure,
    const figure::RendererPtr& renderer,
    QGraphicsItem* parentItem,
    QObject* parentObject)
    :
    QObject(parentObject),
    QGraphicsItem(parentItem),
    m_figure(figure),
    m_renderer(renderer)
{
    setAcceptedMouseButtons(Qt::NoButton);
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemClipsToShape);
}

void FigureItem::setFigure(const figure::FigurePtr& value)
{
    m_figure = value;

    if (!m_figure)
        setContainsMouse(false);
}

figure::FigurePtr FigureItem::figure() const
{
    return m_figure;
}

bool FigureItem::containsMouse() const
{
    return m_containsMouse;
}

void FigureItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
    setContainsMouse(m_figure && m_figure->containsPoint(event->pos(), parentWidget()->size()));
}

void FigureItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* /*event*/)
{
    setContainsMouse(false);
}

void FigureItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    event->accept();
    emit clicked();
}

void FigureItem::setContainsMouse(bool value)
{
    if (m_containsMouse == value)
        return;

    m_containsMouse = value;
    emit containsMouseChanged();
}

void FigureItem::paint(QPainter* painter,
    const QStyleOptionGraphicsItem* /*option*/,
    QWidget* widget)
{
    if (m_figure)
        m_renderer->renderFigure(painter, widget, m_figure, parentWidget()->size());
}

QRectF FigureItem::boundingRect() const
{
    return m_figure
        ? m_figure->visualRect(parentWidget()->size())
        : QRect();
}

QPainterPath FigureItem::shape() const
{
    using namespace nx::vms::client::core;

    const auto parentRect = parentWidget()->boundingRect().translated(-pos());
    const auto clipRect = Geometry::intersection(boundingRect(), parentRect);
    QPainterPath result;
    static constexpr qreal kOffset = 2;
    result.addRect(clipRect.adjusted(-kOffset, -kOffset, kOffset, kOffset));
    return result;
}

} // namespace nx::vms::client::desktop
