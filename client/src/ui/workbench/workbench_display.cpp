#include "workbench_display.h"
#include <cassert>
#include <cmath> /* For fmod. */

#include <functional> /* For std::binary_function. */

#include <camera/resource_display.h>
#include <camera/camera.h>

#include <ui/animation/viewport_animator.h>
#include <ui/animation/widget_animator.h>
#include <ui/animation/curtain_animator.h>

#include <ui/graphics/instruments/instrumentmanager.h>
#include <ui/graphics/instruments/boundinginstrument.h>
#include <ui/graphics/instruments/transformlistenerinstrument.h>
#include <ui/graphics/instruments/activitylistenerinstrument.h>

#include <ui/graphics/items/resource_widget.h>
#include <ui/graphics/items/curtain_item.h>

#include <utils/common/warnings.h>

#include "workbench_layout.h"
#include "workbench_item.h"
#include "workbench_grid_mapper.h"
#include "workbench.h"

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

QnWorkbenchDisplay::QnWorkbenchDisplay(QnWorkbench *workbench, QObject *parent):
    QObject(parent),
    m_instrumentManager(new InstrumentManager(this)),
    m_workbench(NULL),
    m_scene(NULL),
    m_view(NULL),
    m_viewportAnimator(NULL),
    m_curtainAnimator(NULL),
    m_zoomedItem(NULL),
    m_raisedItem(NULL),
    m_mode(QnWorkbench::VIEWING),
    m_frontZ(0.0),
    m_dummyScene(new QGraphicsScene(this))
{
    /* Create and configure instruments. */
    m_boundingInstrument = new BoundingInstrument(this);
    m_transformListenerInstrument = new TransformListenerInstrument(this);
    m_activityListenerInstrument = new ActivityListenerInstrument(1000, this);
    m_instrumentManager->installInstrument(m_transformListenerInstrument);
    m_instrumentManager->installInstrument(m_boundingInstrument);
    m_instrumentManager->installInstrument(m_activityListenerInstrument);

    m_activityListenerInstrument->recursiveDisable();

    connect(m_transformListenerInstrument, SIGNAL(transformChanged(QGraphicsView *)),                   this,                   SLOT(at_viewport_transformationChanged()));
    connect(m_activityListenerInstrument,  SIGNAL(activityStopped()),                                   this,                   SLOT(at_activityStopped()));
    connect(m_activityListenerInstrument,  SIGNAL(activityResumed()),                                   this,                   SLOT(at_activityStarted()));

    /* Configure viewport updates. */
    m_updateTimer = new AnimationTimer(this);
    m_updateTimer->setListener(this);

    /* Create curtain animator. */
    m_curtainAnimator = new QnCurtainAnimator(1000, this);
    connect(m_curtainAnimator,  SIGNAL(curtained()),                                                    this,                   SLOT(at_curtained()));
    connect(m_curtainAnimator,  SIGNAL(uncurtained()),                                                  this,                   SLOT(at_uncurtained()));

    /* Create viewport animator. */
    m_viewportAnimator = new QnViewportAnimator(this);
    m_viewportAnimator->setMovementSpeed(4.0);
    m_viewportAnimator->setScalingSpeed(32.0);
    connect(m_viewportAnimator, SIGNAL(animationStarted()),                                             this,                   SIGNAL(viewportGrabbed()));
    connect(m_viewportAnimator, SIGNAL(animationStarted()),                                             m_boundingInstrument,   SLOT(recursiveDisable()));
    connect(m_viewportAnimator, SIGNAL(animationFinished()),                                            this,                   SIGNAL(viewportUngrabbed()));
    connect(m_viewportAnimator, SIGNAL(animationFinished()),                                            m_boundingInstrument,   SLOT(recursiveEnable()));
    connect(m_viewportAnimator, SIGNAL(animationFinished()),                                            this,                   SLOT(at_viewport_animationFinished()));

    /* Set up defaults. */
    initWorkbench(workbench);
    setScene(m_dummyScene);
}

QnWorkbenchDisplay::~QnWorkbenchDisplay() {
    m_dummyScene = NULL;

    initWorkbench(NULL);
    setScene(NULL);
}

