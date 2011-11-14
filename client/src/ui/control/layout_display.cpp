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

    const QSizeF defaultCellSize = QSizeF(150.0, 100.0);
    const QSizeF defaultSpacing = QSizeF(25.0, 25.0);

    const int widgetAnimationDurationMsec = 250;
    const int zoomAnimationDurationMsec = 250;

    /** The amount of z-space that one layer occupies. */
    const qreal layerZSize = 10000000.0;

    /** The amount that is added to maximal Z value each time a move to front
     * operation is performed. */
    const qreal zStep = 1.0;

} // anonymous namespace

QnLayoutDisplay::QnLayoutDisplay(QnDisplayState *state, QGraphicsScene *scene, QGraphicsView *view, QObject *parent):
    QObject(parent),
    m_state(state),
    m_manager(new InstrumentManager(this)),
    m_scene(scene),
    m_view(view),
    m_viewportAnimator(new QnViewportAnimator(view, this)),
    m_frontZ(0.0)
{
    assert(state != NULL && scene != NULL && view != NULL);

    /* Provide some meaningful defaults for the grid. */
    m_state->gridMapper()->setCellSize(defaultCellSize);
    m_state->gridMapper()->setSpacing(defaultSpacing);

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

    connect(m_transformListenerInstrument, SIGNAL(transformChanged(QGraphicsView *)), this, SLOT(at_viewport_transformationChanged()));
    connect(m_activityListenerInstrument,  SIGNAL(activityStopped()),                 this, SLOT(at_activityStopped()));
    connect(m_activityListenerInstrument,  SIGNAL(activityStarted()),                 this, SLOT(at_activityStarted()));

    /* Set up curtain. */
    m_curtainItem = new QnCurtainItem();
    m_scene->addItem(m_curtainItem);
    setLayer(m_curtainItem, CURTAIN_LAYER);
    m_curtainAnimator = new QnCurtainAnimator(m_curtainItem, 1000, this);
    connect(m_curtainAnimator, SIGNAL(curtained()),     this, SLOT(at_curtained()));
    connect(m_curtainAnimator, SIGNAL(uncurtained()),   this, SLOT(at_uncurtained()));

    /* Configure animator. */
    m_viewportAnimator->setMovementSpeed(4.0);
    m_viewportAnimator->setScalingSpeed(32.0);
    connect(m_viewportAnimator, SIGNAL(animationStarted()),  this,                 SIGNAL(viewportGrabbed()));
    connect(m_viewportAnimator, SIGNAL(animationStarted()),  m_boundingInstrument, SLOT(recursiveDisable()));
    connect(m_viewportAnimator, SIGNAL(animationFinished()), this,                 SIGNAL(viewportUngrabbed()));
    connect(m_viewportAnimator, SIGNAL(animationFinished()), m_boundingInstrument, SLOT(recursiveEnable()));
    connect(m_viewportAnimator, SIGNAL(animationFinished()), this,                 SLOT(synchronizeSceneBounds()));

    /* Subscribe to state changes and create items. */
    connect(m_state,          SIGNAL(modeChanged()),                                                this, SLOT(at_state_modeChanged()));
    connect(m_state,          SIGNAL(selectedEntityChanged(QnLayoutItemModel *, QnLayoutItemModel *)),    this, SLOT(at_state_selectedEntityChanged(QnLayoutItemModel *, QnLayoutItemModel *)));
    connect(m_state,          SIGNAL(zoomedEntityChanged(QnLayoutItemModel *, QnLayoutItemModel *)),      this, SLOT(at_state_zoomedEntityChanged(QnLayoutItemModel *, QnLayoutItemModel *)));
    connect(m_state->model(), SIGNAL(itemAdded(QnLayoutItemModel *)),                                  this, SLOT(at_model_itemAdded(QnLayoutItemModel *)));
    connect(m_state->model(), SIGNAL(itemAboutToBeRemoved(QnLayoutItemModel *)),                       this, SLOT(at_model_itemAboutToBeRemoved(QnLayoutItemModel *)));
    foreach(QnLayoutItemModel *entity, m_state->model()->items())
        at_model_itemAdded(entity);

    /* Configure viewport updates. */
    m_updateTimer = new AnimationTimer(this);
    m_updateTimer->setListener(this);
    m_updateTimer->start();

    /* Fire signals if needed. */
    if(m_state->zoomedEntity() != NULL)
        at_state_zoomedEntityChanged(NULL, m_state->zoomedEntity());
    if(m_state->selectedEntity() != NULL)
        at_state_selectedEntityChanged(NULL, m_state->selectedEntity());
}

