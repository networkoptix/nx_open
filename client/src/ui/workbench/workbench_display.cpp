#include "workbench_display.h"
#include <cassert>
#include <cstring> /* For std::memset. */
#include <cmath> /* For std::fmod. */

#include <functional> /* For std::binary_function. */

#include <QtAlgorithms>
#include <QGLContext>
#include <QGLWidget>
#include <QAction>

#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>
#include <utils/common/float.h>
#include <utils/common/delete_later.h>

#include <core/resource/layout_resource.h>
#include <core/resourcemanagment/resource_pool.h>
#include <camera/resource_display.h>
#include <camera/camera.h>

#include <ui/animation/viewport_animator.h>
#include <ui/animation/widget_animator.h>
#include <ui/animation/curtain_animator.h>
#include <ui/animation/opacity_animator.h>

#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/instruments/bounding_instrument.h>
#include <ui/graphics/instruments/transform_listener_instrument.h>
#include <ui/graphics/instruments/activity_listener_instrument.h>
#include <ui/graphics/instruments/forwarding_instrument.h>
#include <ui/graphics/instruments/stop_instrument.h>
#include <ui/graphics/instruments/signaling_instrument.h>
#include <ui/graphics/instruments/selection_overlay_hack_instrument.h>
#include <ui/graphics/instruments/focus_listener_instrument.h>

#include <ui/graphics/items/resource_widget.h>
#include <ui/graphics/items/resource_widget_renderer.h>
#include <ui/graphics/items/curtain_item.h>
#include <ui/graphics/items/image_button_widget.h>
#include <ui/graphics/items/grid_item.h>

#include <ui/style/skin.h>
#include <ui/style/globals.h>

#include "extensions/workbench_stream_synchronizer.h"
#include "extensions/workbench_render_watcher.h"
#include "workbench_layout.h"
#include "workbench_item.h"
#include "workbench_grid_mapper.h"
#include "workbench_utility.h"
#include "workbench_context.h"
#include "workbench_access_controller.h"
#include "workbench.h"

#include "core/dataprovider/abstract_streamdataprovider.h"
#include "plugins/resources/archive/abstract_archive_stream_reader.h"

namespace {
    struct GraphicsItemZLess: public std::binary_function<QGraphicsItem *, QGraphicsItem *, bool> {
        bool operator()(QGraphicsItem *l, QGraphicsItem *r) const {
            return l->zValue() < r->zValue();
        }
    };

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

    QRectF rotated(const QRectF &rect, qreal degrees) {
        QPointF c = rect.center();

        QTransform transform;
        transform.translate(c.x(), c.y());
        transform.rotate(degrees);
        transform.translate(-c.x(), -c.y());

        return transform.mapRect(rect);
    }

    /** Size multiplier for raised widgets. */
    const qreal focusExpansion = 100.0;

    /** Maximal expanded size of a raised widget, relative to viewport size. */
    const qreal maxExpandedSize = 0.5;

    /** Viewport lower size boundary, in scene coordinates. */
    const QSizeF viewportLowerSizeBound = QSizeF(qnGlobals->workbenchUnitSize() * 0.05, qnGlobals->workbenchUnitSize() * 0.05);

    const qreal defaultFrameWidth = qnGlobals->workbenchUnitSize() * 0.005;
    const qreal selectedFrameWidth = defaultFrameWidth * 2;

    const int widgetAnimationDurationMsec = 500;
    const int zoomAnimationDurationMsec = 500;

    /** The amount of z-space that one layer occupies. */
    const qreal layerZSize = 10000000.0;

    /** The amount that is added to maximal Z value each time a move to front
     * operation is performed. */
    const qreal zStep = 1.0;

    enum {
        ITEM_LAYER_KEY = 0x93A7FA71,    /**< Key for item layer. */
        ITEM_ANIMATOR_KEY = 0x81AFD591  /**< Key for item animator. */
    };

} // anonymous namespace

