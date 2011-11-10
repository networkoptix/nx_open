#include "display_synchronizer.h"
#include <cassert>
#include <ui/model/display_entity.h>
#include <ui/model/display_model.h>
#include <ui/view/viewport_animator.h>
#include <ui/instruments/instrumentmanager.h>
#include <ui/instruments/boundinginstrument.h>
#include <ui/instruments/transformlistenerinstrument.h>
#include <utils/common/warnings.h>
#include "display_state.h"
#include "display_widget.h"
#include "display_grid_mapper.h"
#include "widget_animator.h"

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

    const qreal initialFrontZ = 0.0;

} // anonymous namespace

QnDisplaySynchronizer::QnDisplaySynchronizer(QnDisplayState *state, QGraphicsScene *scene, QGraphicsView *view, QObject *parent):
    QObject(parent),
    m_state(state),
    m_manager(new InstrumentManager(this)),
    m_scene(scene),
    m_view(view),
    m_viewportAnimator(new QnViewportAnimator(view, this)),
    m_frontZ(initialFrontZ)
{
    assert(state != NULL && scene != NULL && view != NULL);
    
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
    m_manager->installInstrument(m_transformListenerInstrument);
    m_manager->installInstrument(m_boundingInstrument);

    m_boundingInstrument->setSizeEnforced(view, true);
    m_boundingInstrument->setPositionEnforced(view, true);
    m_boundingInstrument->setScalingSpeed(view, 32.0);
    m_boundingInstrument->setMovementSpeed(view, 4.0);

    connect(m_transformListenerInstrument, SIGNAL(transformChanged(QGraphicsView *)), this, SLOT(at_viewport_transformationChanged()));

    /* Configure animator. */
    m_viewportAnimator->setMovementSpeed(4.0);
    m_viewportAnimator->setScalingSpeed(32.0);
    connect(m_viewportAnimator, SIGNAL(animationStarted()),  this,                 SIGNAL(viewportGrabbed()));
    connect(m_viewportAnimator, SIGNAL(animationStarted()),  m_boundingInstrument, SLOT(recursiveDisable()));
    connect(m_viewportAnimator, SIGNAL(animationFinished()), this,                 SIGNAL(viewportUngrabbed()));
    connect(m_viewportAnimator, SIGNAL(animationFinished()), m_boundingInstrument, SLOT(recursiveEnable()));
    connect(m_viewportAnimator, SIGNAL(animationFinished()), this,                 SLOT(updateSceneBounds()));

    /* Subscribe to state changes and create items. */
    connect(m_state,          SIGNAL(modeChanged()),                                                this, SLOT(at_state_modeChanged()));
    connect(m_state,          SIGNAL(selectedEntityChanged(QnDisplayEntity *, QnDisplayEntity *)),  this, SLOT(at_state_selectedEntityChanged(QnDisplayEntity *, QnDisplayEntity *)));
    connect(m_state,          SIGNAL(zoomedEntityChanged(QnDisplayEntity *, QnDisplayEntity *)),    this, SLOT(at_state_zoomedEntityChanged(QnDisplayEntity *, QnDisplayEntity *)));
    connect(m_state->model(), SIGNAL(entityAdded(QnDisplayEntity *)),                               this, SLOT(at_model_entityAdded(QnDisplayEntity *)));
    connect(m_state->model(), SIGNAL(entityAboutToBeRemoved(QnDisplayEntity *)),                    this, SLOT(at_model_entityAboutToBeRemoved(QnDisplayEntity *)));
    foreach(QnDisplayEntity *entity, m_state->model()->entities())
        at_model_entityAdded(entity);

    /* Configure viewport updates. */
    m_updateTimer = new AnimationTimer(this);
    m_updateTimer->setListener(this);
    m_updateTimer->start();

    /* Create items. */
#if 0
    for (int i = 0; i < 16; ++i) {
        QnDisplayWidget *widget = new QnDisplayWidget(new QnDefaultDeviceVideoLayout(), d->centralWidget);

        widget->setFlag(QGraphicsItem::ItemIgnoresParentOpacity, true); /* Optimization. */
        widget->setFlag(QGraphicsItem::ItemIsSelectable, true);
        widget->setFlag(QGraphicsItem::ItemIsFocusable, true);

        widget->setExtraFlag(GraphicsWidget::ItemIsResizable);
        widget->setInteractive(true);

        widget->setAnimationsEnabled(true);
        widget->setWindowTitle(QnSceneController::tr("item # %1").arg(i + 1));
        widget->setPos(QPointF(-500, -500));
        widget->setZValue(normalZ);

        QString uid = "e:\\Video\\!Fun\\No time for nuts.avi";

        QnResourcePtr device = qnResPool->getResourceByUniqId(uid);
        if (!device)
        {
            device = QnResourceDirectoryBrowser::instance().checkFile(uid);

            if (device)
                qnResPool->addResource(device);
        }

        CLCamDisplay *camDisplay = new CLCamDisplay(false);

        QnAbstractStreamDataProvider* reader = device->createDataProvider(QnResource::Role_Default);
        QnAbstractMediaStreamDataProvider* mediaReader = dynamic_cast<QnAbstractMediaStreamDataProvider*>(reader);

        int videonum = mediaReader->getVideoLayout()->numberOfChannels();

        for (int i = 0; i < videonum; ++i)
        {
            camDisplay->addVideoChannel(i, widget->renderer(), true);
        }

        mediaReader->addDataProcessor(camDisplay);

        mediaReader->start();
        camDisplay->start();

        /*QGraphicsGridLayout *layout = new QGraphicsGridLayout(widget);
        layout->setSpacing(0);
        layout->setContentsMargins(0, 0, 0, 0);

        GraphicsLayoutItem *item = new GraphicsLayoutItem(widget);
        layout->addItem(item, 0, 0, 1, 1, Qt::AlignCenter);
        item->setAcceptedMouseButtons(0);*/

        connect(widget, SIGNAL(resizingStarted()),  this, SLOT(at_widget_resizingStarted()));
        connect(widget, SIGNAL(resizingFinished()), this, SLOT(at_widget_resizingFinished()));

        connect(widget, SIGNAL(resizingStarted()),  d->m_dragInstrument, SLOT(stopCurrentOperation()));

        d->animatedWidgets.push_back(widget);
    }
#endif
}

