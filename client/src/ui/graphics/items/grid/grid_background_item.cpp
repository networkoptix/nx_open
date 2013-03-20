#include "grid_background_item.h"

#include <QPainter>

#include <ui/workbench/workbench_grid_mapper.h>

QnGridBackgroundItem::QnGridBackgroundItem(QGraphicsItem *parent):
    QGraphicsObject(parent),
    m_color(QColor(0, 0, 0, 0))
{
    setAcceptedMouseButtons(0);

    /* Don't disable this item here. When disabled, it starts accepting wheel events
     * (and probably other events too). Looks like a Qt bug. */
}

QRectF QnGridBackgroundItem::boundingRect() const {
    return m_rect;
}

QnGridBackgroundItem::~QnGridBackgroundItem() {
    return;
}

void QnGridBackgroundItem::setMapper(QnWorkbenchGridMapper *mapper) {
    m_mapper = mapper;
    connect(mapper,     SIGNAL(originChanged()),    this,   SLOT(updateGeometry()));
    connect(mapper,     SIGNAL(cellSizeChanged()),  this,   SLOT(updateGeometry()));
    connect(mapper,     SIGNAL(spacingChanged()),   this,   SLOT(updateGeometry()));
}

void QnGridBackgroundItem::setSceneRect(const QRect &rect) {
    m_sceneRect = rect;
    updateGeometry();
}

void QnGridBackgroundItem::updateGeometry() {
    if(mapper() == NULL)
        return;

    prepareGeometryChange();
    m_rect = mapper()->mapFromGrid(m_sceneRect);
}

void QnGridBackgroundItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    painter->fillRect(m_rect, m_color);
}

