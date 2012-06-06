#include "grid_item.h"
#include <limits>
#include <cmath>
#include <QPainter>
#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/checked_cast.h>
#include <ui/common/geometry.h>
#include <ui/workbench/workbench_grid_mapper.h>
#include <ui/animation/variant_animator.h>
#include "grid_highlight_item.h"

namespace {
    const char *pointPropertyName = "_qn_itemPoint";
    const char *animatorPropertyName = "_qn_itemAnimator";
}

Q_DECLARE_METATYPE(VariantAnimator *);

QnGridItem::QnGridItem(QGraphicsItem *parent):
    QGraphicsObject(parent),
    m_color(QColor(0, 0, 0, 0)),
    m_lineWidth(0.0),
    m_opacityAnimator(new VariantAnimator(this))
{
    qreal d = std::numeric_limits<qreal>::max() / 4;
    m_boundingRect = QRectF(QPointF(-d, -d), QPointF(d, d));

    setStateColor(ALLOWED, QColor(0, 255, 0, 64));
    setStateColor(DISALLOWED, QColor(255, 0, 0, 64));

    m_opacityAnimator->setTargetObject(this);
    m_opacityAnimator->setAccessor(new PropertyAccessor("opacity"));
    m_opacityAnimator->setSpeed(1.0);

    setAcceptedMouseButtons(0);
    
    /* Don't disable this item here. When disabled, it starts accepting wheel events 
     * (and probably other events too). Looks like a Qt bug. */
}

QnGridItem::~QnGridItem() {
    return;
}

void QnGridItem::setMapper(QnWorkbenchGridMapper *mapper) {
    m_mapper = mapper;
}

void QnGridItem::setAnimationTimer(AnimationTimer *timer) {
    m_opacityAnimator->setTimer(timer);
}

void QnGridItem::setAnimationSpeed(qreal speed) {
    if(m_opacityAnimator->isRunning()) {
        m_opacityAnimator->pause();
        m_opacityAnimator->setSpeed(speed);
        m_opacityAnimator->start();
    } else {
        m_opacityAnimator->setSpeed(speed);
    }
}

void QnGridItem::setAnimationTimeLimit(int timeLimitMSec) {
    if(m_opacityAnimator->isRunning()) {
        m_opacityAnimator->pause();
        m_opacityAnimator->setTimeLimit(timeLimitMSec);
        m_opacityAnimator->start();
    } else {
        m_opacityAnimator->setTimeLimit(timeLimitMSec);
    }
}

void QnGridItem::animatedShow() {
    m_opacityAnimator->animateTo(1.0);
}

void QnGridItem::animatedHide() {
    m_opacityAnimator->animateTo(0.0);
}

QRectF QnGridItem::boundingRect() const {
    return m_boundingRect;
}

void QnGridItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *widget) {
    if(mapper() == NULL)
        return;

    if(qFuzzyIsNull(painter->opacity()))
        return;

    /* Calculate the extends that would cover entire viewport. */
    QRectF viewportRect = painter->transform().inverted().mapRect(QRectF(widget->rect()));
    QRectF gridRectF = mapper()->mapToGridF(viewportRect);
    QRect gridRect = QRect(
        QPoint(
            std::floor(gridRectF.left()),
            std::floor(gridRectF.top())
        ) - QPoint(1, 1), /* Adjustment due to the fact that we draw the grid slightly out of the border cells. */
        QPoint(
            std::ceil(gridRectF.right()),
            std::ceil(gridRectF.bottom())
        ) - QPoint(1, 1) /* Adjustment due to QRect's totally inhuman handling of height & width. */
    );

    /* Draw! */
    QnScopedPainterPenRollback penRollback(painter, QPen(m_color, m_lineWidth));
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);
    QPointF topLeft = mapper()->mapFromGrid(gridRect.topLeft()) - QnGeometry::toPoint(mapper()->spacing()) / 2;
    QPointF delta = QnGeometry::toPoint(mapper()->step());

    /* Vertical lines. */
    qreal x = topLeft.x();
    for (int i = gridRect.left(); i <= gridRect.right(); ++i) {
        painter->drawLine(QPointF(x, viewportRect.top()), QPointF(x, viewportRect.bottom()));

        x += delta.x();
    }

    /* Horizontal lines. */
    qreal y = topLeft.y();
    for (int i = gridRect.top(); i <= gridRect.bottom(); ++i) {
        painter->drawLine(QPointF(viewportRect.left(), y), QPointF(viewportRect.right(), y));

        y += delta.y();
    }
}

