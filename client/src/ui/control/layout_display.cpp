#include "layout_display.h"
#include <cassert>
#include <cmath> /* For fmod. */

#include <ui/animation/viewport_animator.h>
#include <ui/animation/widget_animator.h>
#include <ui/animation/curtain_animator.h>

#include <ui/model/layout_model.h>
#include <ui/model/resource_item_model.h>
#include <ui/model/layout_grid_mapper.h>

#include <ui/graphics/instruments/instrumentmanager.h>
#include <ui/graphics/instruments/boundinginstrument.h>
#include <ui/graphics/instruments/transformlistenerinstrument.h>
#include <ui/graphics/instruments/activitylistenerinstrument.h>

#include <ui/graphics/items/display_widget.h>
#include <ui/graphics/items/curtain_item.h>

#include <utils/common/warnings.h>

#include "display_state.h"

namespace {
    void calculateExpansionValues(qreal start, qreal end, qreal center, qreal newLength, qreal *deltaStart, qreal *deltaEnd) {
        qreal newStart = center - newLength / 2;
        qreal newEnd = center + newLength / 2;

        if(newStart > start) {
            newEnd += start - newStart;
            newStart = start;
        }

        if(newEnd < end) {
            newStart += end - newEnd;
            newEnd = end;
        }

        *deltaStart = newStart - start;
        *deltaEnd = newEnd - end;
    }

    /** Size multiplier for focused widgets. */
    const qreal focusExpansion = 1.5;

    /** Maximal expanded size of a focused widget, relative to viewport size. */
    const qreal maxExpandedSize = 0.8;

    /** Viewport lower size boundary, in scene coordinates. */
    const QSizeF viewportLowerSizeBound = QSizeF(1.0, 1.0);

    const int widgetAnimationDurationMsec = 250;
    const int zoomAnimationDurationMsec = 250;

    /** The amount of z-space that one layer occupies. */
    const qreal layerZSize = 10000000.0;

    /** The amount that is added to maximal Z value each time a move to front
     * operation is performed. */
    const qreal zStep = 1.0;

} // anonymous namespace

