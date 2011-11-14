#include "display_synchronizer.h"
#include <cassert>
#include <cmath> /* For fmod. */
#include <ui/model/ui_layout_item.h>
#include <ui/model/ui_layout.h>
#include <ui/view/viewport_animator.h>
#include <ui/instruments/instrumentmanager.h>
#include <ui/instruments/boundinginstrument.h>
#include <ui/instruments/transformlistenerinstrument.h>
#include <ui/instruments/activitylistenerinstrument.h>
#include <utils/common/warnings.h>
#include "display_state.h"
#include "display_widget.h"
#include "display_grid_mapper.h"
#include "widget_animator.h"
#include "curtain_item.h"
#include "curtain_animator.h"

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

QnDisplaySynchronizer::QnDisplaySynchronizer(QnDisplayState *state, QGraphicsScene *scene, QGraphicsView *view, QObject *parent):
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
    connect(m_state,          SIGNAL(selectedEntityChanged(QnUiLayoutItem *, QnUiLayoutItem *)),    this, SLOT(at_state_selectedEntityChanged(QnUiLayoutItem *, QnUiLayoutItem *)));
    connect(m_state,          SIGNAL(zoomedEntityChanged(QnUiLayoutItem *, QnUiLayoutItem *)),      this, SLOT(at_state_zoomedEntityChanged(QnUiLayoutItem *, QnUiLayoutItem *)));
    connect(m_state->model(), SIGNAL(itemAdded(QnUiLayoutItem *)),                                  this, SLOT(at_model_itemAdded(QnUiLayoutItem *)));
    connect(m_state->model(), SIGNAL(itemAboutToBeRemoved(QnUiLayoutItem *)),                       this, SLOT(at_model_itemAboutToBeRemoved(QnUiLayoutItem *)));
    foreach(QnUiLayoutItem *entity, m_state->model()->items())
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

QnDisplaySynchronizer::~QnDisplaySynchronizer() {
    foreach(QnUiLayoutItem *entity, m_state->model()->items())
        at_model_itemAboutToBeRemoved(entity);
}


// -------------------------------------------------------------------------- //
// QnDisplaySynchronizer :: mutators
// -------------------------------------------------------------------------- //
void QnDisplaySynchronizer::disableViewportChanges() {
    m_boundingInstrument->dontEnforcePosition(m_view);
    m_boundingInstrument->dontEnforceSize(m_view);
}

void QnDisplaySynchronizer::enableViewportChanges() {
    m_boundingInstrument->enforcePosition(m_view);
    m_boundingInstrument->enforceSize(m_view);
}

void QnDisplaySynchronizer::fitInView() {
    if(m_state->zoomedEntity() != NULL) {
        m_viewportAnimator->moveTo(zoomedEntityGeometry(), zoomAnimationDurationMsec);
    } else {
        m_viewportAnimator->moveTo(boundingGeometry(), zoomAnimationDurationMsec);
    }
}

void QnDisplaySynchronizer::bringToFront(const QList<QGraphicsItem *> &items) {
    foreach(QGraphicsItem *item, items)
        bringToFront(item);
}

void QnDisplaySynchronizer::bringToFront(QGraphicsItem *item) {
    if(item == NULL) {
        qnNullWarning(item);
        return;
    }

    m_frontZ += zStep;
    item->setZValue(layerFrontZ(layer(item)));
}