QnWorkbenchDisplay::QnWorkbenchDisplay(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_instrumentManager(new InstrumentManager(this)),
    m_scene(NULL),
    m_view(NULL),
    m_viewportAnimator(NULL),
    m_frameOpacityAnimator(NULL),
    m_curtainAnimator(NULL),
    m_frontZ(0.0),
    m_dummyScene(new QGraphicsScene(this)),
	m_streamSynchronizer(NULL),
    m_frameOpacity(1.0),
    m_frameWidthsDirty(false),
    m_zoomedMarginFlags(0),
    m_normalMarginFlags(0)
{
    std::memset(m_itemByRole, 0, sizeof(m_itemByRole));

    AnimationTimer *animationTimer = m_instrumentManager->animationTimer();

    /* Create and configure instruments. */
    Instrument::EventTypeSet paintEventTypes = Instrument::makeSet(QEvent::Paint);

    SignalingInstrument *resizeSignalingInstrument = new SignalingInstrument(Instrument::Viewport, Instrument::makeSet(QEvent::Resize), this);
    m_beforePaintInstrument = new SignalingInstrument(Instrument::Viewport, paintEventTypes, this);
    m_afterPaintInstrument = new SignalingInstrument(Instrument::Viewport, paintEventTypes, this);
    m_boundingInstrument = new BoundingInstrument(this);
    m_transformListenerInstrument = new TransformListenerInstrument(this);
    m_curtainActivityInstrument = new ActivityListenerInstrument(true, 1000, this);
    m_widgetActivityInstrument = new ActivityListenerInstrument(true, 1000 * 10, this);
    m_focusListenerInstrument = new FocusListenerInstrument(this);
    m_paintForwardingInstrument = new ForwardingInstrument(Instrument::Viewport, paintEventTypes, this);
    m_selectionOverlayHackInstrument = new SelectionOverlayHackInstrument(this);

    m_instrumentManager->installInstrument(new StopInstrument(Instrument::Viewport, paintEventTypes, this));
    m_instrumentManager->installInstrument(m_afterPaintInstrument);
    m_instrumentManager->installInstrument(m_paintForwardingInstrument);
    m_instrumentManager->installInstrument(m_beforePaintInstrument);
    m_instrumentManager->installInstrument(m_transformListenerInstrument);
    m_instrumentManager->installInstrument(m_focusListenerInstrument);
    m_instrumentManager->installInstrument(resizeSignalingInstrument);
    m_instrumentManager->installInstrument(m_boundingInstrument);
    m_instrumentManager->installInstrument(m_curtainActivityInstrument);
    m_instrumentManager->installInstrument(m_widgetActivityInstrument);
    m_instrumentManager->installInstrument(m_selectionOverlayHackInstrument);

    m_curtainActivityInstrument->recursiveDisable();

    connect(m_transformListenerInstrument,  SIGNAL(transformChanged(QGraphicsView *)),                  this,                   SLOT(synchronizeRaisedGeometry()));
    connect(resizeSignalingInstrument,      SIGNAL(activated(QWidget *, QEvent *)),                     this,                   SLOT(synchronizeRaisedGeometry()));
    connect(resizeSignalingInstrument,      SIGNAL(activated(QWidget *, QEvent *)),                     this,                   SLOT(synchronizeSceneBoundsExtension()));
    connect(resizeSignalingInstrument,      SIGNAL(activated(QWidget *, QEvent *)),                     this,                   SLOT(fitInView()));
    connect(m_beforePaintInstrument,        SIGNAL(activated(QWidget *, QEvent *)),                     this,                   SLOT(updateFrameWidths()));
    connect(m_curtainActivityInstrument,    SIGNAL(activityStopped()),                                  this,                   SLOT(at_activityStopped()));
    connect(m_curtainActivityInstrument,    SIGNAL(activityResumed()),                                  this,                   SLOT(at_activityStarted()));

    /* Create curtain animator. */
    m_curtainAnimator = new QnCurtainAnimator(this);
    m_curtainAnimator->setSpeed(1.0); /* (255, 0, 0) -> (0, 0, 0) in 1 second. */
    m_curtainAnimator->setTimer(animationTimer);
    connect(m_curtainAnimator,              SIGNAL(curtained()),                                        this,                   SLOT(at_curtained()));
    connect(m_curtainAnimator,              SIGNAL(uncurtained()),                                      this,                   SLOT(at_uncurtained()));

    /* Create viewport animator. */
    m_viewportAnimator = new ViewportAnimator(this); // ANIMATION: viewport.
    m_viewportAnimator->setAbsoluteMovementSpeed(0.0); /* Viewport movement speed in scene coordinates. */
    m_viewportAnimator->setRelativeMovementSpeed(1.0); /* Viewport movement speed in viewports per second. */
    m_viewportAnimator->setScalingSpeed(4.0); /* Viewport scaling speed, scale factor per second. */
    m_viewportAnimator->setTimeLimit(zoomAnimationDurationMsec);
    m_viewportAnimator->setTimer(animationTimer);
    connect(m_viewportAnimator,             SIGNAL(started()),                                          this,                   SIGNAL(viewportGrabbed()));
    connect(m_viewportAnimator,             SIGNAL(started()),                                          m_boundingInstrument,   SLOT(recursiveDisable()));
    connect(m_viewportAnimator,             SIGNAL(finished()),                                         this,                   SIGNAL(viewportUngrabbed()));
    connect(m_viewportAnimator,             SIGNAL(finished()),                                         m_boundingInstrument,   SLOT(recursiveEnable()));
    connect(m_viewportAnimator,             SIGNAL(finished()),                                         this,                   SLOT(at_viewportAnimator_finished()));

    /* Create frame opacity animator. */
    m_frameOpacityAnimator = new VariantAnimator(this);
    m_frameOpacityAnimator->setTimer(animationTimer);
    m_frameOpacityAnimator->setAccessor(new PropertyAccessor("widgetsFrameOpacity"));
    m_frameOpacityAnimator->setTargetObject(this);
    m_frameOpacityAnimator->setTimeLimit(500);

    /* Connect to context. */
    connect(accessController(),             SIGNAL(permissionsChanged(const QnResourcePtr &)),          this,                   SLOT(at_context_permissionsChanged(const QnResourcePtr &)));

    /* Set up defaults. */
    setScene(m_dummyScene);
}

QnWorkbenchDisplay::~QnWorkbenchDisplay() {
    m_dummyScene = NULL;

    setScene(NULL);
}

void QnWorkbenchDisplay::setStreamsSynchronized(bool synchronized, qint64 currentTime, float speed)
{
    if(synchronized == isStreamsSynchronized())
        return;

    if(!m_streamSynchronizer) {
        QnWorkbenchRenderWatcher *renderWatcher = new QnWorkbenchRenderWatcher(this, this);
        m_streamSynchronizer = new QnWorkbenchStreamSynchronizer(this, renderWatcher, this);

        connect(m_streamSynchronizer, SIGNAL(effectiveChanged()), this, SIGNAL(streamsSynchronizationEffectiveChanged()));
        if(m_streamSynchronizer->isEffective())
            emit streamsSynchronizationEffectiveChanged();
    }

    if (synchronized)
        m_streamSynchronizer->enableSync(currentTime, speed);
    else
        m_streamSynchronizer->disableSync();

    emit streamsSynchronizedChanged();
}

void QnWorkbenchDisplay::setStreamsSynchronized(QnResourceWidget* widget)
{
    bool synchronized = widget != 0;
    qint64 currentTime = AV_NOPTS_VALUE;
    float speed = 1.0;
    if (widget) {
        currentTime = widget->display()->currentTimeUSec();
        speed = widget->display()->camDisplay()->getSpeed();
    }
    setStreamsSynchronized(synchronized, currentTime, speed);
}

bool QnWorkbenchDisplay::isStreamsSynchronized() const {
    if(!m_streamSynchronizer)
        return false;

    return m_streamSynchronizer->isEnabled();
}

bool QnWorkbenchDisplay::isStreamsSynchronizationEffective() const {
    if(!m_streamSynchronizer)
        return false;

    return m_streamSynchronizer->isEffective();
}

void QnWorkbenchDisplay::setScene(QGraphicsScene *scene) {
    if(m_scene == scene)
        return;

    if(m_scene != NULL && context() != NULL)
        deinitSceneContext();

    /* Prepare new scene. */
    m_scene = scene;
    if(m_scene == NULL && m_dummyScene != NULL) {
        m_dummyScene->clear();
        m_scene = m_dummyScene;
    }

    /* Set up new scene.
     * It may be NULL only when this function is called from destructor. */
    if(m_scene != NULL && context() != NULL)
        initSceneContext();
}

void QnWorkbenchDisplay::deinitSceneContext() {
    assert(m_scene != NULL && context() != NULL);

    /* Deinit scene. */
    m_instrumentManager->unregisterScene(m_scene);

    disconnect(m_scene, NULL, this, NULL);
    disconnect(m_scene, NULL, context()->action(Qn::SelectionChangeAction), NULL);

    /* Clear curtain. */
    if(!m_curtainItem.isNull()) {
        delete m_curtainItem.data();
        m_curtainItem.clear();
    }
    m_curtainAnimator->setCurtainItem(NULL);

    /* Clear grid. */
    if(!m_gridItem.isNull())
        delete m_gridItem.data();

    /* Deinit workbench. */
    disconnect(workbench(), NULL, this, NULL);

    for(int i = 0; i < Qn::ItemRoleCount; i++)
        at_workbench_itemChanged(static_cast<Qn::ItemRole>(i), NULL);

    foreach(QnWorkbenchItem *item, workbench()->currentLayout()->items())
        removeItemInternal(item, true, false);
}

void QnWorkbenchDisplay::initSceneContext() {
    assert(m_scene != NULL && context() != NULL);

    /* Init scene. */
    m_instrumentManager->registerScene(m_scene);
    if(m_view != NULL) {
        m_view->setScene(m_scene);
        m_instrumentManager->registerView(m_view);

        initBoundingInstrument();
    }

    connect(m_scene,                SIGNAL(destroyed()),                            this,                   SLOT(at_scene_destroyed()));
    connect(m_scene,                SIGNAL(selectionChanged()),                     context()->action(Qn::SelectionChangeAction), SLOT(trigger()));
    connect(m_scene,                SIGNAL(selectionChanged()),                     this,                   SLOT(at_scene_selectionChanged()));

    /* Scene indexing will only slow everything down. */
    m_scene->setItemIndexMethod(QGraphicsScene::NoIndex);

    /* Set up curtain. */
    m_curtainItem = new QnCurtainItem();
    m_scene->addItem(m_curtainItem.data());
    setLayer(m_curtainItem.data(), Qn::CurtainLayer);
    m_curtainItem.data()->setColor(QColor(0, 0, 0, 255));
    m_curtainAnimator->setCurtainItem(m_curtainItem.data());

    /* Set up grid. */
    m_gridItem = new QnGridItem();
    m_scene->addItem(m_gridItem.data());
    setLayer(m_gridItem.data(), Qn::BackLayer);
    m_gridItem.data()->setAnimationSpeed(2.0);
    m_gridItem.data()->setAnimationTimeLimit(300);
    m_gridItem.data()->setColor(QColor(0, 240, 240, 128));
    m_gridItem.data()->setOpacity(0.0);
    m_gridItem.data()->setLineWidth(100.0);
    m_gridItem.data()->setMapper(workbench()->mapper());
    m_gridItem.data()->setAnimationTimer(m_instrumentManager->animationTimer());

    /* Connect to context. */
    connect(workbench(),            SIGNAL(itemChanged(Qn::ItemRole)),              this,                   SLOT(at_workbench_itemChanged(Qn::ItemRole)));
    connect(workbench(),            SIGNAL(itemAdded(QnWorkbenchItem *)),           this,                   SLOT(at_workbench_itemAdded(QnWorkbenchItem *)));
    connect(workbench(),            SIGNAL(itemRemoved(QnWorkbenchItem *)),         this,                   SLOT(at_workbench_itemRemoved(QnWorkbenchItem *)));
    connect(workbench(),            SIGNAL(boundingRectChanged()),                  this,                   SLOT(fitInView()));
    connect(workbench(),            SIGNAL(currentLayoutChanged()),                 this,                   SLOT(at_workbench_currentLayoutChanged()));

    /* Connect to grid mapper. */
    QnWorkbenchGridMapper *mapper = workbench()->mapper();
    connect(mapper,                 SIGNAL(originChanged()),                        this,                   SLOT(at_mapper_originChanged()));
    connect(mapper,                 SIGNAL(cellSizeChanged()),                      this,                   SLOT(at_mapper_cellSizeChanged()));
    connect(mapper,                 SIGNAL(spacingChanged()),                       this,                   SLOT(at_mapper_spacingChanged()));

    /* Create items. */
    foreach(QnWorkbenchItem *item, workbench()->currentLayout()->items())
        addItemInternal(item, false);

    for(int i = 0; i < Qn::ItemRoleCount; i++)
        if(workbench()->item(static_cast<Qn::ItemRole>(i)) != m_itemByRole[i])
            at_workbench_itemChanged(static_cast<Qn::ItemRole>(i));

    synchronizeSceneBounds();
}

void QnWorkbenchDisplay::setView(QGraphicsView *view) {
    if(m_view == view)
        return;

    if(m_view != NULL) {
        m_instrumentManager->unregisterView(m_view);

        disconnect(m_view, NULL, this, NULL);

        m_viewportAnimator->setView(NULL);
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
        m_view->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);

        /* In OpenGL mode this one seems to be ignored, but it will help in software mode. */
        m_view->setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);

        /* All our items save and restore painter state. */
        m_view->setOptimizationFlag(QGraphicsView::DontSavePainterState, false); /* Can be turned on if we won't be using framed widgets. */
        m_view->setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing);

        /* Don't even try to uncomment this one, it slows everything down. */
        //m_view->setCacheMode(QGraphicsView::CacheBackground);

        /* We don't need scrollbars. */
        m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        /* This may be needed by instruments. */
        m_view->setDragMode(QGraphicsView::NoDrag);
        m_view->viewport()->setAcceptDrops(true);

        /* Configure bounding instrument. */
        initBoundingInstrument();

        /* Configure viewport animator. */
        m_viewportAnimator->setView(m_view);
    }
}