QnLayoutDisplay::QnLayoutDisplay(QGraphicsScene *scene, QGraphicsView *view, QObject *parent):
    QObject(parent),
    m_state(NULL),
    m_manager(new InstrumentManager(this)),
    m_scene(scene),
    m_view(view),
    m_viewportAnimator(new QnViewportAnimator(view, this)),
    m_frontZ(0.0)
{
    assert(scene != NULL && view != NULL);

    /* Prepare manager. */
    m_manager->registerScene(scene);
    m_manager->registerView(view);
    
    /* Scene indexing will only slow everything down. */ 
    scene->setItemIndexMethod(QGraphicsScene::NoIndex);

    /* Configure OpenGL */
    if (!QGLFormat::hasOpenGL()) {
        qnCritical("Software rendering is not supported.");
    } else {
        QGLFormat glFormat;
        glFormat.setOption(QGL::SampleBuffers); /* Multisampling. */
        glFormat.setSwapInterval(1); /* Turn vsync on. */
        view->setViewport(new QGLWidget(glFormat));
    }

    /* Turn on antialiasing at QPainter level. */
    view->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing); 
    view->setRenderHints(QPainter::SmoothPixmapTransform);

    /* In OpenGL mode this one seems to be ignored, but it will help in software mode. */
    view->setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);

    /* All our items save and restore painter state. */
    view->setOptimizationFlag(QGraphicsView::DontSavePainterState, false); /* Can be turned on if we won't be using framed widgets. */ 
    view->setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing);

    /* Don't even try to uncomment this one, it slows everything down. */
    //setCacheMode(QGraphicsView::CacheBackground);

    /* We don't need scrollbars & frame. */
    view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view->setFrameShape(QFrame::NoFrame);

    /* Create and configure instruments. */
    m_boundingInstrument = new BoundingInstrument(this);
    m_transformListenerInstrument = new TransformListenerInstrument(this);
    m_activityListenerInstrument = new ActivityListenerInstrument(1000, this);
    m_manager->installInstrument(m_transformListenerInstrument);
    m_manager->installInstrument(m_boundingInstrument);
    m_manager->installInstrument(m_activityListenerInstrument);

    m_boundingInstrument->setSizeEnforced(view, true);
    m_boundingInstrument->setPositionEnforced(view, true);
    m_boundingInstrument->setScalingSpeed(view, 32.0);
    m_boundingInstrument->setMovementSpeed(view, 4.0);

    m_activityListenerInstrument->recursiveDisable();

    connect(m_transformListenerInstrument, SIGNAL(transformChanged(QGraphicsView *)),                   this,                   SLOT(at_viewport_transformationChanged()));
    connect(m_activityListenerInstrument,  SIGNAL(activityStopped()),                                   this,                   SLOT(at_activityStopped()));
    connect(m_activityListenerInstrument,  SIGNAL(activityStarted()),                                   this,                   SLOT(at_activityStarted()));

    /* Set up curtain. */
    m_curtainItem = new QnCurtainItem();
    m_scene->addItem(m_curtainItem);
    setLayer(m_curtainItem, CURTAIN_LAYER);
    m_curtainAnimator = new QnCurtainAnimator(m_curtainItem, 1000, this);
    connect(m_curtainAnimator,  SIGNAL(curtained()),                                                    this,                   SLOT(at_curtained()));
    connect(m_curtainAnimator,  SIGNAL(uncurtained()),                                                  this,                   SLOT(at_uncurtained()));

    /* Configure animator. */
    m_viewportAnimator->setMovementSpeed(4.0);
    m_viewportAnimator->setScalingSpeed(32.0);
    connect(m_viewportAnimator, SIGNAL(animationStarted()),                                             this,                   SIGNAL(viewportGrabbed()));
    connect(m_viewportAnimator, SIGNAL(animationStarted()),                                             m_boundingInstrument,   SLOT(recursiveDisable()));
    connect(m_viewportAnimator, SIGNAL(animationFinished()),                                            this,                   SIGNAL(viewportUngrabbed()));
    connect(m_viewportAnimator, SIGNAL(animationFinished()),                                            m_boundingInstrument,   SLOT(recursiveEnable()));
    connect(m_viewportAnimator, SIGNAL(animationFinished()),                                            this,                   SLOT(at_viewport_animationFinished()));


    /* Configure viewport updates. */
    m_updateTimer = new AnimationTimer(this);
    m_updateTimer->setListener(this);
    m_updateTimer->start();

}

QnLayoutDisplay::~QnLayoutDisplay() {
}

void QnLayoutDisplay::setState(QnDisplayState *state) {
    if(m_state != NULL) {
        disconnect(m_state, NULL, this, NULL);

        foreach(QnLayoutItemModel *item, m_state->layout()->items())
            removeItemInternal(item);
    }

    m_state = state;

    if(m_state != NULL) {
        /* Subscribe to state changes. */
        connect(m_state,            SIGNAL(aboutToBeDestroyed()),                                           this,                   SLOT(at_state_aboutToBeDestroyed()));
        connect(m_state,            SIGNAL(modeChanged()),                                                  this,                   SLOT(at_state_modeChanged()));
        connect(m_state,            SIGNAL(selectedItemChanged(QnLayoutItemModel *, QnLayoutItemModel *)),  this,                   SLOT(at_state_selectedItemChanged(QnLayoutItemModel *, QnLayoutItemModel *)));
        connect(m_state,            SIGNAL(zoomedItemChanged(QnLayoutItemModel *, QnLayoutItemModel *)),    this,                   SLOT(at_state_zoomedItemChanged(QnLayoutItemModel *, QnLayoutItemModel *)));
        connect(m_state->layout(),  SIGNAL(itemAdded(QnLayoutItemModel *)),                                 this,                   SLOT(at_layout_itemAdded(QnLayoutItemModel *)));
        connect(m_state->layout(),  SIGNAL(itemAboutToBeRemoved(QnLayoutItemModel *)),                      this,                   SLOT(at_layout_itemAboutToBeRemoved(QnLayoutItemModel *)));
        
        /* Create items. */
        foreach(QnLayoutItemModel *item, m_state->layout()->items())
            addItemInternal(item);

        /* Fire signals if needed. */
        if(m_state->zoomedItem() != NULL)
            at_state_zoomedItemChanged(NULL, m_state->zoomedItem());
        if(m_state->selectedItem() != NULL)
            at_state_selectedItemChanged(NULL, m_state->selectedItem());

        synchronizeSceneBounds();
    }
}

