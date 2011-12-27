#include "grid_item.h"
#include <limits>
#include <cmath>
#include <QPainter>
#include <utils/common/scoped_painter_rollback.h>
#include <ui/common/scene_utility.h>
#include <ui/workbench/workbench_grid_mapper.h>
#include <ui/animation/variant_animator.h>

//QColor color(0, 240, 240);

QnGridItem::QnGridItem(QGraphicsItem *parent):
    QGraphicsObject(parent),
    m_lineWidth(1.0),
    m_colorAnimator(new QnVariantAnimator(this))
{
    qreal d = std::numeric_limits<qreal>::max() / 4;
    m_boundingRect = QRectF(QPointF(-d, -d), QPointF(d, d));

    m_colorAnimator->setAccessor(new QnPropertyAccessor("color"));
    m_colorAnimator->setConverter(new QnColorToVectorConverter());
    m_colorAnimator->setSpeed(1.0);
}

QnGridItem::~QnGridItem() {
    return;
}

void QnGridItem::setMapper(QnWorkbenchGridMapper *mapper) {
    m_mapper = mapper;
}

void QnGridItem::setFadingSpeed(qreal speed) {
    m_colorAnimator->setSpeed(speed);
}

void QnGridItem::fadeIn() {
    m_colorAnimator->pause();
    m_colorAnimator->setTargetValue(m_defaultColor);
    m_colorAnimator->start();
}

void QnGridItem::fadeOut() {
    m_colorAnimator->pause();
    m_colorAnimator->setTargetValue(SceneUtility::translucent(m_defaultColor));
    m_colorAnimator->start();
}

QRectF QnGridItem::boundingRect() const {
    return m_boundingRect;
}

void QnGridItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *widget) {
    if(mapper() == NULL)
        return;

    if(m_color.alpha() == 0)
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
    QPointF topLeft = mapper()->mapToGrid(QPoint(gridRect.left(), gridRect.top())) - SceneUtility::toPoint(mapper()->spacing()) / 2;
    QPointF delta = SceneUtility::toPoint(mapper()->step());

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