QColor QnGridItem::stateColor(int cellState) const {
    return m_colorByState.value(cellState, QColor(0, 0, 0, 0));
}

void QnGridItem::setStateColor(int cellState, const QColor &color) {
    if(cellState == INITIAL) {
        qnWarning("Cannot change color for initial cell state.");
        return;
    }

    m_colorByState[cellState] = color;
}

QnGridHighlightItem *QnGridItem::newHighlightItem() {
    if(!m_freeItems.empty())
        return m_freeItems.takeLast();

    return new QnGridHighlightItem(this);
}

QPoint QnGridItem::itemCell(QnGridHighlightItem *item) const {
    return item->property(pointPropertyName).toPoint();
}

void QnGridItem::setItemCell(QnGridHighlightItem *item, const QPoint &cell) const {
    item->setProperty(pointPropertyName, cell);
}

VariantAnimator *QnGridItem::itemAnimator(QnGridHighlightItem *item) {
    VariantAnimator *animator = item->property(animatorPropertyName).value<VariantAnimator *>();
    if(animator != NULL)
        return animator;
    
    animator = new VariantAnimator(item);
    animator->setTargetObject(item);
    animator->setTimer(m_opacityAnimator->timer());
    animator->setAccessor(new PropertyAccessor("color"));
    animator->setConverter(new QnColorToVectorConverter());
    animator->setSpeed(m_opacityAnimator->speed());
    connect(animator, SIGNAL(finished()), this, SLOT(at_itemAnimator_finished()));
    item->setProperty(animatorPropertyName, QVariant::fromValue<VariantAnimator *>(animator));
    return animator;
}

void QnGridItem::at_itemAnimator_finished() {
    VariantAnimator *animator = checked_cast<VariantAnimator *>(sender());
    QnGridHighlightItem *item = checked_cast<QnGridHighlightItem *>(animator->targetObject());

    QPoint cell = itemCell(item);
    PointData &data = m_dataByCell[cell];
    if(data.state == INITIAL) {
        data.item = NULL;
        m_freeItems.push_back(item);
        setItemCell(item, QPoint(0, 0));
    }
}

int QnGridItem::cellState(const QPoint &cell) const {
    QHash<QPoint, PointData>::const_iterator pos = m_dataByCell.find(cell);
    if(pos == m_dataByCell.end())
        return INITIAL;

    return pos->state;
}

void QnGridItem::setCellState(const QPoint &cell, int cellState) {
    if(mapper() == NULL)
        return;
    
    PointData &data = m_dataByCell[cell];
    if(data.state == cellState)
        return;
    data.state = cellState;

    if(data.item == NULL) {
        data.item = newHighlightItem();

        qreal d =  m_lineWidth / 2; 
        data.item->setRect(QnGeometry::dilated(mapper()->mapFromGrid(QRect(cell, cell)), mapper()->spacing() / 2).adjusted(d, d, -d, -d));
        setItemCell(data.item, cell);
    }

    VariantAnimator *animator = itemAnimator(data.item);
    animator->pause();
    
    QColor targetColor = stateColor(cellState);
    if(data.item->color().alpha() == 0)
        data.item->setColor(toTransparent(targetColor));
    animator->setTargetValue(targetColor);
    
    animator->start();
}

void QnGridItem::setCellState(const QSet<QPoint> &cells, int cellState) {
    foreach(const QPoint &cell, cells)
        setCellState(cell, cellState);
}

void QnGridItem::setCellState(const QRect &cells, int cellState) {
    for (int r = cells.top(); r <= cells.bottom(); r++) 
        for (int c = cells.left(); c <= cells.right(); c++) 
            setCellState(QPoint(c, r), cellState);
}

void QnGridItem::setCellState(const QList<QRect> &cells, int cellState) {
    foreach(const QRect &rect, cells)
        setCellState(rect, cellState);
}