QnDisplaySynchronizer::~QnDisplaySynchronizer() {
    foreach(QnDisplayEntity *entity, m_state->model()->entities())
        at_model_entityAboutToBeRemoved(entity);
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

void QnDisplaySynchronizer::synchronize(QnDisplayEntity *entity) {
    updateWidgetGeometry(entity, true);
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

    if(qFuzzyCompare(item->zValue(), m_frontZ))
        return;

    m_frontZ = m_frontZ + 1.0;
    item->setZValue(m_frontZ);
}

void QnDisplaySynchronizer::bringToFront(QnDisplayEntity *entity) {
    if(entity == NULL) {
        qnNullWarning(entity);
        return;
    }

    if(entity->model() != m_state->model()) {
        qnWarning("Cannot bring to front an entity from another model.");
        return;
    }

    bringToFront(m_dataByEntity[entity].widget);
}


// -------------------------------------------------------------------------- //
// QnDisplaySynchronizer :: calculators
// -------------------------------------------------------------------------- //
QnWidgetAnimator *QnDisplaySynchronizer::newWidgetAnimator(QnDisplayWidget *widget) {
    QnWidgetAnimator *animator = new QnWidgetAnimator(widget, widget);
    animator->setMovementSpeed(4.0);
    animator->setScalingSpeed(32.0);
    animator->setRotationSpeed(720.0);
    return animator;
}

QRectF QnDisplaySynchronizer::entityGeometry(QnDisplayEntity *entity) const {
    QRectF geometry = m_state->gridMapper()->mapFromGrid(entity->geometry());

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
// QnDisplaySynchronizer :: updaters
// -------------------------------------------------------------------------- //
void QnDisplaySynchronizer::updateWidgetGeometry(QnDisplayEntity *entity, bool animate) {
    const EntityData &data = m_dataByEntity[entity];
    QnDisplayWidget *widget = data.widget;

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
    
    if(animate) {
        data.animator->moveTo(geometry, entity->rotation(), widgetAnimationDurationMsec);
    } else {
        data.animator->stopAnimation();
        widget->setGeometry(geometry);
        widget->setRotation(entity->rotation());
    }
}

void QnDisplaySynchronizer::updateSceneBounds() {
    QRectF boundingRect = m_state->zoomedEntity() != NULL ? zoomedEntityGeometry() : boundingGeometry();

    m_boundingInstrument->setPositionBounds(m_view, boundingRect, 0.0);
    m_boundingInstrument->setSizeBounds(m_view, viewportLowerSizeBound, Qt::KeepAspectRatioByExpanding, boundingRect.size(), Qt::KeepAspectRatioByExpanding);
}


// -------------------------------------------------------------------------- //
// QnDisplaySynchronizer :: handlers
// -------------------------------------------------------------------------- //
void QnDisplaySynchronizer::tick(int /*currentTime*/) {
    m_view->viewport()->update();
}

void QnDisplaySynchronizer::at_model_entityAdded(QnDisplayEntity *entity) {
    QnDisplayWidget *widget = new QnDisplayWidget(entity);
    m_scene->addItem(widget);

    widget->setFlag(QGraphicsItem::ItemIgnoresParentOpacity, true); /* Optimization. */
    widget->setFlag(QGraphicsItem::ItemIsSelectable, true);
    widget->setFlag(QGraphicsItem::ItemIsFocusable, true);

    widget->setFlag(QGraphicsItem::ItemIsMovable, true);
    widget->setWindowFlags(Qt::Window);
    widget->setExtraFlag(GraphicsWidget::ItemIsResizable);

    /* Unsetting this flag is VERY important. If it is set, graphics scene
     * will mess with widget's z value and bring it to front every time 
     * it is clicked. This will wreak havoc in our layered system.
     * 
     * Note that this flag must be unset after Qt::Window window flag is set
     * because the latter automatically sets the former. */
    widget->setFlag(QGraphicsItem::ItemIsPanel, false);

    connect(entity, SIGNAL(geometryChanged(const QRect &, const QRect &)),          this, SLOT(at_entity_geometryUpdated(const QRect &, const QRect &)));
    connect(entity, SIGNAL(geometryDeltaChanged(const QRectF &, const QRectF &)),   this, SLOT(at_entity_geometryDeltaUpdated()));
    connect(entity, SIGNAL(rotationChanged(qreal, qreal)),                          this, SLOT(at_entity_rotationUpdated()));

    m_dataByEntity.insert(entity, EntityData(widget, newWidgetAnimator(widget)));

    updateWidgetGeometry(entity, false);
    updateSceneBounds();
    bringToFront(widget);

    emit widgetAdded(widget);
}

void QnDisplaySynchronizer::at_model_entityAboutToBeRemoved(QnDisplayEntity *entity) {
    disconnect(entity, NULL, this, NULL);

    QnDisplayWidget *widget = m_dataByEntity[entity].widget;
    m_dataByEntity.remove(entity);

    delete widget;

    updateSceneBounds();
}

void QnDisplaySynchronizer::at_state_modeChanged() {
    if(m_state->mode() == QnDisplayState::EDITING) {
        m_boundingInstrument->recursiveDisable();
    } else {
        m_boundingInstrument->recursiveEnable();
    }
}

void QnDisplaySynchronizer::at_state_selectedEntityChanged(QnDisplayEntity *oldSelectedEntity, QnDisplayEntity *newSelectedEntity) {
    if(oldSelectedEntity != NULL)
        updateWidgetGeometry(oldSelectedEntity, true);

    if(newSelectedEntity != NULL)
        updateWidgetGeometry(newSelectedEntity, true);
}

void QnDisplaySynchronizer::at_state_zoomedEntityChanged(QnDisplayEntity *oldZoomedEntity, QnDisplayEntity *newZoomedEntity) {
    if(oldZoomedEntity != NULL)
        updateWidgetGeometry(oldZoomedEntity, true);

    if(newZoomedEntity != NULL) {
        updateWidgetGeometry(newZoomedEntity, true);

        m_viewportAnimator->moveTo(zoomedEntityGeometry(), zoomAnimationDurationMsec);
    } else {
        m_viewportAnimator->moveTo(boundingGeometry(), zoomAnimationDurationMsec);
    }

    updateSceneBounds();
}

void QnDisplaySynchronizer::at_viewport_transformationChanged() {
    if(m_state->selectedEntity() != NULL) {
        EntityData &data = m_dataByEntity[m_state->selectedEntity()];
        updateWidgetGeometry(m_state->selectedEntity(), data.animator->isAnimating());
    }
}

void QnDisplaySynchronizer::at_entity_geometryUpdated(const QRect &oldGeometry, const QRect &newGeometry) {
    updateWidgetGeometry(static_cast<QnDisplayEntity *>(sender()), true);
    updateSceneBounds();
}

void QnDisplaySynchronizer::at_entity_geometryDeltaUpdated() {
    updateWidgetGeometry(static_cast<QnDisplayEntity *>(sender()), true);
}

void QnDisplaySynchronizer::at_entity_rotationUpdated() {
    updateWidgetGeometry(static_cast<QnDisplayEntity *>(sender()), true);
}