// -------------------------------------------------------------------------- //
// QnLayoutDisplay :: item properties
// -------------------------------------------------------------------------- //
QnLayoutDisplay::Layer QnLayoutDisplay::layer(QGraphicsItem *item) const {
     QHash<QGraphicsItem *, ItemProperties>::const_iterator pos = m_propertiesByItem.find(item);
     if(pos == m_propertiesByItem.end())
         return BACK_LAYER;

    return pos->layer;
}

void QnLayoutDisplay::setLayer(QGraphicsItem *item, Layer layer) {
    if(item == NULL) {
        qnNullWarning(item);
        return;
    }

    ItemProperties &properties = m_propertiesByItem[item];

    properties.layer = layer;
    item->setZValue(layer * layerZSize + fmod(item->zValue(), layerZSize));
}

QnWidgetAnimator *QnLayoutDisplay::animator(QnDisplayWidget *widget) {
    ItemProperties &properties = m_propertiesByItem[widget];
    if(properties.animator != NULL)
        return properties.animator;

    /* Create if it's not there. */
    properties.animator = new QnWidgetAnimator(widget, "enclosingGeometry", "rotation", widget);
    properties.animator->setMovementSpeed(4.0);
    properties.animator->setScalingSpeed(32.0);
    properties.animator->setRotationSpeed(720.0);
    return properties.animator;
}

QnDisplayWidget *QnLayoutDisplay::widget(QnLayoutItemModel *item) const {
    return m_widgetByItem[item];
}


// -------------------------------------------------------------------------- //
// QnLayoutDisplay :: mutators
// -------------------------------------------------------------------------- //
void QnLayoutDisplay::disableViewportChanges() {
    m_boundingInstrument->dontEnforcePosition(m_view);
    m_boundingInstrument->dontEnforceSize(m_view);
}

void QnLayoutDisplay::enableViewportChanges() {
    m_boundingInstrument->enforcePosition(m_view);
    m_boundingInstrument->enforceSize(m_view);
}

void QnLayoutDisplay::fitInView() {
    if(m_state == NULL)
        return;

    if(m_state->zoomedItem() != NULL) {
        m_viewportAnimator->moveTo(itemGeometry(m_state->zoomedItem()), zoomAnimationDurationMsec);
    } else {
        m_viewportAnimator->moveTo(layoutBoundingGeometry(), zoomAnimationDurationMsec);
    }
}

void QnLayoutDisplay::bringToFront(const QList<QGraphicsItem *> &items) {
    foreach(QGraphicsItem *item, items)
        bringToFront(item);
}

void QnLayoutDisplay::bringToFront(QGraphicsItem *item) {
    if(item == NULL) {
        qnNullWarning(item);
        return;
    }

    m_frontZ += zStep;
    item->setZValue(layerFront(layer(item)));
}

void QnLayoutDisplay::bringToFront(QnLayoutItemModel *item) {
    if(item == NULL) {
        qnNullWarning(item);
        return;
    }

    bringToFront(widget(item));
}