void QnWorkbenchDisplay::initBoundingInstrument() {
    assert(m_view != NULL);

    m_boundingInstrument->setSizeEnforced(m_view, true);
    m_boundingInstrument->setPositionEnforced(m_view, true);
    m_boundingInstrument->setScalingSpeed(m_view, 32.0);
    m_boundingInstrument->setMovementSpeed(m_view, 4.0);
}

QnGridItem *QnWorkbenchDisplay::gridItem() {
    return m_gridItem.data();
}


// -------------------------------------------------------------------------- //
// QnWorkbenchDisplay :: item properties
// -------------------------------------------------------------------------- //
Qn::ItemLayer QnWorkbenchDisplay::layer(QGraphicsItem *item) const {
    bool ok;
    Qn::ItemLayer layer = static_cast<Qn::ItemLayer>(item->data(ITEM_LAYER_KEY).toInt(&ok));
    return ok ? layer : Qn::BackLayer;
}

void QnWorkbenchDisplay::setLayer(QGraphicsItem *item, Qn::ItemLayer layer) {
    if(item == NULL) {
        qnNullWarning(item);
        return;
    }

    /* Moving items back and forth between layers should preserve their relative
     * z order. Hence the fmod. */
    item->setData(ITEM_LAYER_KEY, static_cast<int>(layer));
    item->setZValue(layer * layerZSize + std::fmod(item->zValue(), layerZSize));

    QnResourceWidget *widget = item->isWidget() ? qobject_cast<QnResourceWidget *>(item->toGraphicsObject()) : NULL;
    if(widget)
        widget->shadow()->setZValue(shadowLayer(layer) * layerZSize);
}