void QnDisplaySynchronizer::bringToFront(QnUiLayoutItem *entity) {
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
// QnDisplaySynchronizer :: calculators
// -------------------------------------------------------------------------- //
qreal QnDisplaySynchronizer::layerFrontZ(Layer layer) const {
    return m_frontZ + layer * layerZSize;
}

QnDisplaySynchronizer::Layer QnDisplaySynchronizer::entityLayer(QnUiLayoutItem *entity) const {
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

QRectF QnDisplaySynchronizer::entityGeometry(QnUiLayoutItem *entity) const {
    QRectF geometry = m_state->gridMapper()->mapFromGrid(entity->geometry());

    //QnDisplayWidget *widget = m_widgetByEntity[entity];


    QSizeF step = m_state->gridMapper()->step();
    QRectF delta = entity->geometryDelta();
    geometry = QRectF(
        geometry.left()   + delta.left()   * step.width(),
        geometry.top()    + delta.top()    * step.height(),
        geometry.width()  + delta.width()  * step.width(),
        geometry.height() + delta.height() * step.height()
    );

    return geometry;
}

QRectF QnDisplaySynchronizer::boundingGeometry() const {
    return m_state->gridMapper()->mapFromGrid(m_state->model()->boundingRect().adjusted(-1, -1, 1, 1));
}

QRectF QnDisplaySynchronizer::zoomedEntityGeometry() const {
    if(m_state->zoomedEntity() == NULL)
        return QRectF();

    return entityGeometry(m_state->zoomedEntity());
}

QRectF QnDisplaySynchronizer::viewportGeometry() const {
    return mapRectToScene(m_view, m_view->viewport()->rect());
}


// -------------------------------------------------------------------------- //
// QnDisplaySynchronizer :: synchronizers
// -------------------------------------------------------------------------- //
void QnDisplaySynchronizer::synchronize(QnUiLayoutItem *entity, bool animate) {
    synchronize(m_widgetByEntity[entity], animate);
}

void QnDisplaySynchronizer::synchronize(QnDisplayWidget *widget, bool animate) {
    synchronizeGeometry(widget, animate);
    synchronizeLayer(widget);
}

void QnDisplaySynchronizer::synchronizeGeometry(QnUiLayoutItem *entity, bool animate) {
    synchronizeGeometry(m_widgetByEntity[entity], animate);
}

void QnDisplaySynchronizer::synchronizeGeometry(QnDisplayWidget *widget, bool animate) {
    QnUiLayoutItem *entity = widget->item();

    QRectF geometry = entityGeometry(entity);

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

void QnDisplaySynchronizer::synchronizeLayer(QnUiLayoutItem *entity) {
    synchronizeLayer(m_widgetByEntity[entity]);
}

void QnDisplaySynchronizer::synchronizeLayer(QnDisplayWidget *widget) {
    setLayer(widget, entityLayer(widget->item()));
}

void QnDisplaySynchronizer::synchronizeSceneBounds() {
    QRectF boundingRect = m_state->zoomedEntity() != NULL ? zoomedEntityGeometry() : boundingGeometry();

    m_boundingInstrument->setPositionBounds(m_view, boundingRect, 0.0);
    m_boundingInstrument->setSizeBounds(m_view, viewportLowerSizeBound, Qt::KeepAspectRatioByExpanding, boundingRect.size(), Qt::KeepAspectRatioByExpanding);
}


// -------------------------------------------------------------------------- //
// QnDisplaySynchronizer :: item properties
// -------------------------------------------------------------------------- //
QnDisplaySynchronizer::Layer QnDisplaySynchronizer::layer(QGraphicsItem *item) {
    ItemProperties &properties = m_propertiesByItem[item];

    return properties.layer;
}

void QnDisplaySynchronizer::setLayer(QGraphicsItem *item, Layer layer) {
    ItemProperties &properties = m_propertiesByItem[item];

    properties.layer = layer;
    item->setZValue(layer * layerZSize + fmod(item->zValue(), layerZSize));
}

QnWidgetAnimator *QnDisplaySynchronizer::animator(QnDisplayWidget *widget) {
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
// QnDisplaySynchronizer :: handlers
// -------------------------------------------------------------------------- //
void QnDisplaySynchronizer::tick(int /*currentTime*/) {
    m_view->viewport()->update();
}

void QnDisplaySynchronizer::at_model_itemAdded(QnUiLayoutItem *entity) {
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

void QnDisplaySynchronizer::at_model_itemAboutToBeRemoved(QnUiLayoutItem *entity) {
    disconnect(entity, NULL, this, NULL);

    QnDisplayWidget *widget = m_widgetByEntity[entity];
    m_widgetByEntity.remove(entity);

    delete widget;

    synchronizeSceneBounds();
}

void QnDisplaySynchronizer::at_state_modeChanged() {
    if(m_state->mode() == QnDisplayState::EDITING) {
        m_boundingInstrument->recursiveDisable();
    } else {
        m_boundingInstrument->recursiveEnable();
    }
}

void QnDisplaySynchronizer::at_state_selectedEntityChanged(QnUiLayoutItem *oldSelectedEntity, QnUiLayoutItem *newSelectedEntity) {
    if(oldSelectedEntity != NULL)
        synchronize(oldSelectedEntity, true);

    if(newSelectedEntity != NULL) {
        bringToFront(newSelectedEntity);
        synchronize(newSelectedEntity, true);
    }
}

void QnDisplaySynchronizer::at_state_zoomedEntityChanged(QnUiLayoutItem *oldZoomedEntity, QnUiLayoutItem *newZoomedEntity) {
    if(oldZoomedEntity != NULL) {
        synchronize(oldZoomedEntity, true);
        
        m_activityListenerInstrument->recursiveDisable();
    }

    if(newZoomedEntity != NULL) {
        bringToFront(newZoomedEntity);
        synchronize(newZoomedEntity, true);

        m_activityListenerInstrument->recursiveEnable();

        m_viewportAnimator->moveTo(zoomedEntityGeometry(), zoomAnimationDurationMsec);
    } else {
        m_viewportAnimator->moveTo(boundingGeometry(), zoomAnimationDurationMsec);
    }

    synchronizeSceneBounds();
}

void QnDisplaySynchronizer::at_viewport_transformationChanged() {
    if(m_state->selectedEntity() != NULL) {
        QnDisplayWidget *widget = m_widgetByEntity[m_state->selectedEntity()];
        synchronizeGeometry(widget, animator(widget)->isAnimating());
    }
}

void QnDisplaySynchronizer::at_item_geometryChanged() {
    synchronizeGeometry(static_cast<QnUiLayoutItem *>(sender()), true);
    synchronizeSceneBounds();
}

void QnDisplaySynchronizer::at_item_geometryDeltaChanged() {
    synchronizeGeometry(static_cast<QnUiLayoutItem *>(sender()), true);
}

void QnDisplaySynchronizer::at_item_rotationChanged() {
    synchronizeGeometry(static_cast<QnUiLayoutItem *>(sender()), true);
}

void QnDisplaySynchronizer::at_item_flagsChanged() {
    synchronizeLayer(static_cast<QnUiLayoutItem *>(sender()));
}

void QnDisplaySynchronizer::at_activityStopped() {
    m_curtainAnimator->curtain(m_widgetByEntity[m_state->zoomedEntity()]);
}

void QnDisplaySynchronizer::at_activityStarted() {
    m_curtainAnimator->uncurtain();
}

void QnDisplaySynchronizer::at_curtained() {
    foreach(QnDisplayWidget *widget, m_widgetByEntity)
        if(widget->item() != m_state->zoomedEntity())
            widget->hide();

    m_view->viewport()->setCursor(QCursor(Qt::BlankCursor));
}

void QnDisplaySynchronizer::at_uncurtained() {
    foreach(QnDisplayWidget *widget, m_widgetByEntity)
        widget->show();

    m_view->viewport()->setCursor(QCursor(Qt::ArrowCursor));
}