void QnWorkbenchDisplay::initWorkbench(QnWorkbench *workbench) {
    if(m_workbench == workbench)
        return;

    if(m_workbench != NULL && m_scene != NULL)
        deinitSceneWorkbench();

    m_workbench = workbench;

    if(m_workbench != NULL && m_scene != NULL)
        initSceneWorkbench();
}

void QnWorkbenchDisplay::setScene(QGraphicsScene *scene) {
    if(m_scene == scene)
        return;

    if(m_scene != NULL && m_workbench != NULL)
        deinitSceneWorkbench();
    
    /* Prepare new scene. */ 
    m_scene = scene;
    if(m_scene == NULL && m_dummyScene != NULL) {
        m_dummyScene->clear();
        m_scene = m_dummyScene;
    }

    /* Set up new scene. 
     * It may be NULL only when this function is called from destructor. */
    if(m_scene != NULL && m_workbench != NULL)
        initSceneWorkbench();
}

void QnWorkbenchDisplay::deinitSceneWorkbench() {
    assert(m_scene != NULL && m_workbench != NULL);

    /* Deinit scene. */
    m_instrumentManager->unregisterScene(m_scene);

    disconnect(m_scene, NULL, this, NULL);
    m_propertiesByItem.clear();

    /* Clear curtain. */
    if(!m_curtainItem.isNull()) {
        delete m_curtainItem.data();
        m_curtainItem.clear();
    }
    m_curtainAnimator->setCurtainItem(NULL);


    /* Deinit workbench. */
    disconnect(m_workbench, NULL, this, NULL);

    foreach(QnWorkbenchItem *item, m_workbench->layout()->items())
        removeItemInternal(item);

    m_raisedItem = m_zoomedItem = NULL;

    assert(m_raisedItem == NULL);
    assert(m_zoomedItem == NULL);
    assert(m_mode == QnWorkbench::VIEWING);
}

void QnWorkbenchDisplay::initSceneWorkbench() {
    assert(m_scene != NULL && m_workbench != NULL);

    /* Init scene. */
    m_instrumentManager->registerScene(m_scene);
    if(m_view != NULL) {
        m_view->setScene(m_scene);
        m_instrumentManager->registerView(m_view);

        initBoundingInstrument();
    }

    connect(m_scene, SIGNAL(destroyed()), this, SLOT(at_scene_destroyed()));

    /* Scene indexing will only slow everything down. */ 
    m_scene->setItemIndexMethod(QGraphicsScene::NoIndex);

    /* Set up curtain. */
    m_curtainItem = new QnCurtainItem();
    m_scene->addItem(m_curtainItem.data());
    setLayer(m_curtainItem.data(), CURTAIN_LAYER);
    m_curtainAnimator->setCurtainItem(m_curtainItem.data());

    /* Init workbench. */
    connect(m_workbench,            SIGNAL(aboutToBeDestroyed()),                   this,                   SLOT(at_workbench_aboutToBeDestroyed()));
    connect(m_workbench,            SIGNAL(modeChanged()),                          this,                   SLOT(at_workbench_modeChanged()));
    connect(m_workbench,            SIGNAL(raisedItemChanged()),                  this,                   SLOT(at_workbench_raisedItemChanged()));
    connect(m_workbench,            SIGNAL(zoomedItemChanged()),                    this,                   SLOT(at_workbench_zoomedItemChanged()));
    connect(m_workbench,            SIGNAL(itemAdded(QnWorkbenchItem *)),           this,                   SLOT(at_workbench_itemAdded(QnWorkbenchItem *)));
    connect(m_workbench,            SIGNAL(itemAboutToBeRemoved(QnWorkbenchItem *)),this,                   SLOT(at_workbench_itemAboutToBeRemoved(QnWorkbenchItem *)));

    /* Create items. */
    foreach(QnWorkbenchItem *item, m_workbench->layout()->items())
        addItemInternal(item);

    /* Fire signals if needed. */
    if(m_workbench->mode() != m_mode)
        at_workbench_modeChanged();
    at_workbench_raisedItemChanged();
    at_workbench_zoomedItemChanged();

    synchronizeSceneBounds();
}