void QnWorkbenchDisplay::setLayer(const QList<QGraphicsItem *> &items, Qn::ItemLayer layer) {
    foreach(QGraphicsItem *item, items)
        setLayer(item, layer);
}

WidgetAnimator *QnWorkbenchDisplay::animator(QnResourceWidget *widget) {
    WidgetAnimator *animator = widget->data(ITEM_ANIMATOR_KEY).value<WidgetAnimator *>();
    if(animator != NULL)
        return animator;

    /* Create if it's not there.
     *
     * Note that widget is set as animator's parent. */
    animator = new WidgetAnimator(widget, "enclosingGeometry", "rotation", widget); // ANIMATION: items.
    animator->setAbsoluteMovementSpeed(0.0);
    animator->setRelativeMovementSpeed(8.0);
    animator->setScalingSpeed(128.0);
    animator->setRotationSpeed(270.0);
    animator->setTimer(m_instrumentManager->animationTimer());
    animator->setTimeLimit(widgetAnimationDurationMsec);
    widget->setData(ITEM_ANIMATOR_KEY, QVariant::fromValue<WidgetAnimator *>(animator));
    return animator;
}

QnResourceWidget *QnWorkbenchDisplay::widget(QnWorkbenchItem *item) const {
    return m_widgetByItem.value(item);
}

QnResourceWidget *QnWorkbenchDisplay::widget(QnAbstractRenderer *renderer) const {
    return m_widgetByRenderer.value(renderer);
}

QnResourceWidget *QnWorkbenchDisplay::widget(Qn::ItemRole role) const {
    return widget(m_itemByRole[role]);
}

QList<QnResourceWidget *> QnWorkbenchDisplay::widgets() const {
    return m_widgetByRenderer.values();
}

QList<QnResourceWidget *> QnWorkbenchDisplay::widgets(const QnResourcePtr &resource) const {
    return m_widgetsByResource.value(resource);
}

QnResourceDisplay *QnWorkbenchDisplay::display(QnWorkbenchItem *item) const {
    if(QnResourceWidget *widget = this->widget(item))
        return widget->display();
    return NULL;
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

    return camera->getCamDisplay();
}


// -------------------------------------------------------------------------- //
// QnWorkbenchDisplay :: mutators
// -------------------------------------------------------------------------- //
void QnWorkbenchDisplay::fitInView(bool animate) {
    QRectF targetGeometry;

    QnWorkbenchItem *zoomedItem = m_itemByRole[Qn::ZoomedRole];
    if(zoomedItem != NULL) {
        targetGeometry = itemGeometry(zoomedItem);
    } else {
        targetGeometry = fitInViewGeometry();
    }

    if(animate) {
        m_viewportAnimator->moveTo(targetGeometry);
    } else {
        m_boundingInstrument->recursiveDisable();
        m_viewportAnimator->moveTo(targetGeometry, false);
        m_boundingInstrument->recursiveEnable(); /* So that caches are updated. */
    }
}

void QnWorkbenchDisplay::bringToFront(const QList<QGraphicsItem *> &items) {
    QList<QGraphicsItem *> localItems = items;

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

    QnResourceWidget *widget = this->widget(item);
    if(widget == NULL)
        return; /* Widget was not created for the given item. */
    
    bringToFront(widget);
}

bool QnWorkbenchDisplay::addItemInternal(QnWorkbenchItem *item, bool animate) {
    if (m_widgetByRenderer.size() >= 24) { // TODO: item limit must be changeable.
        qnDeleteLater(item);
        return false;
    }

    QnResourcePtr resource = qnResPool->getResourceByUniqId(item->resourceUid());
    if(resource.isNull()) {
        qnDeleteLater(item);
        return false;
    }

    if (!resource->checkFlags(QnResource::media)) { // TODO: unsupported for now
        qnDeleteLater(item);
        return false;
    }

    QnResourceWidget *widget = new QnResourceWidget(context(), item);
    widget->setParent(this); /* Just to feel totally safe and not to leak memory no matter what happens. */
    widget->setAttribute(Qt::WA_DeleteOnClose);
    widget->setFrameOpacity(m_frameOpacity);
    widget->setFrameWidth(defaultFrameWidth);

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

    {
        QPalette palette = widget->palette();
        palette.setColor(QPalette::Active, QPalette::Shadow, qnGlobals->selectedFrameColor());
        palette.setColor(QPalette::Inactive, QPalette::Shadow, qnGlobals->frameColor());
        widget->setPalette(palette);
    }

    m_scene->addItem(widget);

    connect(item, SIGNAL(geometryChanged()),                            this, SLOT(at_item_geometryChanged()));
    connect(item, SIGNAL(geometryDeltaChanged()),                       this, SLOT(at_item_geometryDeltaChanged()));
    connect(item, SIGNAL(rotationChanged()),                            this, SLOT(at_item_rotationChanged()));
    connect(item, SIGNAL(flagChanged(Qn::ItemFlag, bool)),              this, SLOT(at_item_flagChanged(Qn::ItemFlag, bool)));

    m_widgetByItem.insert(item, widget);
    m_widgetsByResource[widget->resource()].push_back(widget);
    if(widget->renderer() != NULL)
        m_widgetByRenderer.insert(widget->renderer(), widget);

    synchronize(widget, false);
    bringToFront(widget);

    if(item->checkFlag(Qn::PendingGeometryAdjustment))
        adjustGeometry(item, animate);

    connect(widget,                     SIGNAL(aboutToBeDestroyed()),   this,   SLOT(at_widget_aboutToBeDestroyed()));
    connect(m_widgetActivityInstrument, SIGNAL(activityStopped()),      widget, SLOT(showActivityDecorations()));
    connect(m_widgetActivityInstrument, SIGNAL(activityResumed()),      widget, SLOT(hideActivityDecorations()));
    if(widgets(widget->resource()).size() == 1)
        connect(widget->resource().data(),  SIGNAL(disabledChanged(bool, bool)), this, SLOT(at_resource_disabledChanged()), Qt::QueuedConnection);

    emit widgetAdded(widget);

    widget->display()->start();

    return true;
}

