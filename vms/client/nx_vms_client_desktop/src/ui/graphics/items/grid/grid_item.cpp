// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "grid_item.h"

#include <cmath>
#include <limits>

#include <QtGui/QPainter>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QWidget>

#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/checked_cast.h>
#include <utils/math/color_transformations.h>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <ui/common/color_to_vector_converter.h>
#include <ui/workbench/workbench_grid_mapper.h>
#include <ui/animation/variant_animator.h>

#include "grid_highlight_item.h"

using nx::vms::client::core::Geometry;
using namespace nx::vms::client;
using namespace nx::vms::client::desktop;

namespace {
    const char *pointPropertyName = "_qn_itemPoint";
    const char *animatorPropertyName = "_qn_itemAnimator";
}

QnGridItem::QnGridItem(QGraphicsItem *parent):
    base_type(parent),
    m_boundingRect(Geometry::maxBoundingRect())
{
    m_colorByState[Allowed] = core::colorTheme()->color("scene.grid.allowedCell");
    m_colorByState[Disallowed] = core::colorTheme()->color("scene.grid.disallowedCell");

    setAcceptedMouseButtons(Qt::NoButton);

    connect(&m_scaleWatcher, &QnViewportScaleWatcher::scaleChanged, this,
        [this](qreal value)
        {
            static const auto kPixelLineWidth = 1;
            m_lineWidth = kPixelLineWidth * value;
        });

    /* Don't disable this item here. When disabled, it starts accepting wheel events
     * (and probably other events too). Looks like a Qt bug. */
}

QnGridItem::~QnGridItem() {
    return;
}

QnWorkbenchGridMapper* QnGridItem::mapper() const {
    return m_mapper.data();
}

void QnGridItem::setMapper(QnWorkbenchGridMapper *mapper) {
    m_mapper = mapper;
}

QRectF QnGridItem::boundingRect() const {
    return m_boundingRect;
}

QColor QnGridItem::stateColor(int cellState) const {
    return m_colorByState.value(cellState, QColor(0, 0, 0, 0));
}

int QnGridItem::cellState(const QPoint &cell) const {
    QHash<QPoint, PointData>::const_iterator pos = m_dataByCell.find(cell);
    if(pos == m_dataByCell.end())
        return Initial;

    return pos->state;
}

void QnGridItem::setCellState(const QPoint &cell, int cellState) {
    if(mapper() == nullptr)
        return;

    PointData &data = m_dataByCell[cell];
    if(data.state == cellState)
        return;
    data.state = cellState;

    if(data.item == nullptr) {
        data.item = newHighlightItem();

        qreal d =  m_lineWidth / 2;
        data.item->setRect(Geometry::dilated(mapper()->mapFromGrid(QRect(cell, cell)), mapper()->spacing() / 2).adjusted(d, d, -d, -d));
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

void QnGridItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *widget) {
    if(mapper() == nullptr)
        return;

    if(qFuzzyIsNull(painter->opacity()))
        return;

    if (!widget)
        widget = scene()->views().front();

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
    const QPen pen(core::colorTheme()->color("brand_d7"), m_lineWidth);
    QnScopedPainterPenRollback penRollback(painter, pen);
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);
    QPointF topLeft = mapper()->mapFromGrid(gridRect.topLeft()) - QPointF(mapper()->spacing() / 2, mapper()->spacing() / 2);
    QPointF delta = Geometry::toPoint(mapper()->step());

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
    if(animator != nullptr)
        return animator;

    animator = new VariantAnimator(item);
    animator->setTargetObject(item);
    animator->setAccessor(new PropertyAccessor("color"));
    animator->setConverter(new QnColorToVectorConverter());
    animator->setTimeLimit(300);

    registerAnimation(animator);
    connect(animator, SIGNAL(finished()), this, SLOT(at_itemAnimator_finished()));
    item->setProperty(animatorPropertyName, QVariant::fromValue<VariantAnimator *>(animator));
    return animator;
}

void QnGridItem::at_itemAnimator_finished() {
    VariantAnimator *animator = checked_cast<VariantAnimator *>(sender());
    QnGridHighlightItem *item = checked_cast<QnGridHighlightItem *>(animator->targetObject());

    QPoint cell = itemCell(item);
    PointData &data = m_dataByCell[cell];
    if(data.state == Initial) {
        data.item = nullptr;
        m_freeItems.push_back(item);
        setItemCell(item, QPoint(0, 0));
    }
}

QVariant QnGridItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == QGraphicsItem::ItemSceneHasChanged)
        m_scaleWatcher.initialize(scene());

    return base_type::itemChange(change, value);
}