void QnWorkbenchDisplay::setView(QGraphicsView *view) {
    if(m_view == view)
        return;

    if(m_view != NULL) {
        m_instrumentManager->unregisterView(m_view);
        
        disconnect(m_view, NULL, this, NULL);

        m_viewportAnimator->setView(NULL);

        /* Stop viewport updates. */
        m_updateTimer->stop();
    }

    m_view = view;

    if(m_view != NULL) {
        m_view->setScene(m_scene);

        m_instrumentManager->registerView(m_view);

        connect(m_view, SIGNAL(destroyed()), this, SLOT(at_view_destroyed()));

        /* Configure OpenGL */
        if (!QGLFormat::hasOpenGL()) {
            qnCritical("Software rendering is not supported.");
        } else {
            QGLFormat glFormat;
            glFormat.setOption(QGL::SampleBuffers); /* Multisampling. */
            glFormat.setSwapInterval(1); /* Turn vsync on. */
            m_view->setViewport(new QGLWidget(glFormat));
        }

        /* Turn on antialiasing at QPainter level. */
        m_view->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing); 
        m_view->setRenderHints(QPainter::SmoothPixmapTransform);

        /* In OpenGL mode this one seems to be ignored, but it will help in software mode. */
        m_view->setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);

        /* All our items save and restore painter state. */
        m_view->setOptimizationFlag(QGraphicsView::DontSavePainterState, false); /* Can be turned on if we won't be using framed widgets. */ 
        m_view->setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing);

        /* Don't even try to uncomment this one, it slows everything down. */
        //m_view->setCacheMode(QGraphicsView::CacheBackground);

        /* We don't need scrollbars & frame. */
        m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_view->setFrameShape(QFrame::NoFrame);

        /* This may be needed by other instruments. */
        m_view->setDragMode(QGraphicsView::NoDrag);
        m_view->viewport()->setAcceptDrops(true);

        /* Configure bounding instrument. */
        initBoundingInstrument();

        /* Configure viewport animator. */
        m_viewportAnimator->setView(m_view);

        /* Start viewport updates. */
        m_updateTimer->start();
    }
}

void QnWorkbenchDisplay::initBoundingInstrument() {
    assert(m_view != NULL);

    m_boundingInstrument->setSizeEnforced(m_view, true);
    m_boundingInstrument->setPositionEnforced(m_view, true);
    m_boundingInstrument->setScalingSpeed(m_view, 32.0);
    m_boundingInstrument->setMovementSpeed(m_view, 4.0);
}


// -------------------------------------------------------------------------- //
// QnWorkbenchDisplay :: item properties
// -------------------------------------------------------------------------- //
QnWorkbenchDisplay::Layer QnWorkbenchDisplay::layer(QGraphicsItem *item) const {
     QHash<QGraphicsItem *, ItemProperties>::const_iterator pos = m_propertiesByItem.find(item);
     if(pos == m_propertiesByItem.end())
         return BACK_LAYER;

    return pos->layer;
}

void QnWorkbenchDisplay::setLayer(QGraphicsItem *item, Layer layer) {
    if(item == NULL) {
        qnNullWarning(item);
        return;
    }

    ItemProperties &properties = m_propertiesByItem[item];

    /* Moving items back and forth between layers should preserve their relative
     * z order. Hence the fmod.*/
    properties.layer = layer;
    item->setZValue(layer * layerZSize + fmod(item->zValue(), layerZSize));
}

void QnWorkbenchDisplay::setLayer(const QList<QGraphicsItem *> &items, Layer layer) {
    foreach(QGraphicsItem *item, items)
        setLayer(item, layer);
}