bool QnWorkbenchDisplay::removeItemInternal(QnWorkbenchItem *item, bool destroyWidget, bool destroyItem) {
    disconnect(item, NULL, this, NULL);

    QnResourceWidget *widget = m_widgetByItem.value(item);
    if(widget == NULL) {
        assert(!destroyItem);
        return false; /* The widget wasn't created. */
    }

    disconnect(widget, NULL, this, NULL);
    disconnect(m_widgetActivityInstrument, NULL, widget, NULL);
    if(widgets(widget->resource()).size() == 1)
        disconnect(widget->resource().data(), NULL, this, NULL);

    emit widgetAboutToBeRemoved(widget);

    m_widgetByItem.remove(item);
    m_widgetsByResource[widget->resource()].removeOne(widget);
    if(widget->renderer() != NULL)
        m_widgetByRenderer.remove(widget->renderer());

    /* We better clear these as soon as possible. */
    for(int i = 0; i < Qn::ItemRoleCount; i++)
        if(item == workbench()->item(static_cast<Qn::ItemRole>(i)))
            workbench()->setItem(static_cast<Qn::ItemRole>(i), NULL);

    if(destroyWidget) {
        widget->hide();
        qnDeleteLater(widget);
    }

    if(destroyItem)
        qnDeleteLater(item);

    return true;
}

QMargins QnWorkbenchDisplay::viewportMargins() const {
    return m_viewportAnimator->viewportMargins();
}

void QnWorkbenchDisplay::setViewportMargins(const QMargins &margins) {
    if(viewportMargins() == margins)
        return;

    m_viewportAnimator->setViewportMargins(margins);

    synchronizeSceneBoundsExtension();
}

Qn::MarginFlags QnWorkbenchDisplay::currentMarginFlags() const {
    return m_viewportAnimator->marginFlags();
}

Qn::MarginFlags QnWorkbenchDisplay::zoomedMarginFlags() const {
    return m_zoomedMarginFlags;
}

Qn::MarginFlags QnWorkbenchDisplay::normalMarginFlags() const {
    return m_normalMarginFlags;
}

void QnWorkbenchDisplay::setZoomedMarginFlags(Qn::MarginFlags flags) {
    if(m_zoomedMarginFlags == flags)
        return;

    m_zoomedMarginFlags = flags;

    updateCurrentMarginFlags();
}

void QnWorkbenchDisplay::setNormalMarginFlags(Qn::MarginFlags flags) {
    if(m_normalMarginFlags == flags)
        return;

    m_normalMarginFlags = flags;

    updateCurrentMarginFlags();
}

void QnWorkbenchDisplay::updateCurrentMarginFlags() {
    Qn::MarginFlags flags;
    if(m_itemByRole[Qn::ZoomedRole] == NULL) {
        flags = m_normalMarginFlags;
    } else {
        flags = m_zoomedMarginFlags;
    }
    if(flags == currentMarginFlags())
        return;

    m_viewportAnimator->setMarginFlags(flags);

    synchronizeSceneBoundsExtension();
}

qreal QnWorkbenchDisplay::widgetsFrameOpacity() const {
    return m_frameOpacity;
}

void QnWorkbenchDisplay::setWidgetsFrameOpacity(qreal opacity) {
    if(qFuzzyCompare(m_frameOpacity, opacity))
        return;

    m_frameOpacity = opacity;
    
    foreach(QnResourceWidget *widget, m_widgetByRenderer) 
        widget->setFrameOpacity(opacity);
}


// -------------------------------------------------------------------------- //
// QnWorkbenchDisplay :: calculators
// -------------------------------------------------------------------------- //
qreal QnWorkbenchDisplay::layerFrontZValue(Qn::ItemLayer layer) const {
    return layerZValue(layer) + m_frontZ;
}

qreal QnWorkbenchDisplay::layerZValue(Qn::ItemLayer layer) const {
    return layer * layerZSize;
}

Qn::ItemLayer QnWorkbenchDisplay::shadowLayer(Qn::ItemLayer itemLayer) const {
    switch(itemLayer) {
    case Qn::PinnedRaisedLayer:
        return Qn::PinnedLayer;
    case Qn::UnpinnedRaisedLayer:
        return Qn::UnpinnedLayer;
    default:
        return itemLayer;
    }
}

Qn::ItemLayer QnWorkbenchDisplay::synchronizedLayer(QnWorkbenchItem *item) const {
    assert(item != NULL);

    if(item == m_itemByRole[Qn::ZoomedRole]) {
        return Qn::ZoomedLayer;
    } else if(item->isPinned()) {
        if(item == m_itemByRole[Qn::RaisedRole]) {
            return Qn::PinnedRaisedLayer;
        } else {
            return Qn::PinnedLayer;
        }
    } else {
        if(item == m_itemByRole[Qn::RaisedRole]) {
            return Qn::UnpinnedRaisedLayer;
        } else {
            return Qn::UnpinnedLayer;
        }
    }
}