void QnLayoutDisplay::addItemInternal(QnLayoutItemModel *item) {
    QnDisplayWidget *widget = new QnDisplayWidget(item);
    m_scene->addItem(widget);

    widget->setFlag(QGraphicsItem::ItemIgnoresParentOpacity, true); /* Optimization. */
    widget->setFlag(QGraphicsItem::ItemIsSelectable, true);
    widget->setFlag(QGraphicsItem::ItemIsFocusable, true);
    widget->setFlag(QGraphicsItem::ItemIsMovable, true);

    widget->setFocusPolicy(Qt::StrongFocus);
    widget->setWindowFlags(Qt::Window);

    /* Unsetting this flag is VERY important. If it is set, graphics scene
     * will mess with widget's z value and bring it to front every time 
     * it is clicked. This will wreak havoc in our layered system.
     * 
     * Note that this flag must be unset after Qt::Window window flag is set
     * because the latter automatically sets the former. */
    widget->setFlag(QGraphicsItem::ItemIsPanel, false);

    connect(item, SIGNAL(geometryChanged(const QRect &, const QRect &)),                            this, SLOT(at_item_geometryChanged()));
    connect(item, SIGNAL(geometryDeltaChanged(const QRectF &, const QRectF &)),                     this, SLOT(at_item_geometryDeltaChanged()));
    connect(item, SIGNAL(rotationChanged(qreal, qreal)),                                            this, SLOT(at_item_rotationChanged()));
    connect(item, SIGNAL(flagsChanged(QnLayoutItemModel::ItemFlags, QnLayoutItemModel::ItemFlags)), this, SLOT(at_item_flagsChanged()));

    m_widgetByItem.insert(item, widget);

    synchronize(widget, false);
    bringToFront(widget);
}

void QnLayoutDisplay::removeItemInternal(QnLayoutItemModel *item) {
    disconnect(item, NULL, this, NULL);

    QnDisplayWidget *widget = m_widgetByItem[item];
    m_widgetByItem.remove(item);

    delete widget;
}


// -------------------------------------------------------------------------- //
// QnLayoutDisplay :: calculators
// -------------------------------------------------------------------------- //
qreal QnLayoutDisplay::layerFront(Layer layer) const {
    return m_frontZ + layer * layerZSize;
}

QnLayoutDisplay::Layer QnLayoutDisplay::synchronizedLayer(QnLayoutItemModel *item) const {
    assert(item != NULL);
    assert(m_state != NULL);
    
    if(item == m_state->zoomedItem()) {
        return ZOOMED_LAYER;
    } else if(item->isPinned()) {
        if(item == m_state->selectedItem()) {
            return PINNED_SELECTED_LAYER;
        } else {
            return PINNED_LAYER;
        }
    } else {
        if(item == m_state->selectedItem()) {
            return UNPINNED_SELECTED_LAYER;
        } else {
            return UNPINNED_LAYER;
        }
    }
}

QRectF QnLayoutDisplay::itemEnclosingGeometry(QnLayoutItemModel *item) const {
    if(item == NULL) {
        qnNullWarning(item);
        return QRectF();
    }

    if(m_state == NULL) {
        qnWarning("State is not set.");
        return QRectF();
    }

    QRectF result = m_state->mapper()->mapFromGrid(item->geometry());

    QSizeF step = m_state->mapper()->step();
    QRectF delta = item->geometryDelta();
    result = QRectF(
        result.left()   + delta.left()   * step.width(),
        result.top()    + delta.top()    * step.height(),
        result.width()  + delta.width()  * step.width(),
        result.height() + delta.height() * step.height()
    );

    return result;
}

QRectF QnLayoutDisplay::itemGeometry(QnLayoutItemModel *item, QRectF *enclosingGeometry) const {
    if(item == NULL) {
        qnNullWarning(item);
        return QRectF();
    }

    QRectF result = itemEnclosingGeometry(item);
    if(enclosingGeometry != NULL)
        *enclosingGeometry = result;

    QnDisplayWidget *widget = this->widget(item);
    if(!widget->hasAspectRatio())
        return result;

    return expanded(widget->aspectRatio(), result, Qt::KeepAspectRatio);
}

QRectF QnLayoutDisplay::layoutBoundingGeometry() const {
    if(m_state == NULL)
        return QRectF();

    return m_state->mapper()->mapFromGrid(m_state->layout()->boundingRect().adjusted(-1, -1, 1, 1));
}

QRectF QnLayoutDisplay::viewportGeometry() const {
    return mapRectToScene(m_view, m_view->viewport()->rect());
}


// -------------------------------------------------------------------------- //
// QnLayoutDisplay :: synchronizers
// -------------------------------------------------------------------------- //
void QnLayoutDisplay::synchronize(QnLayoutItemModel *item, bool animate) {
    if(item == NULL) {
        qnNullWarning(item);
        return;
    }

    synchronize(widget(item), animate);
}