QnWidgetAnimator *QnWorkbenchDisplay::animator(QnResourceWidget *widget) {
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

QnResourceWidget *QnWorkbenchDisplay::widget(QnWorkbenchItem *item) const {
    return m_widgetByItem[item];
}

QnResourceDisplay *QnWorkbenchDisplay::display(QnWorkbenchItem *item) const {
    QnResourceWidget *widget = this->widget(item);
    if(widget == NULL)
        return NULL;

    return widget->display();
}

CLVideoCamera *QnWorkbenchDisplay::camera(QnWorkbenchItem *item) const {
    QnResourceDisplay *display = this->display(item);
    if(display == NULL)
        return NULL;

    return display->camera();
}

CLCamDisplay *QnWorkbenchDisplay::camDisplay(QnWorkbenchItem *item) const {
    CLVideoCamera *camera = this->camera(item);
    if(camera == NULL)
        return NULL;

    return camera->getCamCamDisplay();
}


// -------------------------------------------------------------------------- //
// QnWorkbenchDisplay :: mutators
// -------------------------------------------------------------------------- //
void QnWorkbenchDisplay::fitInView() {
    if(m_zoomedItem != NULL) {
        m_viewportAnimator->moveTo(itemGeometry(m_zoomedItem), zoomAnimationDurationMsec);
    } else {
        m_viewportAnimator->moveTo(layoutBoundingGeometry(), zoomAnimationDurationMsec);
    }
}

void QnWorkbenchDisplay::bringToFront(const QList<QGraphicsItem *> &items) {
    QList<QGraphicsItem *> localItems = items;

    struct GraphicsItemZLess: public std::binary_function<QGraphicsItem *, QGraphicsItem *, bool> {
        bool operator()(QGraphicsItem *l, QGraphicsItem *r) const {
            return l->zValue() < r->zValue();
        }
    };

    /* Sort by z order first, so that relative z order is preserved. */
    qSort(localItems.begin(), localItems.end(), GraphicsItemZLess());

    foreach(QGraphicsItem *item, localItems)
        bringToFront(item);
}

void QnWorkbenchDisplay::bringToFront(QGraphicsItem *item) {
    if(item == NULL) {
        qnNullWarning(item);
        return;
    }

    m_frontZ += zStep;
    item->setZValue(layerFrontZValue(layer(item)));
}

void QnWorkbenchDisplay::bringToFront(QnWorkbenchItem *item) {
    if(item == NULL) {
        qnNullWarning(item);
        return;
    }

    bringToFront(widget(item));
}

void QnWorkbenchDisplay::addItemInternal(QnWorkbenchItem *item) {
    QnResourceWidget *widget = new QnResourceWidget(item);
    widget->setParent(this); /* Just to feel totally safe and not to leak memory no matter what happens. */

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

    connect(item, SIGNAL(geometryChanged()),        this, SLOT(at_item_geometryChanged()));
    connect(item, SIGNAL(geometryDeltaChanged()),   this, SLOT(at_item_geometryDeltaChanged()));
    connect(item, SIGNAL(rotationChanged()),        this, SLOT(at_item_rotationChanged()));
    connect(item, SIGNAL(flagsChanged()),           this, SLOT(at_item_flagsChanged()));

    m_widgetByItem.insert(item, widget);

    synchronize(widget, false);
    bringToFront(widget);

    connect(widget, SIGNAL(destroyed()), this, SLOT(at_widget_destroyed()));
}

void QnWorkbenchDisplay::removeItemInternal(QnWorkbenchItem *item) {
    disconnect(item, NULL, this, NULL);

    QnResourceWidget *widget = m_widgetByItem[item];
    if(widget == NULL)
        return;

    m_widgetByItem.remove(item);

    disconnect(widget, NULL, this, NULL);

    delete widget;
}


// -------------------------------------------------------------------------- //
// QnWorkbenchDisplay :: calculators
// -------------------------------------------------------------------------- //
qreal QnWorkbenchDisplay::layerFrontZValue(Layer layer) const {
    return layerZValue(layer) + m_frontZ;
}

qreal QnWorkbenchDisplay::layerZValue(Layer layer) const {
    return layer * layerZSize;
}

QnWorkbenchDisplay::Layer QnWorkbenchDisplay::synchronizedLayer(QnWorkbenchItem *item) const {
    assert(item != NULL);
    
    if(item == m_zoomedItem) {
        return ZOOMED_LAYER;
    } else if(item->isPinned()) {
        if(item == m_raisedItem) {
            return PINNED_RAISED_LAYER;
        } else {
            return PINNED_LAYER;
        }
    } else {
        if(item == m_raisedItem) {
            return UNPINNED_RAISED_LAYER;
        } else {
            return UNPINNED_LAYER;
        }
    }
}

QRectF QnWorkbenchDisplay::itemEnclosingGeometry(QnWorkbenchItem *item) const {
    if(item == NULL) {
        qnNullWarning(item);
        return QRectF();
    }

    QRectF result = m_workbench->mapper()->mapFromGrid(item->geometry());

    QSizeF step = m_workbench->mapper()->step();
    QRectF delta = item->geometryDelta();
    result = QRectF(
        result.left()   + delta.left()   * step.width(),
        result.top()    + delta.top()    * step.height(),
        result.width()  + delta.width()  * step.width(),
        result.height() + delta.height() * step.height()
    );

    return result;
}

QRectF QnWorkbenchDisplay::itemGeometry(QnWorkbenchItem *item, QRectF *enclosingGeometry) const {
    if(item == NULL) {
        qnNullWarning(item);
        return QRectF();
    }

    QRectF result = itemEnclosingGeometry(item);
    if(enclosingGeometry != NULL)
        *enclosingGeometry = result;

    QnResourceWidget *widget = this->widget(item);
    if(!widget->hasAspectRatio())
        return result;

    return expanded(widget->aspectRatio(), result, Qt::KeepAspectRatio);
}

QRectF QnWorkbenchDisplay::layoutBoundingGeometry() const {
    return m_workbench->mapper()->mapFromGrid(m_workbench->layout()->boundingRect().adjusted(-1, -1, 1, 1));
}

QRectF QnWorkbenchDisplay::viewportGeometry() const {
    if(m_view == NULL) {
        return QRectF();
    } else {
        return mapRectToScene(m_view, m_view->viewport()->rect());
    }
}


// -------------------------------------------------------------------------- //
// QnWorkbenchDisplay :: synchronizers
// -------------------------------------------------------------------------- //
void QnWorkbenchDisplay::synchronize(QnWorkbenchItem *item, bool animate) {
    if(item == NULL) {
        qnNullWarning(item);
        return;
    }

    synchronize(widget(item), animate);
}

void QnWorkbenchDisplay::synchronize(QnResourceWidget *widget, bool animate) {
    if(widget == NULL) {
        qnNullWarning(widget);
        return;
    }

    synchronizeGeometry(widget, animate);
    synchronizeLayer(widget);
}

void QnWorkbenchDisplay::synchronizeGeometry(QnWorkbenchItem *item, bool animate) {
    synchronizeGeometry(widget(item), animate);
}

void QnWorkbenchDisplay::synchronizeGeometry(QnResourceWidget *widget, bool animate) {
    assert(widget != NULL);
    assert(m_workbench != NULL);

    QnWorkbenchItem *item = widget->item();

    QRectF enclosingGeometry = itemEnclosingGeometry(item);

    /* Adjust for raise. */
    if(item == m_raisedItem && item != m_zoomedItem && m_view != NULL) {
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
    if(item == m_raisedItem || item == m_zoomedItem)
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

void QnWorkbenchDisplay::synchronizeLayer(QnWorkbenchItem *item) {
    synchronizeLayer(widget(item));
}

void QnWorkbenchDisplay::synchronizeLayer(QnResourceWidget *widget) {
    setLayer(widget, synchronizedLayer(widget->item()));
}

void QnWorkbenchDisplay::synchronizeSceneBounds() {
    QRectF rect = m_zoomedItem != NULL ? itemGeometry(m_zoomedItem) : layoutBoundingGeometry();
    m_boundingInstrument->setPositionBounds(m_view, rect, 0.0);
    m_boundingInstrument->setSizeBounds(m_view, viewportLowerSizeBound, Qt::KeepAspectRatioByExpanding, rect.size(), Qt::KeepAspectRatioByExpanding);
}


// -------------------------------------------------------------------------- //
// QnWorkbenchDisplay :: handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchDisplay::tick(int /*currentTime*/) {
    assert(m_view != NULL);

    m_view->viewport()->update();
}

void QnWorkbenchDisplay::at_viewport_animationFinished() {
    synchronizeSceneBounds();
}

void QnWorkbenchDisplay::at_workbench_itemAdded(QnWorkbenchItem *item) {
    addItemInternal(item);
    synchronizeSceneBounds();
}

void QnWorkbenchDisplay::at_workbench_itemAboutToBeRemoved(QnWorkbenchItem *item) {
    removeItemInternal(item);
    synchronizeSceneBounds();
}

void QnWorkbenchDisplay::at_workbench_aboutToBeDestroyed() {
    initWorkbench(new QnWorkbench(this));
}

void QnWorkbenchDisplay::at_workbench_modeChanged() {
    if(m_mode == m_workbench->mode()) {
        qnWarning("Mode change signal was received even though the mode didn't change.");
        return;
    }

    m_mode = m_workbench->mode();

    if(m_workbench->mode() == QnWorkbench::EDITING) {
        m_boundingInstrument->recursiveDisable();
    } else {
        m_boundingInstrument->recursiveEnable();
    }
}

void QnWorkbenchDisplay::at_workbench_raisedItemChanged() {
    QnWorkbenchItem *oldRaisedItem = m_raisedItem;
    m_raisedItem = m_workbench->raisedItem();

    if(oldRaisedItem != NULL)
        synchronize(oldRaisedItem);
    if(m_raisedItem != NULL) {
        bringToFront(m_raisedItem);
        synchronize(m_raisedItem, true);
    }
}

void QnWorkbenchDisplay::at_workbench_zoomedItemChanged() {
    QnWorkbenchItem *oldZoomedItem = m_zoomedItem;
    m_zoomedItem = m_workbench->zoomedItem();

    if(oldZoomedItem != NULL) {
        synchronize(oldZoomedItem, true);
        
        m_activityListenerInstrument->recursiveDisable();
    }

    if(m_zoomedItem != NULL) {
        bringToFront(m_zoomedItem);
        synchronize(m_zoomedItem, true);

        m_activityListenerInstrument->recursiveEnable();

        m_viewportAnimator->moveTo(itemGeometry(m_zoomedItem), zoomAnimationDurationMsec);
    } else {
        m_viewportAnimator->moveTo(layoutBoundingGeometry(), zoomAnimationDurationMsec);
    }

    synchronizeSceneBounds();
}


void QnWorkbenchDisplay::at_viewport_transformationChanged() {
    if(m_raisedItem == NULL)
        return;

    QnResourceWidget *widget = this->widget(m_raisedItem);
    synchronizeGeometry(widget, animator(widget)->isAnimating());
}

void QnWorkbenchDisplay::at_item_geometryChanged() {
    synchronizeGeometry(static_cast<QnWorkbenchItem *>(sender()), true);
    synchronizeSceneBounds();
}

void QnWorkbenchDisplay::at_item_geometryDeltaChanged() {
    synchronizeGeometry(static_cast<QnWorkbenchItem *>(sender()), true);
}

void QnWorkbenchDisplay::at_item_rotationChanged() {
    synchronizeGeometry(static_cast<QnWorkbenchItem *>(sender()), true);
}

void QnWorkbenchDisplay::at_item_flagsChanged() {
    synchronizeLayer(static_cast<QnWorkbenchItem *>(sender()));
}

void QnWorkbenchDisplay::at_activityStopped() {
    m_curtainAnimator->curtain(widget(m_zoomedItem));
}

void QnWorkbenchDisplay::at_activityStarted() {
    m_curtainAnimator->uncurtain();
}

void QnWorkbenchDisplay::at_curtained() {
    foreach(QnResourceWidget *widget, m_widgetByItem)
        if(widget->item() != m_zoomedItem)
            widget->hide();

    if(m_view != NULL)
        m_view->viewport()->setCursor(QCursor(Qt::BlankCursor));
}

void QnWorkbenchDisplay::at_uncurtained() {
    foreach(QnResourceWidget *widget, m_widgetByItem)
        widget->show();

    if(m_view != NULL)
        m_view->viewport()->setCursor(QCursor(Qt::ArrowCursor));
}

void QnWorkbenchDisplay::at_widget_destroyed() {
    /* Holy crap! Somebody's destroying our widgets. Disconnect from the scene. */
    m_widgetByItem.clear();
    setScene(NULL);
}

void QnWorkbenchDisplay::at_scene_destroyed() {
    m_widgetByItem.clear();
    setScene(NULL);
}

void QnWorkbenchDisplay::at_view_destroyed() {
    setView(NULL);
}