QRectF QnWorkbenchDisplay::itemEnclosingGeometry(QnWorkbenchItem *item) const {
    if(item == NULL) {
        qnNullWarning(item);
        return QRectF();
    }

    QRectF result = workbench()->mapper()->mapFromGrid(item->geometry());

    QSizeF step = workbench()->mapper()->step();
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

    QnResourceWidget *widget = this->widget(item);
    if(widget == NULL) {
        /* A perfectly normal situation - the widget was not created. */
        if(enclosingGeometry)
            *enclosingGeometry = QRectF();
        return QRectF();
    }

    QRectF result = itemEnclosingGeometry(item);
    if(enclosingGeometry != NULL)
        *enclosingGeometry = rotated(result, item->rotation());

    if(!widget->hasAspectRatio())
        return rotated(result, item->rotation());

    return rotated(expanded(widget->aspectRatio(), result, Qt::KeepAspectRatio), item->rotation());
}

QRectF QnWorkbenchDisplay::layoutBoundingGeometry() const {
    return fitInViewGeometry();
}

QRectF QnWorkbenchDisplay::fitInViewGeometry() const {
    QRect layoutBoundingRect = workbench()->currentLayout()->boundingRect();
    if(layoutBoundingRect.isNull())
        layoutBoundingRect = QRect(0, 0, 1, 1);

    return workbench()->mapper()->mapFromGridF(QRectF(layoutBoundingRect).adjusted(-0.05, -0.05, 0.05, 0.05));
}

QRectF QnWorkbenchDisplay::viewportGeometry() const {
    if(m_view == NULL) {
        return QRectF();
    } else {
        return m_viewportAnimator->accessor()->get(m_view).toRectF();
    }
}

QPoint QnWorkbenchDisplay::mapViewportToGrid(const QPoint &viewportPoint) const {
    if(m_view == NULL)
        return QPoint();

    return workbench()->mapper()->mapToGrid(m_view->mapToScene(viewportPoint));
}

QPoint QnWorkbenchDisplay::mapGlobalToGrid(const QPoint &globalPoint) const {
    if(m_view == NULL)
        return QPoint();

    return mapViewportToGrid(m_view->mapFromGlobal(globalPoint));
}

QPointF QnWorkbenchDisplay::mapViewportToGridF(const QPoint &viewportPoint) const {
    if(m_view == NULL)
        return QPointF();

    return workbench()->mapper()->mapToGridF(m_view->mapToScene(viewportPoint));

}

QPointF QnWorkbenchDisplay::mapGlobalToGridF(const QPoint &globalPoint) const {
    if(m_view == NULL)
        return QPoint();

    return mapViewportToGridF(m_view->mapFromGlobal(globalPoint));
}



// -------------------------------------------------------------------------- //
// QnWorkbenchDisplay :: synchronizers
// -------------------------------------------------------------------------- //
void QnWorkbenchDisplay::synchronize(QnWorkbenchItem *item, bool animate) {
    if(item == NULL) {
        qnNullWarning(item);
        return;
    }

    QnResourceWidget *widget = this->widget(item);
    if(widget == NULL)
        return; /* No widget was created for the item provided. */

    synchronize(widget, animate);
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
    QnResourceWidget *widget = this->widget(item);
    if(widget == NULL)
        return; /* No widget was created for the given item. */

    synchronizeGeometry(widget, animate);
}