QnLayoutDisplay::~QnLayoutDisplay() {
    foreach(QnLayoutItemModel *entity, m_state->model()->items())
        at_model_itemAboutToBeRemoved(entity);
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
    if(m_state->zoomedEntity() != NULL) {
        m_viewportAnimator->moveTo(zoomedItemGeometry(), zoomAnimationDurationMsec);
    } else {
        m_viewportAnimator->moveTo(boundingGeometry(), zoomAnimationDurationMsec);
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
    item->setZValue(layerFrontZ(layer(item)));
}

void QnLayoutDisplay::bringToFront(QnLayoutItemModel *entity) {
    if(entity == NULL) {
        qnNullWarning(entity);
        return;
    }

    if(entity->layout() != m_state->model()) {
        qnWarning("Cannot bring to front an entity from another model.");
        return;
    }

    bringToFront(m_widgetByEntity[entity]);
}


// -------------------------------------------------------------------------- //
// QnLayoutDisplay :: calculators
// -------------------------------------------------------------------------- //
qreal QnLayoutDisplay::layerFrontZ(Layer layer) const {
    return m_frontZ + layer * layerZSize;
}

QnLayoutDisplay::Layer QnLayoutDisplay::entityLayer(QnLayoutItemModel *entity) const {
    if(entity == m_state->zoomedEntity()) {
        return ZOOMED_LAYER;
    } else if(entity->isPinned()) {
        if(entity == m_state->selectedEntity()) {
            return PINNED_SELECTED_LAYER;
        } else {
            return PINNED_LAYER;
        }
    } else {
        if(entity == m_state->selectedEntity()) {
            return UNPINNED_SELECTED_LAYER;
        } else {
            return UNPINNED_LAYER;
        }
    }
}

QRectF QnLayoutDisplay::itemGeometry(QnLayoutItemModel *item) const {
    QRectF geometry = m_state->gridMapper()->mapFromGrid(item->geometry());

    QSizeF step = m_state->gridMapper()->step();
    QRectF delta = item->geometryDelta();
    geometry = QRectF(
        geometry.left()   + delta.left()   * step.width(),
        geometry.top()    + delta.top()    * step.height(),
        geometry.width()  + delta.width()  * step.width(),
        geometry.height() + delta.height() * step.height()
    );

    return geometry;
}

QRectF QnLayoutDisplay::boundingGeometry() const {
    return m_state->gridMapper()->mapFromGrid(m_state->model()->boundingRect().adjusted(-1, -1, 1, 1));
}

QRectF QnLayoutDisplay::zoomedItemGeometry() const {
    if(m_state->zoomedEntity() == NULL)
        return QRectF();

    return itemGeometry(m_state->zoomedEntity());
}

QRectF QnLayoutDisplay::viewportGeometry() const {
    return mapRectToScene(m_view, m_view->viewport()->rect());
}


// -------------------------------------------------------------------------- //
// QnLayoutDisplay :: synchronizers
// -------------------------------------------------------------------------- //
void QnLayoutDisplay::synchronize(QnLayoutItemModel *entity, bool animate) {
    synchronize(m_widgetByEntity[entity], animate);
}

void QnLayoutDisplay::synchronize(QnDisplayWidget *widget, bool animate) {
    synchronizeGeometry(widget, animate);
    synchronizeLayer(widget);
}

void QnLayoutDisplay::synchronizeGeometry(QnLayoutItemModel *entity, bool animate) {
    synchronizeGeometry(m_widgetByEntity[entity], animate);
}

void QnLayoutDisplay::synchronizeGeometry(QnDisplayWidget *widget, bool animate) {
    QnLayoutItemModel *entity = widget->item();

    QRectF geometry = itemGeometry(entity);

    /* Adjust for selection. */
    if(entity == m_state->selectedEntity() && entity != m_state->zoomedEntity()) {
        QPointF geometryCenter = geometry.center();
        QTransform transform;
        transform.translate(geometryCenter.x(), geometryCenter.y());
        transform.rotate(-entity->rotation());
        transform.translate(-geometryCenter.x(), -geometryCenter.y());

        QRectF viewportGeometry = transform.mapRect(mapRectToScene(m_view, m_view->viewport()->rect()));

        QSizeF newWidgetSize = geometry.size() * focusExpansion;
        QSizeF maxWidgetSize = viewportGeometry.size() * maxExpandedSize;
        QPointF viewportCenter = viewportGeometry.center();

        /* Allow expansion no further than the maximal size, but no less than current size. */
        newWidgetSize =  bounded(newWidgetSize, maxWidgetSize,   Qt::KeepAspectRatio);
        newWidgetSize = expanded(newWidgetSize, geometry.size(), Qt::KeepAspectRatio);

        /* Calculate expansion values. Expand towards the screen center. */
        qreal xp1 = 0.0, xp2 = 0.0, yp1 = 0.0, yp2 = 0.0;
        calculateExpansionValues(geometry.left(), geometry.right(),  viewportCenter.x(), newWidgetSize.width(),  &xp1, &xp2);
        calculateExpansionValues(geometry.top(),  geometry.bottom(), viewportCenter.y(), newWidgetSize.height(), &yp1, &yp2);

        geometry = geometry.adjusted(xp1, yp1, xp2, yp2);
    }

    /* Update Z value. */
    if(entity == m_state->selectedEntity() || entity == m_state->zoomedEntity())
        bringToFront(widget);
    
    /* Move! */
    QnWidgetAnimator *animator = this->animator(widget);
    if(animate) {
        animator->moveTo(geometry, entity->rotation(), widgetAnimationDurationMsec);
    } else {
        animator->stopAnimation();
        widget->setGeometry(geometry);
        widget->setRotation(entity->rotation());
    }
}

void QnLayoutDisplay::synchronizeLayer(QnLayoutItemModel *entity) {
    synchronizeLayer(m_widgetByEntity[entity]);
}

void QnLayoutDisplay::synchronizeLayer(QnDisplayWidget *widget) {
    setLayer(widget, entityLayer(widget->item()));
}

void QnLayoutDisplay::synchronizeSceneBounds() {
    QRectF boundingRect = m_state->zoomedEntity() != NULL ? zoomedItemGeometry() : boundingGeometry();

    m_boundingInstrument->setPositionBounds(m_view, boundingRect, 0.0);
    m_boundingInstrument->setSizeBounds(m_view, viewportLowerSizeBound, Qt::KeepAspectRatioByExpanding, boundingRect.size(), Qt::KeepAspectRatioByExpanding);
}


// -------------------------------------------------------------------------- //
// QnLayoutDisplay :: item properties
// -------------------------------------------------------------------------- //
QnLayoutDisplay::Layer QnLayoutDisplay::layer(QGraphicsItem *item) {
    ItemProperties &properties = m_propertiesByItem[item];

    return properties.layer;
}

void QnLayoutDisplay::setLayer(QGraphicsItem *item, Layer layer) {
    ItemProperties &properties = m_propertiesByItem[item];

    properties.layer = layer;
    item->setZValue(layer * layerZSize + fmod(item->zValue(), layerZSize));
}

QnWidgetAnimator *QnLayoutDisplay::animator(QnDisplayWidget *widget) {
    ItemProperties &properties = m_propertiesByItem[widget];
    if(properties.animator != NULL)
        return properties.animator;

    /* Create if it's not there. */
    properties.animator = new QnWidgetAnimator(widget, widget);
    properties.animator->setMovementSpeed(4.0);
    properties.animator->setScalingSpeed(32.0);
    properties.animator->setRotationSpeed(720.0);
    return properties.animator;
}


// -------------------------------------------------------------------------- //
// QnLayoutDisplay :: handlers
// -------------------------------------------------------------------------- //
void QnLayoutDisplay::tick(int /*currentTime*/) {
    m_view->viewport()->update();
}

void QnLayoutDisplay::at_model_itemAdded(QnLayoutItemModel *entity) {
    QnDisplayWidget *widget = new QnDisplayWidget(entity);
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

    connect(entity, SIGNAL(geometryChanged(const QRect &, const QRect &)),          this, SLOT(at_item_geometryChanged()));
    connect(entity, SIGNAL(geometryDeltaChanged(const QRectF &, const QRectF &)),   this, SLOT(at_item_geometryDeltaChanged()));
    connect(entity, SIGNAL(rotationChanged(qreal, qreal)),                          this, SLOT(at_item_rotationChanged()));
    connect(entity, SIGNAL(flagsChanged(ItemFlags, ItemFlags)),                     this, SLOT(at_item_flagsChanged()));

    m_widgetByEntity.insert(entity, widget);

    synchronize(widget, false);
    synchronizeSceneBounds();
    bringToFront(widget);
}

void QnLayoutDisplay::at_model_itemAboutToBeRemoved(QnLayoutItemModel *entity) {
    disconnect(entity, NULL, this, NULL);

    QnDisplayWidget *widget = m_widgetByEntity[entity];
    m_widgetByEntity.remove(entity);

    delete widget;

    synchronizeSceneBounds();
}

void QnLayoutDisplay::at_state_modeChanged() {
    if(m_state->mode() == QnDisplayState::EDITING) {
        m_boundingInstrument->recursiveDisable();
    } else {
        m_boundingInstrument->recursiveEnable();
    }
}

void QnLayoutDisplay::at_state_selectedEntityChanged(QnLayoutItemModel *oldSelectedEntity, QnLayoutItemModel *newSelectedEntity) {
    if(oldSelectedEntity != NULL)
        synchronize(oldSelectedEntity, true);

    if(newSelectedEntity != NULL) {
        bringToFront(newSelectedEntity);
        synchronize(newSelectedEntity, true);
    }
}

void QnLayoutDisplay::at_state_zoomedEntityChanged(QnLayoutItemModel *oldZoomedEntity, QnLayoutItemModel *newZoomedEntity) {
    if(oldZoomedEntity != NULL) {
        synchronize(oldZoomedEntity, true);
        
        m_activityListenerInstrument->recursiveDisable();
    }

    if(newZoomedEntity != NULL) {
        bringToFront(newZoomedEntity);
        synchronize(newZoomedEntity, true);

        m_activityListenerInstrument->recursiveEnable();

        m_viewportAnimator->moveTo(zoomedItemGeometry(), zoomAnimationDurationMsec);
    } else {
        m_viewportAnimator->moveTo(boundingGeometry(), zoomAnimationDurationMsec);
    }

    synchronizeSceneBounds();
}

void QnLayoutDisplay::at_viewport_transformationChanged() {
    if(m_state->selectedEntity() != NULL) {
        QnDisplayWidget *widget = m_widgetByEntity[m_state->selectedEntity()];
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
    m_curtainAnimator->curtain(m_widgetByEntity[m_state->zoomedEntity()]);
}

void QnLayoutDisplay::at_activityStarted() {
    m_curtainAnimator->uncurtain();
}

void QnLayoutDisplay::at_curtained() {
    foreach(QnDisplayWidget *widget, m_widgetByEntity)
        if(widget->item() != m_state->zoomedEntity())
            widget->hide();

    m_view->viewport()->setCursor(QCursor(Qt::BlankCursor));
}

void QnLayoutDisplay::at_uncurtained() {
    foreach(QnDisplayWidget *widget, m_widgetByEntity)
        widget->show();

    m_view->viewport()->setCursor(QCursor(Qt::ArrowCursor));
}