void QnLayoutDisplay::synchronize(QnDisplayWidget *widget, bool animate) {
    if(widget == NULL) {
        qnNullWarning(widget);
        return;
    }

    if(m_state == NULL) {
        qnWarning("State is not set.");
        return;
    }

    synchronizeGeometry(widget, animate);
    synchronizeLayer(widget);
}

void QnLayoutDisplay::synchronizeGeometry(QnLayoutItemModel *item, bool animate) {
    synchronizeGeometry(widget(item), animate);
}

void QnLayoutDisplay::synchronizeGeometry(QnDisplayWidget *widget, bool animate) {
    assert(widget != NULL);
    assert(m_state != NULL);

    QnLayoutItemModel *item = widget->item();

    QRectF enclosingGeometry = itemEnclosingGeometry(item);

    /* Adjust for selection. */
    if(item == m_state->selectedItem() && item != m_state->zoomedItem()) {
        QPointF geometryCenter = enclosingGeometry.center();
        QTransform transform;
        transform.translate(geometryCenter.x(), geometryCenter.y());
        transform.rotate(-item->rotation());
        transform.translate(-geometryCenter.x(), -geometryCenter.y());

        QRectF viewportGeometry = transform.mapRect(mapRectToScene(m_view, m_view->viewport()->rect()));

        QSizeF newWidgetSize = enclosingGeometry.size() * focusExpansion;
        QSizeF maxWidgetSize = viewportGeometry.size() * maxExpandedSize;
        QPointF viewportCenter = viewportGeometry.center();

        /* Allow expansion no further than the maximal size, but no less than current size. */
        newWidgetSize =  bounded(newWidgetSize, maxWidgetSize,   Qt::KeepAspectRatio);
        newWidgetSize = expanded(newWidgetSize, enclosingGeometry.size(), Qt::KeepAspectRatio);

        /* Calculate expansion values. Expand towards the screen center. */
        qreal xp1 = 0.0, xp2 = 0.0, yp1 = 0.0, yp2 = 0.0;
        calculateExpansionValues(enclosingGeometry.left(), enclosingGeometry.right(),  viewportCenter.x(), newWidgetSize.width(),  &xp1, &xp2);
        calculateExpansionValues(enclosingGeometry.top(),  enclosingGeometry.bottom(), viewportCenter.y(), newWidgetSize.height(), &yp1, &yp2);

        enclosingGeometry = enclosingGeometry.adjusted(xp1, yp1, xp2, yp2);
    }

    /* Update Z value. */
    if(item == m_state->selectedItem() || item == m_state->zoomedItem())
        bringToFront(widget);
    
    /* Update enclosing aspect ratio. */
    widget->setEnclosingAspectRatio(enclosingGeometry.width() / enclosingGeometry.height());

    /* Move! */
    QnWidgetAnimator *animator = this->animator(widget);
    if(animate) {
        animator->moveTo(enclosingGeometry, item->rotation(), widgetAnimationDurationMsec);
    } else {
        animator->stopAnimation();
        widget->setEnclosingGeometry(enclosingGeometry);
        widget->setRotation(item->rotation());
    }
}

void QnLayoutDisplay::synchronizeLayer(QnLayoutItemModel *item) {
    synchronizeLayer(widget(item));
}

void QnLayoutDisplay::synchronizeLayer(QnDisplayWidget *widget) {
    setLayer(widget, synchronizedLayer(widget->item()));
}

void QnLayoutDisplay::synchronizeSceneBounds() {
    assert(m_state != NULL);

    QRectF rect = m_state->zoomedItem() != NULL ? itemGeometry(m_state->zoomedItem()) : layoutBoundingGeometry();
    m_boundingInstrument->setPositionBounds(m_view, rect, 0.0);
    m_boundingInstrument->setSizeBounds(m_view, viewportLowerSizeBound, Qt::KeepAspectRatioByExpanding, rect.size(), Qt::KeepAspectRatioByExpanding);
}