void QnWorkbenchDisplay::synchronizeGeometry(QnResourceWidget *widget, bool animate) {
    if(widget == NULL) {
        qnNullWarning(widget);
        return;
    }

    QnWorkbenchItem *item = widget->item();
    QnWorkbenchItem *zoomedItem = m_itemByRole[Qn::ZoomedRole];
    QnWorkbenchItem *raisedItem = m_itemByRole[Qn::RaisedRole];

    QRectF enclosingGeometry = itemEnclosingGeometry(item);

    /* Adjust for raise. */
    if(item == raisedItem && item != zoomedItem && m_view != NULL) {
        QRectF viewportGeometry = mapRectToScene(m_view, m_view->viewport()->rect());

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
    if(item == raisedItem || item == zoomedItem)
        bringToFront(widget);

    /* Update enclosing aspect ratio. */
    widget->setEnclosingAspectRatio(enclosingGeometry.width() / enclosingGeometry.height());

    /* Move! */
    WidgetAnimator *animator = this->animator(widget);
    if(animate) {
        QEasingCurve easingCurve;

        QSizeF currentSize = widget->enclosingGeometry().size();
        QSizeF targetSize = enclosingGeometry.size();

        animator->moveTo(enclosingGeometry, item->rotation(), easingCurve);
    } else {
        animator->stop();
        widget->setEnclosingGeometry(enclosingGeometry);
        widget->setRotation(item->rotation());
    }
}

void QnWorkbenchDisplay::synchronizeAllGeometries(bool animate) {
    foreach(QnResourceWidget *widget, m_widgetByItem)
        synchronizeGeometry(widget, animate);
}

void QnWorkbenchDisplay::synchronizeLayer(QnWorkbenchItem *item) {
    synchronizeLayer(widget(item));
}

void QnWorkbenchDisplay::synchronizeLayer(QnResourceWidget *widget) {
    setLayer(widget, synchronizedLayer(widget->item()));
}

void QnWorkbenchDisplay::synchronizeSceneBounds() {
    if(m_instrumentManager->scene() == NULL)
        return; /* Do nothing if scene is being destroyed. */

    QRectF sizeRect, moveRect;

    QnWorkbenchItem *zoomedItem = m_itemByRole[Qn::ZoomedRole];
    if(zoomedItem != NULL) {
        sizeRect = moveRect = itemGeometry(zoomedItem);
    } else {
        moveRect = layoutBoundingGeometry();
        sizeRect = fitInViewGeometry();
    }

    m_boundingInstrument->setPositionBounds(m_view, moveRect);
    m_boundingInstrument->setSizeBounds(m_view, viewportLowerSizeBound, Qt::KeepAspectRatioByExpanding, sizeRect.size(), Qt::KeepAspectRatioByExpanding);
}

void QnWorkbenchDisplay::synchronizeSceneBoundsExtension() {
    MarginsF marginsExtension(0.0, 0.0, 0.0, 0.0);
    if(currentMarginFlags() != 0)
        marginsExtension = cwiseDiv(m_viewportAnimator->viewportMargins(), m_view->viewport()->size());

    /* Sync position extension. */
    {
        MarginsF positionExtension(0.0, 0.0, 0.0, 0.0);
        if(currentMarginFlags() & Qn::MarginsAffectPosition)
            positionExtension = marginsExtension;

        QnWorkbenchItem *zoomedItem = m_itemByRole[Qn::ZoomedRole];
        if(zoomedItem != NULL) {
            m_boundingInstrument->setPositionBoundsExtension(m_view, positionExtension);
        } else {
            m_boundingInstrument->setPositionBoundsExtension(m_view, positionExtension + MarginsF(0.5, 0.5, 0.5, 0.5));
        }
    }

    /* Sync size extension. */
    if(currentMarginFlags() & Qn::MarginsAffectSize) {
        QSizeF sizeExtension = sizeDelta(marginsExtension);
        sizeExtension = cwiseDiv(sizeExtension, QSizeF(1.0, 1.0) - sizeExtension);

        m_boundingInstrument->setSizeBoundsExtension(m_view, sizeExtension, sizeExtension);
        if(m_itemByRole[Qn::ZoomedRole] == NULL)
            m_boundingInstrument->stickScale(m_view);
    }
}

void QnWorkbenchDisplay::synchronizeRaisedGeometry() {
    QnWorkbenchItem *raisedItem = m_itemByRole[Qn::RaisedRole];
    if(raisedItem == NULL)
        return;

    QnResourceWidget *widget = this->widget(raisedItem);
    synchronizeGeometry(widget, animator(widget)->isRunning());
}

void QnWorkbenchDisplay::adjustGeometry(QnWorkbenchItem *item, bool animate) {
    if(!item->checkFlag(Qn::PendingGeometryAdjustment))
        return; /* May have been unchecked already. */

    QnResourceWidget *widget = this->widget(item);
    if(widget == NULL)
        return; /* No widget was created for the given item. */

    /* Calculate target position. */
    QPointF newPos;
    if(item->geometry().width() < 0 || item->geometry().height() < 0) {
        newPos = mapViewportToGridF(m_view->viewport()->geometry().center());

        /* Initialize item's geometry with adequate values. */
        item->setFlag(Qn::Pinned, false);
        item->setCombinedGeometry(QRectF(newPos, QSizeF(0.0, 0.0)));
        synchronizeGeometry(widget, false);
    } else {
        newPos = item->combinedGeometry().center();

        if(qFuzzyIsNull(item->combinedGeometry().size()))
            synchronizeGeometry(widget, false);
    }

    /* Assume 4:3 AR of a single channel. In most cases, it will work fine. */
    const QnVideoResourceLayout *videoLayout = widget->display()->videoLayout();
    const qreal estimatedAspectRatio = (4.0 * videoLayout->width()) / (3.0 * videoLayout->height());
    const Qt::Orientation orientation = estimatedAspectRatio > 1.0 ? Qt::Vertical : Qt::Horizontal;
    const QSize size = bestSingleBoundedSize(workbench()->mapper(), 1, orientation, estimatedAspectRatio);

    /* Adjust item's geometry for the new size. */
    if(size != item->geometry().size()) {
        QRectF combinedGeometry = item->combinedGeometry();
        combinedGeometry.moveTopLeft(combinedGeometry.topLeft() - toPoint(size - combinedGeometry.size()) / 2.0);
        combinedGeometry.setSize(size);
        item->setCombinedGeometry(combinedGeometry);
    }

    /* Pin the item. */
    QnAspectRatioMagnitudeCalculator metric(newPos, size, item->layout()->boundingRect(), aspectRatio(m_view->viewport()->size()) / aspectRatio(workbench()->mapper()->step()));
    QRect geometry = item->layout()->closestFreeSlot(newPos, size, &metric);
    item->layout()->pinItem(item, geometry);
    item->setFlag(Qn::PendingGeometryAdjustment, false);

    /* Synchronize. */
    synchronizeGeometry(item, animate);
}

void QnWorkbenchDisplay::updateFrameWidths() {
    if(!m_frameWidthsDirty)
        return;

    foreach(QnResourceWidget *widget, m_widgetByRenderer)
        widget->setFrameWidth(widget->isSelected() ? selectedFrameWidth : defaultFrameWidth);
}


// -------------------------------------------------------------------------- //
// QnWorkbenchDisplay :: handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchDisplay::at_viewportAnimator_finished() {
    synchronizeSceneBounds();
}

void QnWorkbenchDisplay::at_workbench_itemAdded(QnWorkbenchItem *item) {
    if(addItemInternal(item, true)) {
        synchronizeSceneBounds();
        fitInView();
    }
}

void QnWorkbenchDisplay::at_workbench_itemRemoved(QnWorkbenchItem *item) {
    if(removeItemInternal(item, true, false)) {
        synchronizeSceneBounds();
        fitInView();
    }
}

void QnWorkbenchDisplay::at_workbench_itemChanged(Qn::ItemRole role, QnWorkbenchItem *item) {
    if(item == m_itemByRole[role])
        return;

    QnWorkbenchItem *oldItem = m_itemByRole[role];
    QnWorkbenchItem *newItem = item;
    m_itemByRole[role] = newItem;

    /* Sync geometry. */
    switch(role) {
    case Qn::RaisedRole: {
        /* Sync new & old items. */
        if(oldItem != NULL)
            synchronize(oldItem);

        if(newItem != NULL) {
            bringToFront(newItem);
            synchronize(newItem, true);
        }

        break;
    }
    case Qn::ZoomedRole: {
        if(oldItem != NULL) {
            synchronize(oldItem, true);

            m_curtainActivityInstrument->recursiveDisable();
        }

        if(newItem != NULL) {
            bringToFront(newItem);
            synchronize(newItem, true);

            m_curtainActivityInstrument->recursiveEnable();

            m_viewportAnimator->moveTo(itemGeometry(newItem));
        } else {
            m_viewportAnimator->moveTo(fitInViewGeometry());
        }

        synchronizeSceneBounds();
        synchronizeSceneBoundsExtension();
        break;
    }
    case Qn::CentralRole: {
        /* Update audio playback. */
        CLCamDisplay *oldCamDisplay = camDisplay(oldItem);
        if(oldCamDisplay != NULL)
            oldCamDisplay->playAudio(false);

        CLCamDisplay *newCamDisplay = camDisplay(newItem);
        if(newCamDisplay != NULL)
            newCamDisplay->playAudio(true);
        break;
    }
    default:
        qnWarning("Invalid item role %1.", static_cast<int>(role));
        return;
    }

    /* Update media quality. */
    if(role == Qn::ZoomedRole) {
        if(QnResourceWidget *oldWidget = this->widget(oldItem)) {
            if (oldWidget->display()->archiveReader()) {
                //oldWidget->display()->archiveReader()->setQuality(MEDIA_Quality_Low);
            	oldWidget->display()->archiveReader()->enableQualityChange();
            }
        }
        if(QnResourceWidget *newWidget = this->widget(newItem)) {
            if (newWidget->display()->archiveReader()) {
            	newWidget->display()->archiveReader()->setQuality(MEDIA_Quality_High, true);
            	newWidget->display()->archiveReader()->disableQualityChange();
			}
        }
    }

    /* Hide / show other items when zoomed. */
    if(role == Qn::ZoomedRole) {
        QnWorkbenchItem *zoomedItem = m_itemByRole[Qn::ZoomedRole];
        if(zoomedItem)
            opacityAnimator(widget(zoomedItem))->animateTo(1.0);
        qreal opacity = zoomedItem ? 0.0 : 1.0;

        foreach(QnResourceWidget *widget, m_widgetByRenderer)
            if(widget->item() != zoomedItem)
                opacityAnimator(widget)->animateTo(opacity);
    }

    /* Update margin flags. */
    updateCurrentMarginFlags();

    emit widgetChanged(role);
}


