#include "grid_background_item.h"

#include <QPainter>

#include <ui/animation/animation_timer.h>
#include <ui/animation/opacity_animator.h>
#include <ui/animation/rect_animator.h>
#include <ui/animation/variant_animator.h>

#include <ui/workbench/workbench_grid_mapper.h>

QnGridBackgroundItem::QnGridBackgroundItem(QGraphicsItem *parent):
    QGraphicsObject(parent),
    m_color(QColor(0, 0, 0, 0)),
    m_geometryAnimator(NULL),
    m_opacityAnimator(NULL)
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

void QnGridBackgroundItem::animatedHide() {
    m_targetOpacity = 0.0;
    if (!m_opacityAnimator)
        return;

    if (!m_opacityAnimator->isRunning())
        m_opacityAnimator->animateTo(m_targetOpacity);
}

void QnGridBackgroundItem::animatedShow() {
    m_targetOpacity = 0.7;
    if (!m_opacityAnimator)
        return;

    if (!m_opacityAnimator->isRunning())
        m_opacityAnimator->animateTo(m_targetOpacity);
}

void QnGridBackgroundItem::setMapper(QnWorkbenchGridMapper *mapper) {
    m_mapper = mapper;
    connect(mapper,     SIGNAL(originChanged()),    this,   SLOT(updateGeometry()));
    connect(mapper,     SIGNAL(cellSizeChanged()),  this,   SLOT(updateGeometry()));
    connect(mapper,     SIGNAL(spacingChanged()),   this,   SLOT(updateGeometry()));

    updateGeometry();
}


void QnGridBackgroundItem::setAnimationTimer(AnimationTimer *timer) {
    if (!m_geometryAnimator) {
        m_geometryAnimator = new RectAnimator(this);
        m_geometryAnimator->setTargetObject(this);
        m_geometryAnimator->setAccessor(new PropertyAccessor("viewportRect"));
    }

    if (!m_opacityAnimator) {
        m_opacityAnimator = opacityAnimator(this, 0.2);
        m_opacityAnimator->setTimeLimit(5000);
        connect(m_opacityAnimator, SIGNAL(finished()), this, SLOT(at_opacityAnimator_finished()));
    }

    m_geometryAnimator->setTimer(timer);
}

void QnGridBackgroundItem::setSceneRect(const QRect &rect) {
    if (m_sceneRect == rect)
        return;
    m_sceneRect = rect;
    updateGeometry();
}

void QnGridBackgroundItem::updateGeometry() {
    if(mapper() == NULL)
        return;

    QRectF targetRect = mapper()->mapFromGrid(m_sceneRect);
    if (m_geometryAnimator)
        m_geometryAnimator->animateTo(targetRect);
    else
        setViewportRect(targetRect);
}

void QnGridBackgroundItem::at_opacityAnimator_finished() {
    if (!qFuzzyCompare(this->opacity(), m_targetOpacity))
         m_opacityAnimator->animateTo(m_targetOpacity);
}

void QnGridBackgroundItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    painter->fillRect(m_rect, m_color);
}