// -------------------------------------------------------------------------- //
// QnLayoutDisplay :: handlers
// -------------------------------------------------------------------------- //
void QnLayoutDisplay::tick(int /*currentTime*/) {
    m_view->viewport()->update();
}

void QnLayoutDisplay::at_viewport_animationFinished() {
    if(m_state != NULL)
        synchronizeSceneBounds();
}

void QnLayoutDisplay::at_layout_itemAdded(QnLayoutItemModel *item) {
    addItemInternal(item);
    synchronizeSceneBounds();
}

void QnLayoutDisplay::at_layout_itemAboutToBeRemoved(QnLayoutItemModel *item) {
    removeItemInternal(item);
    synchronizeSceneBounds();
}

void QnLayoutDisplay::at_state_aboutToBeDestroyed() {
    setState(NULL);
}

void QnLayoutDisplay::at_state_modeChanged() {
    if(m_state->mode() == QnDisplayState::EDITING) {
        m_boundingInstrument->recursiveDisable();
    } else {
        m_boundingInstrument->recursiveEnable();
    }
}

void QnLayoutDisplay::at_state_selectedItemChanged(QnLayoutItemModel *oldSelectedItem, QnLayoutItemModel *newSelectedItem) {
    if(oldSelectedItem != NULL)
        synchronize(oldSelectedItem, true);

    if(newSelectedItem != NULL) {
        bringToFront(newSelectedItem);
        synchronize(newSelectedItem, true);
    }
}

void QnLayoutDisplay::at_state_zoomedItemChanged(QnLayoutItemModel *oldZoomedItem, QnLayoutItemModel *newZoomedItem) {
    if(oldZoomedItem != NULL) {
        synchronize(oldZoomedItem, true);
        
        m_activityListenerInstrument->recursiveDisable();
    }

    if(newZoomedItem != NULL) {
        bringToFront(newZoomedItem);
        synchronize(newZoomedItem, true);

        m_activityListenerInstrument->recursiveEnable();

        m_viewportAnimator->moveTo(itemGeometry(m_state->zoomedItem()), zoomAnimationDurationMsec);
    } else {
        m_viewportAnimator->moveTo(layoutBoundingGeometry(), zoomAnimationDurationMsec);
    }

    synchronizeSceneBounds();
}

void QnLayoutDisplay::at_viewport_transformationChanged() {
    if(m_state != NULL && m_state->selectedItem() != NULL) {
        QnDisplayWidget *widget = this->widget(m_state->selectedItem());
        synchronizeGeometry(widget, animator(widget)->isAnimating());
    }
}

void QnLayoutDisplay::at_item_geometryChanged() {
    synchronizeGeometry(static_cast<QnLayoutItemModel *>(sender()), true);
    synchronizeSceneBounds();
}

void QnLayoutDisplay::at_item_geometryDeltaChanged() {
    synchronizeGeometry(static_cast<QnLayoutItemModel *>(sender()), true);
}

void QnLayoutDisplay::at_item_rotationChanged() {
    synchronizeGeometry(static_cast<QnLayoutItemModel *>(sender()), true);
}

void QnLayoutDisplay::at_item_flagsChanged() {
    synchronizeLayer(static_cast<QnLayoutItemModel *>(sender()));
}

void QnLayoutDisplay::at_activityStopped() {
    if(m_state == NULL)
        return;

    m_curtainAnimator->curtain(widget(m_state->zoomedItem()));
}

void QnLayoutDisplay::at_activityStarted() {
    m_curtainAnimator->uncurtain();
}

void QnLayoutDisplay::at_curtained() {
    if(m_state == NULL)
        return;

    foreach(QnDisplayWidget *widget, m_widgetByItem)
        if(widget->item() != m_state->zoomedItem())
            widget->hide();

    m_view->viewport()->setCursor(QCursor(Qt::BlankCursor));
}

void QnLayoutDisplay::at_uncurtained() {
    foreach(QnDisplayWidget *widget, m_widgetByItem)
        widget->show();

    m_view->viewport()->setCursor(QCursor(Qt::ArrowCursor));
}