void QnWorkbenchDisplay::at_workbench_itemChanged(Qn::ItemRole role) {
    at_workbench_itemChanged(role, workbench()->item(role));
}

void QnWorkbenchDisplay::at_workbench_currentLayoutChanged() {
    fitInView(false);
}

void QnWorkbenchDisplay::at_item_geometryChanged() {
    synchronizeGeometry(static_cast<QnWorkbenchItem *>(sender()), true);
    synchronizeSceneBounds();
}

void QnWorkbenchDisplay::at_item_geometryDeltaChanged() {
    synchronizeGeometry(static_cast<QnWorkbenchItem *>(sender()), true);
}

void QnWorkbenchDisplay::at_item_rotationChanged() {
    QnWorkbenchItem *item = static_cast<QnWorkbenchItem *>(sender());

    synchronizeGeometry(item, true);
    if(item == m_itemByRole[Qn::ZoomedRole])
        synchronizeSceneBounds();
}

void QnWorkbenchDisplay::at_item_flagChanged(Qn::ItemFlag flag, bool value) {
    switch(flag) {
    case Qn::Pinned:
        synchronizeLayer(static_cast<QnWorkbenchItem *>(sender()));
        break;
    case Qn::PendingGeometryAdjustment:
        if(value)
            adjustGeometry(static_cast<QnWorkbenchItem *>(sender()));
        break;
    default:
        qnWarning("Invalid item flag '%1'.", static_cast<int>(flag));
        break;
    }
}

void QnWorkbenchDisplay::at_activityStopped() {
    m_curtainAnimator->curtain(widget(m_itemByRole[Qn::ZoomedRole]));
}

void QnWorkbenchDisplay::at_activityStarted() {
    m_curtainAnimator->uncurtain();
}

void QnWorkbenchDisplay::at_curtained() {
    QnWorkbenchItem *zoomedItem = m_itemByRole[Qn::ZoomedRole];
    foreach(QnResourceWidget *widget, m_widgetByItem)
        if(widget->item() != zoomedItem)
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

void QnWorkbenchDisplay::at_widget_aboutToBeDestroyed() {
    QnResourceWidget *widget = checked_cast<QnResourceWidget *>(sender());
    if (widget && widget->item()) {
        /* We can get here only when the widget is destroyed directly 
         * (not by destroying or removing its corresponding item). 
         * Therefore the widget's item must be destroyed. */
        removeItemInternal(widget->item(), false, true);
    }
}

void QnWorkbenchDisplay::at_scene_destroyed() {
    setScene(NULL);
}

void QnWorkbenchDisplay::at_scene_selectionChanged() {
    m_frameWidthsDirty = true;
}

void QnWorkbenchDisplay::at_view_destroyed() {
    setView(NULL);
}

void QnWorkbenchDisplay::at_mapper_originChanged() {
    synchronizeAllGeometries(true);

    synchronizeSceneBounds();

    fitInView();
}

void QnWorkbenchDisplay::at_mapper_cellSizeChanged() {
    synchronizeAllGeometries(true);

    synchronizeSceneBounds();

    fitInView();
}

void QnWorkbenchDisplay::at_mapper_spacingChanged() {
    synchronizeAllGeometries(true);

    synchronizeSceneBounds();

    fitInView();

    QSizeF spacing = workbench()->mapper()->spacing();
    if(qFuzzyIsNull(spacing.width()) || qFuzzyIsNull(spacing.height())) {
        m_frameOpacityAnimator->animateTo(0.0);
    } else {
        m_frameOpacityAnimator->animateTo(1.0);
    }
}

void QnWorkbenchDisplay::at_context_permissionsChanged(const QnResourcePtr &resource) {
    if(QnLayoutResourcePtr layoutResource = resource.dynamicCast<QnLayoutResource>()) {
        if(QnWorkbenchLayout *layout = QnWorkbenchLayout::instance(layoutResource)) {
            Qn::Permissions permissions = accessController()->permissions(resource);

            if(!(permissions & Qn::ReadPermission))
                workbench()->removeLayout(layout);
        }
    }
}

void QnWorkbenchDisplay::at_resource_disabledChanged() {
    QObject *sender = this->sender();
    if(!sender)
        return; /* Already disconnected from this sender. */

    at_resource_disabledChanged(toSharedPointer(checked_cast<QnResource *>(sender)));
}

void QnWorkbenchDisplay::at_resource_disabledChanged(const QnResourcePtr &resource) {
    QnResourcePtr enabledResource = resourcePool()->getEnabledResourceByUniqueId(resource->getUniqueId());
    if(!enabledResource || enabledResource == resource)
        return;

    foreach(QnResourceWidget *widget, widgets(resource)) {
        QnWorkbenchItem *item = widget->item();

        removeItemInternal(item, true, false);
        addItemInternal(item, false);
    }
}
