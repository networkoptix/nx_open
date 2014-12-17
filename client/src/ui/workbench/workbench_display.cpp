#include "workbench_display.h"
#include <cassert>
#include <cstring> /* For std::memset. */
#include <cmath> /* For std::fmod. */

#include <functional> /* For std::binary_function. */

#include <QtCore/QtAlgorithms>
#include <QtWidgets/QAction>
#include <QtOpenGL/QGLContext>
#include <QtOpenGL/QGLWidget>

#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>
#include <utils/common/delete_later.h>
#include <utils/math/math.h>
#include <utils/math/color_transformations.h>
#include <utils/common/toggle.h>
#include <utils/common/util.h>
#include <utils/common/variant_timer.h>
#include <utils/aspect_ratio.h>

#include <client/client_meta_types.h>
#include <common/common_meta_types.h>

#include <core/resource/layout_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource_management/resource_pool.h>
#include <camera/resource_display.h>
#include <camera/client_video_camera.h>

#include <redass/redass_controller.h>

#include <ui/actions/action_manager.h>
#include <ui/actions/action_target_provider.h>

#include <ui/common/notification_levels.h>

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
#include <ui/graphics/instruments/tool_tip_instrument.h>
#include <ui/graphics/instruments/widget_layout_instrument.h>

#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/server_resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/resource/videowall_screen_widget.h>
#include <ui/graphics/items/resource/resource_widget_renderer.h>
#include <ui/graphics/items/resource/decodedpicturetoopengluploadercontextpool.h>
#include <ui/graphics/items/grid/curtain_item.h>
#include <ui/graphics/items/generic/graphics_message_box.h>
#include <ui/graphics/items/generic/splash_item.h>
#include <ui/graphics/items/grid/grid_item.h>
#include <ui/graphics/items/grid/grid_background_item.h>
#include <ui/graphics/items/grid/grid_raised_cone_item.h>

#include <ui/graphics/opengl/gl_hardware_checker.h>

#include <ui/graphics/view/gradient_background_painter.h>

#include <ui/workaround/gl_widget_factory.h>
#include <ui/workaround/gl_widget_workaround.h>

#include <ui/style/skin.h>
#include <ui/style/globals.h>

#include "extensions/workbench_stream_synchronizer.h"
#include "workbench_layout.h"
#include "workbench_item.h"
#include "workbench_grid_mapper.h"
#include "workbench_utility.h"
#include "workbench_context.h"
#include "workbench_access_controller.h"
#include "workbench.h"

#include "core/dataprovider/abstract_streamdataprovider.h"
#include "plugins/resource/archive/abstract_archive_stream_reader.h"

#include <ui/workbench/handlers/workbench_action_handler.h> // TODO: remove
#include <ui/workbench/handlers/workbench_notifications_handler.h>

#include "camera/thumbnails_loader.h" // TODO: remove?
#include "watchers/workbench_server_time_watcher.h"


namespace {
    struct GraphicsItemZLess: public std::binary_function<QGraphicsItem *, QGraphicsItem *, bool> {
        bool operator()(QGraphicsItem *l, QGraphicsItem *r) const {
            return l->zValue() < r->zValue();
        }
    };

    struct WidgetPositionLess {
        bool operator()(QnResourceWidget *l, QnResourceWidget *r) const {
            QRect lg = l->item()->geometry();
            QRect rg = r->item()->geometry();
            return lg.y() < rg.y() || (lg.y() == rg.y() && lg.x() < rg.x());
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

    /** Size multiplier for raised widgets. */
    const qreal focusExpansion = 100.0;

    /** Maximal expanded size of a raised widget, relative to viewport size. */
    const qreal maxExpandedSize = 0.5;

    /** Viewport lower size boundary, in scene coordinates. */
    const QSizeF viewportLowerSizeBound = QSizeF(qnGlobals->workbenchUnitSize() * 0.05, qnGlobals->workbenchUnitSize() * 0.05);

    const qreal defaultFrameWidth = qnGlobals->workbenchUnitSize() * 0.005; // TODO: #Elric move to settings
    const qreal selectedFrameWidth = defaultFrameWidth * 2; // TODO: #Elric same here

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
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_scene(NULL),
    m_view(NULL),
    m_lightMode(0),
    m_frontZ(0.0),
    m_frameOpacity(1.0),
    m_frameWidthsDirty(false),
    m_zoomedMarginFlags(0),
    m_normalMarginFlags(0),
    m_inChangeLayout(false),
    m_instrumentManager(new InstrumentManager(this)),
    m_viewportAnimator(NULL),
    m_curtainAnimator(NULL),
    m_frameOpacityAnimator(NULL),
    m_loader(NULL)
{
    std::memset(m_widgetByRole, 0, sizeof(m_widgetByRole));

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
    m_instrumentManager->installInstrument(new WidgetLayoutInstrument(this));
    m_instrumentManager->installInstrument(m_beforePaintInstrument);
    m_instrumentManager->installInstrument(m_transformListenerInstrument);
    m_instrumentManager->installInstrument(m_focusListenerInstrument);
    m_instrumentManager->installInstrument(resizeSignalingInstrument);
    m_instrumentManager->installInstrument(m_boundingInstrument);
    m_instrumentManager->installInstrument(m_curtainActivityInstrument);
    m_instrumentManager->installInstrument(m_widgetActivityInstrument);
    m_instrumentManager->installInstrument(m_selectionOverlayHackInstrument);
    m_instrumentManager->installInstrument(new ToolTipInstrument(this));

    m_curtainActivityInstrument->recursiveDisable();

    connect(m_transformListenerInstrument,  SIGNAL(transformChanged(QGraphicsView *)),                  this,                   SLOT(synchronizeRaisedGeometry()));
    connect(resizeSignalingInstrument,      SIGNAL(activated(QWidget *, QEvent *)),                     this,                   SLOT(synchronizeRaisedGeometry()));
    connect(resizeSignalingInstrument,      SIGNAL(activated(QWidget *, QEvent *)),                     this,                   SLOT(synchronizeSceneBoundsExtension()));

    //queueing connection because some OS make resize call before move widget to correct postion while expanding it to fullscreen --gdm
    connect(resizeSignalingInstrument,      SIGNAL(activated(QWidget *, QEvent *)),                     this,                   SLOT(fitInView()), Qt::QueuedConnection);

    connect(m_beforePaintInstrument,        SIGNAL(activated(QWidget *, QEvent *)),                     this,                   SLOT(updateFrameWidths()));
    connect(m_curtainActivityInstrument,    SIGNAL(activityStopped()),                                  this,                   SLOT(at_curtainActivityInstrument_activityStopped()));
    connect(m_curtainActivityInstrument,    SIGNAL(activityResumed()),                                  this,                   SLOT(at_curtainActivityInstrument_activityStarted()));
    connect(m_widgetActivityInstrument,     SIGNAL(activityStopped()),                                  this,                   SLOT(at_widgetActivityInstrument_activityStopped()));
    connect(m_widgetActivityInstrument,     SIGNAL(activityResumed()),                                  this,                   SLOT(at_widgetActivityInstrument_activityStarted()));

    /* Create zoomed toggle. */
    m_zoomedToggle = new QnToggle(false, this);
    connect(m_zoomedToggle,                 SIGNAL(activated()),                                        m_curtainActivityInstrument, SLOT(recursiveEnable()));
    connect(m_zoomedToggle,                 SIGNAL(deactivated()),                                      m_curtainActivityInstrument, SLOT(recursiveDisable()));

    /* Create curtain animator. */
    m_curtainAnimator = new QnCurtainAnimator(this);
    m_curtainAnimator->setSpeed(1.0); /* (255, 0, 0) -> (0, 0, 0) in 1 second. */
    m_curtainAnimator->setTimer(animationTimer);
    connect(m_curtainAnimator,              SIGNAL(curtained()),                                        this,                   SLOT(updateCurtainedCursor()));
    connect(m_curtainAnimator,              SIGNAL(uncurtained()),                                      this,                   SLOT(updateCurtainedCursor()));

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
    connect(context()->instance<QnWorkbenchNotificationsHandler>(), SIGNAL(businessActionAdded(const QnAbstractBusinessActionPtr &)), this, SLOT(at_notificationsHandler_businessActionAdded(const QnAbstractBusinessActionPtr &)));

    /* Set up defaults. */
    connect(this, SIGNAL(geometryAdjustmentRequested(QnWorkbenchItem *, bool)), this, SLOT(adjustGeometry(QnWorkbenchItem *, bool)), Qt::QueuedConnection);

    connect(action(Qn::ToggleBackgroundAnimationAction),   &QAction::toggled,  this,   &QnWorkbenchDisplay::toggleBackgroundAnimation);
}

QnWorkbenchDisplay::~QnWorkbenchDisplay() {
    setScene(NULL);
}

Qn::LightModeFlags QnWorkbenchDisplay::lightMode() const {
    return m_lightMode;
}

void QnWorkbenchDisplay::setLightMode(Qn::LightModeFlags mode) {
    if(m_lightMode == mode)
        return;

    if(m_scene && m_view)
        deinitSceneView();

    m_lightMode = mode;

    if(m_scene && m_view)
        initSceneView();
}

void QnWorkbenchDisplay::setScene(QGraphicsScene *scene) {
    if(m_scene == scene)
        return;

    if(m_scene && m_view)
        deinitSceneView();

    m_scene = scene;

    if(m_scene && m_view)
        initSceneView();
}

void QnWorkbenchDisplay::setView(QnGraphicsView *view) {
    if(m_view == view)
        return;

    if(m_scene && m_view)
        deinitSceneView();

    m_view = view;

    if(m_scene && m_view)
        initSceneView();
}

void QnWorkbenchDisplay::deinitSceneView() {
    assert(m_scene && m_view);

    /* Deinit view. */
    m_instrumentManager->unregisterView(m_view);
    m_viewportAnimator->setView(NULL);

    disconnect(m_view, NULL, this, NULL);

    /* Deinit scene. */
    m_instrumentManager->unregisterScene(m_scene);

    disconnect(m_scene, NULL, this, NULL);
    disconnect(m_scene, NULL, context()->action(Qn::SelectionChangeAction), NULL);
    disconnect(action(Qn::SelectionChangeAction), NULL, this, NULL);

    /* Clear curtain. */
    if(!m_curtainItem.isNull()) {
        delete m_curtainItem.data();
        m_curtainItem.clear();
    }
    m_curtainAnimator->setCurtainItem(NULL);

    /* Clear grid. */
    if(!m_gridItem.isNull())
        delete m_gridItem.data();

    /* Clear background painter. */
    if (!m_backgroundPainter.isNull()) {
        m_view->uninstallLayerPainter(m_backgroundPainter.data());
        delete m_backgroundPainter.data();
    }


    /* Deinit workbench. */
    disconnect(workbench(), NULL, this, NULL);

    for(int i = 0; i < Qn::ItemRoleCount; i++)
        at_workbench_itemChanged(static_cast<Qn::ItemRole>(i), NULL);

    foreach(QnWorkbenchItem *item, workbench()->currentLayout()->items())
        removeItemInternal(item, true, false);

    if (gridBackgroundItem())
        delete gridBackgroundItem();
}

QGLWidget *QnWorkbenchDisplay::newGlWidget(QWidget *parent, Qt::WindowFlags windowFlags) const {
    return QnGlWidgetFactory::create<QnGLWidget>(parent, windowFlags);
}

void QnWorkbenchDisplay::initSceneView() {
    assert(m_scene && m_view);

    /* Init scene. */
    m_instrumentManager->registerScene(m_scene);

    /* Note that selection often changes there and back, and we don't want such changes to
     * affect our logic, so we use queued connections here. */ // TODO: #Elric I don't see queued connections
    connect(m_scene,                SIGNAL(selectionChanged()),                     context()->action(Qn::SelectionChangeAction), SLOT(trigger()));
    connect(m_scene,                SIGNAL(selectionChanged()),                     this,                   SLOT(at_scene_selectionChanged()));
    connect(m_scene,                SIGNAL(destroyed()),                            this,                   SLOT(at_scene_destroyed()));

    connect(action(Qn::SelectionChangeAction), &QAction::triggered,                 this,                   &QnWorkbenchDisplay::updateSelectionFromTree);

    /* Scene indexing will only slow everything down. */
    m_scene->setItemIndexMethod(QGraphicsScene::NoIndex);

    /* Init view. */
    m_view->setScene(m_scene);
    m_instrumentManager->registerView(m_view);

    connect(m_view, SIGNAL(destroyed()), this, SLOT(at_view_destroyed()));

    /* Configure OpenGL */
    static const char *qn_viewInitializedPropertyName = "_qn_viewInitialized";
    if(!m_view->property(qn_viewInitializedPropertyName).toBool()) {
        QGLWidget *viewport = newGlWidget(m_view);
            
        m_view->setViewport(viewport);

        viewport->makeCurrent();
        QnGlHardwareChecker::checkCurrentContext(true);

        /* Initializing gl context pool used to render decoded pictures in non-GUI thread. */
        DecodedPictureToOpenGLUploaderContextPool::instance()->ensureThereAreContextsSharedWith(viewport);

        /* Turn on antialiasing at QPainter level. */
        m_view->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);

        /* In OpenGL mode this one seems to be ignored, but it will help in software mode. */
        m_view->setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);

        /* All our items save and restore painter state. */
        m_view->setOptimizationFlag(QGraphicsView::DontSavePainterState, false); /* Can be turned on if we won't be using framed widgets. */

#ifndef Q_OS_MACX
        /* On macos, this flag results in QnMaskedProxyWidget::paint never called. */
        m_view->setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing);
#endif

        /* We don't need scrollbars. */
        m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        /* This may be needed by instruments. */
        m_view->setDragMode(QGraphicsView::NoDrag);
        m_view->viewport()->setAcceptDrops(true);

        /* Don't initialize it again. */
        m_view->setProperty(qn_viewInitializedPropertyName, true);
    }

    /* Configure bounding instrument. */
    initBoundingInstrument();

    /* Configure viewport animator. */
    m_viewportAnimator->setView(m_view);


    /* Set up curtain. */
    m_curtainItem = new QnCurtainItem();
    m_scene->addItem(m_curtainItem.data());
    setLayer(m_curtainItem.data(), Qn::BackLayer);
    m_curtainItem.data()->setColor(Qt::black);
    m_curtainAnimator->setCurtainItem(m_curtainItem.data());

    /* Set up grid. */
    m_gridItem = new QnGridItem();
    m_scene->addItem(m_gridItem.data());
    setLayer(m_gridItem.data(), Qn::BackLayer);
    opacityAnimator(m_gridItem.data())->setTimeLimit(300);
    m_gridItem.data()->setOpacity(0.0);
    m_gridItem.data()->setLineWidth(100.0);
    m_gridItem.data()->setMapper(workbench()->mapper());

	if (!(m_lightMode & Qn::LightModeNoLayoutBackground)) {
		m_gridBackgroundItem = new QnGridBackgroundItem(NULL, context());
		m_scene->addItem(gridBackgroundItem());
		setLayer(gridBackgroundItem(), Qn::EMappingLayer);
		gridBackgroundItem()->setOpacity(0.0);
		gridBackgroundItem()->setMapper(workbench()->mapper());
	}

    /* Set up background */ 
    if (!(m_lightMode & Qn::LightModeNoSceneBackground)) {
        /* Never set QObject* parent in the QScopedPointer-stored objects if not sure in the descruction order. */
        m_backgroundPainter = new QnGradientBackgroundPainter(qnSettings->background().animationPeriodSec, NULL, context());
        m_view->installLayerPainter(m_backgroundPainter.data(), QGraphicsScene::BackgroundLayer);
    }

    /* Connect to context. */
    connect(workbench(),            SIGNAL(itemChanged(Qn::ItemRole)),              this,                   SLOT(at_workbench_itemChanged(Qn::ItemRole)));
    connect(workbench(),            SIGNAL(currentLayoutAboutToBeChanged()),        this,                   SLOT(at_workbench_currentLayoutAboutToBeChanged()));
    connect(workbench(),            SIGNAL(currentLayoutChanged()),                 this,                   SLOT(at_workbench_currentLayoutChanged()));

    /* Connect to grid mapper. */
    QnWorkbenchGridMapper *mapper = workbench()->mapper();
    connect(mapper,                 SIGNAL(originChanged()),                        this,                   SLOT(at_mapper_originChanged()));
    connect(mapper,                 SIGNAL(cellSizeChanged()),                      this,                   SLOT(at_mapper_cellSizeChanged()));
    connect(mapper,                 SIGNAL(spacingChanged()),                       this,                   SLOT(at_mapper_spacingChanged()));

    /* Run handlers. */
    at_workbench_currentLayoutChanged();
}

void QnWorkbenchDisplay::initBoundingInstrument() {
    assert(m_view != NULL);

    m_boundingInstrument->setSizeEnforced(m_view, true);
    m_boundingInstrument->setPositionEnforced(m_view, true);
    m_boundingInstrument->setScalingSpeed(m_view, 32.0);
    m_boundingInstrument->setMovementSpeed(m_view, 4.0);
}

QnGridItem *QnWorkbenchDisplay::gridItem() const {
    return m_gridItem.data();
}

QnCurtainItem* QnWorkbenchDisplay::curtainItem() const {
    return m_curtainItem.data();
} 

QnCurtainAnimator* QnWorkbenchDisplay::curtainAnimator() const {
    return m_curtainAnimator;
} 

QnGridBackgroundItem *QnWorkbenchDisplay::gridBackgroundItem() const {
    return m_gridBackgroundItem.data();
}


void QnWorkbenchDisplay::toggleBackgroundAnimation(bool enabled) {
    if (!m_scene || !m_view || !m_backgroundPainter)
        return;

    m_backgroundPainter->setEnabled(enabled);
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
    animator = new WidgetAnimator(widget, "geometry", "rotation", widget); // ANIMATION: items.
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
    if(role < 0 || role >= Qn::ItemRoleCount) {
        qnWarning("Invalid item role '%1'.", static_cast<int>(role));
        return NULL;
    }

    return m_widgetByRole[role];
}

QnResourceWidget *QnWorkbenchDisplay::widget(const QnUuid &uuid) const {
    return widget(workbench()->currentLayout()->item(uuid));
}

QnResourceWidget *QnWorkbenchDisplay::zoomTargetWidget(QnResourceWidget *widget) const {
    return m_zoomTargetWidgetByWidget.value(widget);
}

void QnWorkbenchDisplay::ensureRaisedConeItem(QnResourceWidget *widget) {
    QnGridRaisedConeItem* item = raisedConeItem(widget);
    if (item->scene() == m_scene)
        return;
    m_scene->addItem(item);
    setLayer(item, Qn::RaisedConeBgLayer);
    item->setOpacity(0.0);
}

void QnWorkbenchDisplay::setWidget(Qn::ItemRole role, QnResourceWidget *widget) {
    if(role < 0 || role >= Qn::ItemRoleCount) {
        qnWarning("Invalid item role '%1'.", static_cast<int>(role));
        return;
    }

    QnResourceWidget *oldWidget = m_widgetByRole[role];
    QnResourceWidget *newWidget = widget;
    if(oldWidget == newWidget)
        return;
    m_widgetByRole[role] = widget;

    switch(role) {
    case Qn::RaisedRole: {
        /* Sync new & old geometry. */
        if(oldWidget != NULL) {
            synchronize(oldWidget, true);

            if (!(m_lightMode & Qn::LightModeNoLayoutBackground)) {
                ensureRaisedConeItem(oldWidget);
                raisedConeItem(oldWidget)->setEffectEnabled(false);
                setLayer(raisedConeItem(oldWidget), Qn::RaisedConeBgLayer);
            }
        }

        if(newWidget != NULL) {
            bringToFront(newWidget);

            if (!(m_lightMode & Qn::LightModeNoLayoutBackground)) {
                ensureRaisedConeItem(newWidget);
                setLayer(raisedConeItem(newWidget), Qn::RaisedConeLayer);
                raisedConeItem(newWidget)->setEffectEnabled(!workbench()->currentLayout()->resource()->backgroundImageFilename().isEmpty());
            }

            synchronize(newWidget, true);
        }
        break;
    }
    case Qn::ZoomedRole: {
        m_zoomedToggle->setActive(newWidget != NULL);

        /* Sync new & old items. */
        if(oldWidget != NULL)
            synchronize(oldWidget, true);

        if(newWidget != NULL) {
            bringToFront(newWidget);
            synchronize(newWidget, true);

            m_viewportAnimator->moveTo(itemGeometry(newWidget->item()));
        } else {
            m_viewportAnimator->moveTo(fitInViewGeometry());
        }

        /* Sync scene geometry. */
        synchronizeSceneBounds();
        synchronizeSceneBoundsExtension();

        /* Un-raise on un-zoom. */
        if(newWidget == NULL)
            workbench()->setItem(Qn::RaisedRole, NULL);

        /* Update media quality. */
        if(QnMediaResourceWidget *oldMediaWidget = dynamic_cast<QnMediaResourceWidget *>(oldWidget)) {
            oldMediaWidget->display()->camDisplay()->setFullScreen(false);
        }
        if(QnMediaResourceWidget *newMediaWidget = dynamic_cast<QnMediaResourceWidget *>(newWidget)) {
            newMediaWidget->display()->camDisplay()->setFullScreen(true);
            if (newMediaWidget->display()->archiveReader()) {
                newMediaWidget->display()->archiveReader()->setQuality(MEDIA_Quality_High, true);
            }
        }

        /* Hide / show other items when zoomed. */
        if(newWidget)
            opacityAnimator(newWidget)->animateTo(1.0);
        qreal opacity = newWidget ? 0.0 : 1.0;
        foreach(QnResourceWidget *widget, m_widgets)
            if(widget != newWidget)
                opacityAnimator(widget)->animateTo(opacity);

        /* Update margin flags. */
        updateCurrentMarginFlags();

        /* Update curtain. */
        if(m_curtainAnimator->isCurtained())
            updateCurtainedCursor();

        break;
    }
    case Qn::ActiveRole: {
        if(oldWidget)
            oldWidget->setLocalActive(false);
        if(newWidget)
            newWidget->setLocalActive(true);
        m_frameWidthsDirty = true;
        break;
    }
    case Qn::CentralRole: {
        /* Update audio playback. */
        if(QnMediaResourceWidget *oldMediaWidget = dynamic_cast<QnMediaResourceWidget *>(oldWidget)) {
            QnCamDisplay *oldCamDisplay = oldMediaWidget ? oldMediaWidget->display()->camDisplay() : NULL;
            if(oldCamDisplay)
                oldCamDisplay->playAudio(false);
        }

        if(QnMediaResourceWidget *newMediaWidget = dynamic_cast<QnMediaResourceWidget *>(newWidget)) {
            QnCamDisplay *newCamDisplay = newMediaWidget ? newMediaWidget->display()->camDisplay() : NULL;
            if(newCamDisplay)
                newCamDisplay->playAudio(true);
        }

        break;
    }
    default:
        break;
    }

    emit widgetChanged(role);
}

void QnWorkbenchDisplay::updateBackground(const QnLayoutResourcePtr &layout) {
    if (!layout)
        return;

    if (m_lightMode & Qn::LightModeNoLayoutBackground)
        return;

	if (gridBackgroundItem())
		gridBackgroundItem()->update(layout);

    synchronizeSceneBounds();
    fitInView();

    QnResourceWidget* raisedWidget = m_widgetByRole[Qn::RaisedRole];
    if (raisedWidget) {
        ensureRaisedConeItem(raisedWidget);
        raisedConeItem(raisedWidget)->setEffectEnabled(!layout->backgroundImageFilename().isEmpty());
    }
}

void QnWorkbenchDisplay::updateSelectionFromTree() {
    QnActionTargetProvider *provider = menu()->targetProvider();
    if(!provider)
        return;

    Qn::ActionScope scope = provider->currentScope();
    if (scope != Qn::TreeScope)
        return; 

    /* Just deselect all items for now. See #4480. */
    foreach (QGraphicsItem *item, scene()->selectedItems())
        item->setSelected(false);
}


QList<QnResourceWidget *> QnWorkbenchDisplay::widgets() const {
    return m_widgets;
}

QnResourceWidget* QnWorkbenchDisplay::activeWidget() const {
    foreach (QnResourceWidget * widget, m_widgets) {
        if (widget->isLocalActive())
            return widget;
    }
    return NULL;
}

QList<QnResourceWidget *> QnWorkbenchDisplay::widgets(const QnResourcePtr &resource) const {
    return m_widgetsByResource.value(resource);
}

QnResourceDisplay *QnWorkbenchDisplay::display(QnWorkbenchItem *item) const {
    if(QnMediaResourceWidget *widget = dynamic_cast<QnMediaResourceWidget *>(this->widget(item)))
        return widget->display().data();
    return NULL;
}

QnClientVideoCamera *QnWorkbenchDisplay::camera(QnWorkbenchItem *item) const {
    QnResourceDisplay *display = this->display(item);
    if(display == NULL)
        return NULL;

    return display->camera();
}

QnCamDisplay *QnWorkbenchDisplay::camDisplay(QnWorkbenchItem *item) const {
    QnClientVideoCamera *camera = this->camera(item);
    if(camera == NULL)
        return NULL;

    return camera->getCamDisplay();
}


// -------------------------------------------------------------------------- //
// QnWorkbenchDisplay :: mutators
// -------------------------------------------------------------------------- //
void QnWorkbenchDisplay::fitInView(bool animate) {
    QRectF targetGeometry;

    QnResourceWidget *zoomedWidget = m_widgetByRole[Qn::ZoomedRole];
    if(zoomedWidget != NULL) {
        targetGeometry = itemGeometry(zoomedWidget->item());
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

bool QnWorkbenchDisplay::addItemInternal(QnWorkbenchItem *item, bool animate, bool startDisplay) {
    int maxItems = (m_lightMode & Qn::LightModeSingleItem)
            ? 1
            : qnSettings->maxSceneVideoItems();

    if (m_widgets.size() >= maxItems) {
        qnDeleteLater(item);
        return false;
    }

    QnResourcePtr resource = qnResPool->getResourceByUniqId(item->resourceUid());
    if(resource.isNull()) {
        qnDeleteLater(item);
        return false;
    }

    QnResourceWidget *widget;
    if (resource->hasFlags(Qn::server)) {
        widget = new QnServerResourceWidget(context(), item);
    }
    else
    if (resource->hasFlags(Qn::videowall)) {
        widget = new QnVideowallScreenWidget(context(), item);
    }
    else
    if (resource->hasFlags(Qn::media)) {
        widget = new QnMediaResourceWidget(context(), item);
    }
    else {
        // TODO: #Elric unsupported for now
        qnDeleteLater(item);
        return false;
    }

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

    m_scene->addItem(widget);

    connect(item, SIGNAL(geometryChanged()),                            this, SLOT(at_item_geometryChanged()));
    connect(item, SIGNAL(geometryDeltaChanged()),                       this, SLOT(at_item_geometryDeltaChanged()));
    connect(item, SIGNAL(rotationChanged()),                            this, SLOT(at_item_rotationChanged()));
    connect(item, SIGNAL(flagChanged(Qn::ItemFlag, bool)),              this, SLOT(at_item_flagChanged(Qn::ItemFlag, bool)));
    connect(item, SIGNAL(zoomRectChanged()),                            this, SLOT(at_item_zoomRectChanged()));
    connect(item, SIGNAL(dataChanged(int)),                             this, SLOT(at_item_dataChanged(int)));

    m_widgets.push_back(widget);
    m_widgetByItem.insert(item, widget);
    if(QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget))
        m_widgetByRenderer.insert(mediaWidget->renderer(), widget);

    /* Note that it is important to query resource from the widget as it may differ from the one passed
     * here because of enabled / disabled state effects. */
    QList<QnResourceWidget *> &widgetsForResource = m_widgetsByResource[widget->resource()];
    widgetsForResource.push_back(widget);
    if(widgetsForResource.size() == 1)
        emit resourceAdded(widget->resource());

    synchronize(widget, false);
    bringToFront(widget);

    if(item->hasFlag(Qn::PendingGeometryAdjustment))
        adjustGeometryLater(item, animate); /* Changing item flags here may confuse the callee, so we do it through the event loop. */

    connect(widget,                     SIGNAL(aboutToBeDestroyed()),   this,   SLOT(at_widget_aboutToBeDestroyed()));

    QColor frameColor = item->data(Qn::ItemFrameDistinctionColorRole).value<QColor>();
    if(frameColor.isValid())
        widget->setFrameDistinctionColor(frameColor);

    QnResourceWidget::Options options = item->data(Qn::ItemWidgetOptions).value<QnResourceWidget::Options>();
    if (options)
        widget->setOptions(widget->options() | options);

    emit widgetAdded(widget);

    for(int i = 0; i < Qn::ItemRoleCount; i++)
        if(item == workbench()->item(static_cast<Qn::ItemRole>(i)))
            setWidget(static_cast<Qn::ItemRole>(i), widget);

    if(QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget)) {
        if(startDisplay)
            mediaWidget->display()->start();
        if(mediaWidget->display()->archiveReader()) {
            if(item->layout()->resource() && !item->layout()->resource()->getLocalRange().isEmpty())
                mediaWidget->display()->archiveReader()->setPlaybackRange(item->layout()->resource()->getLocalRange());

            if(startDisplay) {
                qint64 time = item->data(Qn::ItemTimeRole).toLongLong();
                if (time > 0 && time != DATETIME_NOW)
                    time *= 1000;
                if (time > 0) {
                    mediaWidget->display()->archiveReader()->jumpTo(time, time);
                } else {
                    if(m_widgets.size() == 1 && !mediaWidget->resource()->toResource()->hasFlags(Qn::live))
                        mediaWidget->display()->archiveReader()->jumpTo(0, 0);
                }
            }
        }
        qnRedAssController->registerConsumer(mediaWidget->display()->camDisplay());
    }

    return true;
}

bool QnWorkbenchDisplay::removeItemInternal(QnWorkbenchItem *item, bool destroyWidget, bool destroyItem) {
    disconnect(item, NULL, this, NULL);

    QnResourceWidget *widget = this->widget(item);
    if(widget == NULL) {
        assert(!destroyItem);
        return false; /* The widget wasn't created. */
    }

    /* Remove all linked zoom items. */
    removeZoomLinksInternal(item);

    disconnect(widget, NULL, this, NULL);

    for(int i = 0; i <= Qn::ItemRoleCount; i++)
        if(widget == m_widgetByRole[i])
            setWidget(static_cast<Qn::ItemRole>(i), NULL);

    emit widgetAboutToBeRemoved(widget);

    QList<QnResourceWidget *> &widgetsForResource = m_widgetsByResource[widget->resource()];
    if(widgetsForResource.size() == 1) {
        emit resourceAboutToBeRemoved(widget->resource());
        m_widgetsByResource.remove(widget->resource());
    } else {
        widgetsForResource.removeOne(widget);
    }

    m_widgets.removeOne(widget);
    m_widgetByItem.remove(item);
    if(QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget)) {
        m_widgetByRenderer.remove(mediaWidget->renderer());
        qnRedAssController->unregisterConsumer(mediaWidget->display()->camDisplay());
    }

    if(destroyWidget) {
        widget->hide();
        qnDeleteLater(widget);
    }

    if(destroyItem) {
        if(item->layout())
            item->layout()->removeItem(item);
        qnDeleteLater(item);
    }

    return true;
}

bool QnWorkbenchDisplay::addZoomLinkInternal(QnWorkbenchItem *item, QnWorkbenchItem *zoomTargetItem) {
    return addZoomLinkInternal(widget(item), widget(zoomTargetItem));
}

bool QnWorkbenchDisplay::removeZoomLinkInternal(QnWorkbenchItem *item, QnWorkbenchItem *zoomTargetItem) {
    return removeZoomLinkInternal(widget(item), widget(zoomTargetItem));
}

bool QnWorkbenchDisplay::addZoomLinkInternal(QnResourceWidget *widget, QnResourceWidget *zoomTargetWidget) {
    if(widget && zoomTargetWidget) {
        if(QnResourceWidget *oldZoomTargetWidget = m_zoomTargetWidgetByWidget.value(widget)) {
            if(oldZoomTargetWidget == zoomTargetWidget)
                return false;
            removeZoomLinkInternal(widget, oldZoomTargetWidget);
        }

        m_zoomTargetWidgetByWidget.insert(widget, zoomTargetWidget);
        emit zoomLinkAdded(widget, zoomTargetWidget);
        emit widget->zoomTargetWidgetChanged();
        return true;
    } else {
        return false;
    }
}

bool QnWorkbenchDisplay::removeZoomLinkInternal(QnResourceWidget *widget, QnResourceWidget *zoomTargetWidget) {
    if(widget && zoomTargetWidget) {
        if(m_zoomTargetWidgetByWidget.contains(widget)) {
            emit zoomLinkAboutToBeRemoved(widget, zoomTargetWidget);
            m_zoomTargetWidgetByWidget.remove(widget);
            emit widget->zoomTargetWidgetChanged();
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

bool QnWorkbenchDisplay::removeZoomLinksInternal(QnWorkbenchItem *item) {
    QnResourceWidget *widget = this->widget(item);
    if(!widget)
        return false;

    removeZoomLinkInternal(widget, m_zoomTargetWidgetByWidget.value(widget));
    foreach(QnResourceWidget *zoomWidget, m_zoomTargetWidgetByWidget.keys(widget))
        removeZoomLinkInternal(zoomWidget, widget);
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
    if(m_widgetByRole[Qn::ZoomedRole] == NULL) {
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

    foreach(QnResourceWidget *widget, m_widgets)
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

Qn::ItemLayer QnWorkbenchDisplay::synchronizedLayer(QnResourceWidget *widget) const {
    assert(widget != NULL);

    if(widget == m_widgetByRole[Qn::ZoomedRole]) {
        return Qn::ZoomedLayer;
    } else if(widget->item()->isPinned()) {
        if(widget == m_widgetByRole[Qn::RaisedRole]) {
            return Qn::PinnedRaisedLayer;
        } else {
            return Qn::PinnedLayer;
        }
    } else {
        if(widget == m_widgetByRole[Qn::RaisedRole]) {
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

    QRectF geometry = rotated(widget->calculateGeometry(itemEnclosingGeometry(item)), item->rotation());
    return geometry;
}

QRectF QnWorkbenchDisplay::layoutBoundingGeometry() const {
    return fitInViewGeometry();
}

QRectF QnWorkbenchDisplay::fitInViewGeometry() const {
    QRect layoutBoundingRect = workbench()->currentLayout()->boundingRect();
    if(layoutBoundingRect.isNull())
        layoutBoundingRect = QRect(0, 0, 1, 1);

    QRect backgroundBoundingRect = gridBackgroundItem() ? gridBackgroundItem()->sceneBoundingRect() : QRect();

    QRect sceneBoundingRect = (backgroundBoundingRect.isNull())
            ? layoutBoundingRect
            : layoutBoundingRect.united(backgroundBoundingRect);

    /* Do not add additional spacing in following cases: */
    bool noAdjust = qnSettings->isVideoWallMode()                           /*< Videowall client. */
        || !backgroundBoundingRect.isNull();                                /*< There is a layout background. */

    if (noAdjust)
        return workbench()->mapper()->mapFromGridF(QRectF(sceneBoundingRect));
    
    const qreal minAdjust = 0.015;
    return workbench()->mapper()->mapFromGridF(QRectF(sceneBoundingRect).adjusted(-minAdjust, -minAdjust, minAdjust, minAdjust));
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
    synchronizeZoomRect(widget);
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
    QnResourceWidget *zoomedWidget = m_widgetByRole[Qn::ZoomedRole];
    QnResourceWidget *raisedWidget = m_widgetByRole[Qn::RaisedRole];

    QRectF enclosingGeometry = itemEnclosingGeometry(item);

    /* Adjust for raise. */
    if(widget == raisedWidget && widget != zoomedWidget && m_view != NULL) {
        QRectF originGeometry = enclosingGeometry;
        if (widget->hasAspectRatio())
            originGeometry = expanded(widget->aspectRatio(), originGeometry, Qt::KeepAspectRatio);

        ensureRaisedConeItem(widget);
        raisedConeItem(widget)->setOriginGeometry(originGeometry);

        QRectF viewportGeometry = mapRectToScene(m_view, m_view->viewport()->rect());

        QSizeF newWidgetSize = enclosingGeometry.size() * focusExpansion;

        qreal magicConst = maxExpandedSize;
        if (qnSettings->isVideoWallMode())
            magicConst = 0.8;   //TODO: #Elric magic const
        else
        if (
            !(m_lightMode & Qn::LightModeNoLayoutBackground) &&
            (workbench()->currentLayout()->resource() && !workbench()->currentLayout()->resource()->backgroundImageFilename().isEmpty())
        ) 
            magicConst = 0.33;  //TODO: #Elric magic const
        QSizeF maxWidgetSize = viewportGeometry.size() * magicConst;

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
    if(widget == raisedWidget || widget == zoomedWidget)
        bringToFront(widget);

    /* Calculate rotation. */
    qreal rotation = item->rotation();
    if(item->data<bool>(Qn::ItemFlipRole, false))
        rotation += 180;

    /* Move! */
    WidgetAnimator *animator = this->animator(widget);
    if(animate) {
        widget->setEnclosingGeometry(enclosingGeometry, false);
        animator->moveTo(widget->calculateGeometry(enclosingGeometry, rotation), rotation);
    } else {
        animator->stop();
        widget->setRotation(rotation);
        widget->setEnclosingGeometry(enclosingGeometry);
    }
}

void QnWorkbenchDisplay::synchronizeZoomRect(QnWorkbenchItem *item) {
    QnResourceWidget *widget = this->widget(item);
    if(widget == NULL)
        return; /* No widget was created for the given item. */

    synchronizeZoomRect(widget);
}

void QnWorkbenchDisplay::synchronizeZoomRect(QnResourceWidget *widget) {
    if(QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget))
        mediaWidget->setZoomRect(widget->item()->zoomRect());
}

void QnWorkbenchDisplay::synchronizeAllGeometries(bool animate) {
    foreach(QnResourceWidget *widget, m_widgetByItem)
        synchronizeGeometry(widget, animate);
}

void QnWorkbenchDisplay::synchronizeLayer(QnWorkbenchItem *item) {
    synchronizeLayer(widget(item));
}

void QnWorkbenchDisplay::synchronizeLayer(QnResourceWidget *widget) {
    setLayer(widget, synchronizedLayer(widget));
}

void QnWorkbenchDisplay::synchronizeSceneBounds() {
    if(m_instrumentManager->scene() == NULL)
        return; /* Do nothing if scene is being destroyed. */

    QRectF sizeRect, moveRect;

    QnWorkbenchItem *zoomedItem = m_widgetByRole[Qn::ZoomedRole] ? m_widgetByRole[Qn::ZoomedRole]->item() : NULL;
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

    /* If an item is zoomed then the margins should be null because all panels are hidden. */
    if(currentMarginFlags() != 0 && !m_widgetByRole[Qn::ZoomedRole])
        marginsExtension = cwiseDiv(m_viewportAnimator->viewportMargins(), m_view->viewport()->size());

    /* Sync position extension. */
    {
        MarginsF positionExtension(0.0, 0.0, 0.0, 0.0);

        if(currentMarginFlags() & Qn::MarginsAffectPosition)
            positionExtension = marginsExtension;

        if(m_widgetByRole[Qn::ZoomedRole]) {
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
        if(!m_widgetByRole[Qn::ZoomedRole])
            m_boundingInstrument->stickScale(m_view);
    }
}

void QnWorkbenchDisplay::synchronizeRaisedGeometry() {
    QnResourceWidget *raisedWidget = m_widgetByRole[Qn::RaisedRole];
    QnResourceWidget *zoomedWidget = m_widgetByRole[Qn::ZoomedRole];
    if(!raisedWidget || raisedWidget == zoomedWidget)
        return;

    synchronizeGeometry(raisedWidget, animator(raisedWidget)->isRunning());
}

void QnWorkbenchDisplay::adjustGeometryLater(QnWorkbenchItem *item, bool animate) {
    if(!item->hasFlag(Qn::PendingGeometryAdjustment))
        return;

    QnResourceWidget *widget = this->widget(item);
    if(widget == NULL) {
        return;
    } else {
        widget->hide(); /* So that it won't appear where it shouldn't. */
    }

    emit geometryAdjustmentRequested(item, animate);
}

void QnWorkbenchDisplay::adjustGeometry(QnWorkbenchItem *item, bool animate) {
    if(!item->hasFlag(Qn::PendingGeometryAdjustment))
        return;

    QnResourceWidget *widget = this->widget(item);
    if(widget == NULL) {
        return; /* No widget was created for the given item. */
    } else {
        widget->show(); /* It may have been hidden in a call to adjustGeometryLater. */
    }

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
    QnConstResourceVideoLayoutPtr videoLayout = widget->channelLayout();
    qreal estimatedAspectRatio = aspectRatio(videoLayout->size()) * (item->zoomRect().isNull() ? 1.0 : aspectRatio(item->zoomRect())) * (4.0 / 3.0);
    if (QnAspectRatio::isRotated90(item->rotation()))
        estimatedAspectRatio = 1 / estimatedAspectRatio;
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

    foreach(QnResourceWidget *widget, this->widgets())
        widget->setFrameWidth(widget->isSelected() || widget->isLocalActive() ? selectedFrameWidth : defaultFrameWidth);
}

void QnWorkbenchDisplay::updateCurtainedCursor() {
#ifdef Q_OS_MACX
    if(m_view != NULL)
        m_view->viewport()->setCursor(QCursor(Qt::ArrowCursor));
#else
    bool curtained = m_curtainAnimator->isCurtained();
    if(m_view != NULL)
        m_view->viewport()->setCursor(QCursor(curtained ? Qt::BlankCursor : Qt::ArrowCursor));
#endif
}


// -------------------------------------------------------------------------- //
// QnWorkbenchDisplay :: handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchDisplay::at_viewportAnimator_finished() {
    synchronizeSceneBounds();
}

void QnWorkbenchDisplay::at_layout_itemAdded(QnWorkbenchItem *item) {
    if(addItemInternal(item, true)) {
        synchronizeSceneBounds();
        fitInView();

        workbench()->setItem(Qn::ZoomedRole, NULL); /* Unzoom & fit in view on item addition. */
    }
}

void QnWorkbenchDisplay::at_layout_itemRemoved(QnWorkbenchItem *item) {
    if(removeItemInternal(item, true, false))
        synchronizeSceneBounds();
}

void QnWorkbenchDisplay::at_layout_zoomLinkAdded(QnWorkbenchItem *item, QnWorkbenchItem *zoomTargetItem) {
    addZoomLinkInternal(item, zoomTargetItem);
}

void QnWorkbenchDisplay::at_layout_zoomLinkRemoved(QnWorkbenchItem *item, QnWorkbenchItem *zoomTargetItem) {
    removeZoomLinkInternal(item, zoomTargetItem);
}

void QnWorkbenchDisplay::at_layout_boundingRectChanged(const QRect &oldRect, const QRect &newRect) {
    QRect backgroundBoundingRect = gridBackgroundItem() ? gridBackgroundItem()->sceneBoundingRect() : QRect();

    QRect oldBoundingRect = (backgroundBoundingRect.isNull())
            ? oldRect
            : oldRect.united(backgroundBoundingRect);
    QRect newBoundingRect = (backgroundBoundingRect.isNull())
            ? newRect
            : newRect.united(backgroundBoundingRect);
    if (oldBoundingRect != newBoundingRect)
        fitInView();
}

void QnWorkbenchDisplay::at_workbench_itemChanged(Qn::ItemRole role, QnWorkbenchItem *item) {
    setWidget(role, widget(item));
}

void QnWorkbenchDisplay::at_workbench_itemChanged(Qn::ItemRole role) {
    at_workbench_itemChanged(role, workbench()->item(role));
}

void QnWorkbenchDisplay::at_workbench_currentLayoutAboutToBeChanged() {
    if (m_inChangeLayout)
        return;

    m_inChangeLayout = true;
    QnWorkbenchLayout *layout = workbench()->currentLayout();

    disconnect(layout, NULL, this, NULL);
    if (layout->resource())
        disconnect(layout->resource(), NULL, this, NULL);

    QnWorkbenchStreamSynchronizer *streamSynchronizer = context()->instance<QnWorkbenchStreamSynchronizer>();
    layout->setData(Qn::LayoutSyncStateRole, QVariant::fromValue<QnStreamSynchronizationState>(streamSynchronizer->state()));

    QVector<QnUuid> selectedUuids;
    foreach(QnResourceWidget *widget, widgets())
        if(widget->isSelected())
            selectedUuids.push_back(widget->item()->uuid());
    layout->setData(Qn::LayoutSelectionRole, QVariant::fromValue<QVector<QnUuid> >(selectedUuids));

    foreach(QnResourceWidget *widget, widgets()) {
        if(QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget)) {
            qint64 timeUSec = mediaWidget->display()->camera()->getCurrentTime();
            if((quint64)timeUSec != AV_NOPTS_VALUE)
                mediaWidget->item()->setData(Qn::ItemTimeRole, mediaWidget->display()->camDisplay()->isRealTimeSource() ? DATETIME_NOW : timeUSec / 1000);

            mediaWidget->item()->setData(Qn::ItemPausedRole, mediaWidget->display()->isPaused());
        }
    }

    foreach(QnWorkbenchItem *item, layout->items())
        at_layout_itemRemoved(item);
    if(gridBackgroundItem())
        gridBackgroundItem()->setOpacity(0.0);

    m_inChangeLayout = false;
}


void QnWorkbenchDisplay::at_workbench_currentLayoutChanged() {
    QnWorkbenchLayout *layout = workbench()->currentLayout();

    QnThumbnailsSearchState searchState = layout->data(Qn::LayoutSearchStateRole).value<QnThumbnailsSearchState>();
    bool thumbnailed = searchState.step > 0 && !layout->items().empty();

    if(thumbnailed) {
        if(m_loader) {
            disconnect(m_loader, NULL, this, NULL);
            m_loader->pleaseStop();
        }

        if(QnResourcePtr resource = resourcePool()->getResourceByUniqId((**layout->items().begin()).resourceUid())) {
            m_loader = new QnThumbnailsLoader(resource, false);

            connect(m_loader, SIGNAL(thumbnailLoaded(const QnThumbnail &)), this, SLOT(at_loader_thumbnailLoaded(const QnThumbnail &)));
            connect(m_loader, SIGNAL(finished()), m_loader, SLOT(deleteLater()));

            m_loader->setTimePeriod(searchState.period);
            m_loader->setTimeStep(searchState.step);
            m_loader->start();
        }
    }

    QnWorkbenchStreamSynchronizer *streamSynchronizer = context()->instance<QnWorkbenchStreamSynchronizer>();
    streamSynchronizer->setState(layout->data(Qn::LayoutSyncStateRole).value<QnStreamSynchronizationState>());

    foreach(QnWorkbenchItem *item, layout->items())
        addItemInternal(item, false, !thumbnailed);
    foreach(QnWorkbenchItem *item, layout->items())
        addZoomLinkInternal(item, item->zoomTargetItem());

    bool hasTimeLabels = layout->data(Qn::LayoutTimeLabelsRole).toBool();

    QList<QnResourceWidget *> widgets = this->widgets();
    if(thumbnailed)
        qSort(widgets.begin(), widgets.end(), WidgetPositionLess());

    for(int i = 0; i < widgets.size(); i++) {
        QnResourceWidget *resourceWidget = widgets[i];

        int checkedButtons = resourceWidget->item()->data<int>(Qn::ItemCheckedButtonsRole, -1);
        if(checkedButtons != -1)
            resourceWidget->setCheckedButtons(static_cast<QnResourceWidget::Buttons>(checkedButtons));

        QnMediaResourceWidget *widget = dynamic_cast<QnMediaResourceWidget *>(widgets[i]);
        if(!widget)
            continue;

        qint64 time = widget->item()->data<qint64>(Qn::ItemTimeRole, -1);

        if(!thumbnailed) {
            QnResourcePtr resource = widget->resource()->toResourcePtr();
            if(time > 0) {
                qint64 timeUSec = time == DATETIME_NOW ? DATETIME_NOW : time * 1000;
                if(widget->display()->archiveReader())
                    widget->display()->archiveReader()->jumpTo(timeUSec, timeUSec);
            } else if (!resource->hasFlags(Qn::live)) {
                // default position in SyncPlay is LIVE. If current resource is synchronized and it is not camera (does not has live) seek to 0 (default position)
                if(widget->display()->archiveReader())
                    widget->display()->archiveReader()->jumpTo(0, 0);
            }
        }

        bool paused = widget->item()->data<bool>(Qn::ItemPausedRole, false);
        if(paused) {
            if(widget->display()->archiveReader()) {
                widget->display()->archiveReader()->pauseMedia();
                widget->display()->archiveReader()->setSpeed(0.0); // TODO: #vasilenko check that this call doesn't break anything
            }
        }

        if(hasTimeLabels) {
            widget->setOverlayVisible(true, false);
            widget->setInfoVisible(true, false);

            qint64 displayTime = time;
            if(qnSettings->timeMode() == Qn::ServerTimeMode)
                displayTime += context()->instance<QnWorkbenchServerTimeWatcher>()->localOffset(widget->resource(), 0); // TODO: #Elric do offset adjustments in one place

            // TODO: #Elric move out, common code, another copy is in QnWorkbenchScreenshotHandler
            QString timeString = (widget->resource()->toResource()->flags() & Qn::utc) ? QDateTime::fromMSecsSinceEpoch(displayTime).toString(lit("yyyy MMM dd hh:mm:ss")) : QTime().addMSecs(displayTime).toString(lit("hh:mm:ss"));
            widget->setTitleTextFormat(QLatin1String("%1\t") + timeString);
        }

        if(thumbnailed)
            widget->item()->setData(Qn::ItemDisabledButtonsRole, static_cast<int>(QnMediaResourceWidget::PtzButton));
    }

    QVector<QnUuid> selectedUuids = layout->data(Qn::LayoutSelectionRole).value<QVector<QnUuid> >();
    foreach(const QnUuid &selectedUuid, selectedUuids)
        if(QnResourceWidget *widget = this->widget(selectedUuid))
            widget->setSelected(true);

    connect(layout,             SIGNAL(itemAdded(QnWorkbenchItem *)),           this,                   SLOT(at_layout_itemAdded(QnWorkbenchItem *)));
    connect(layout,             SIGNAL(itemRemoved(QnWorkbenchItem *)),         this,                   SLOT(at_layout_itemRemoved(QnWorkbenchItem *)));
    connect(layout,             SIGNAL(zoomLinkAdded(QnWorkbenchItem *, QnWorkbenchItem *)), this,      SLOT(at_layout_zoomLinkAdded(QnWorkbenchItem *, QnWorkbenchItem *)));
    connect(layout,             SIGNAL(zoomLinkRemoved(QnWorkbenchItem *, QnWorkbenchItem *)), this,    SLOT(at_layout_zoomLinkRemoved(QnWorkbenchItem *, QnWorkbenchItem *)));
    connect(layout,             SIGNAL(boundingRectChanged(QRect, QRect)),      this,                   SLOT(at_layout_boundingRectChanged(QRect, QRect)));
    if (layout->resource()) {
        connect(layout->resource(), SIGNAL(backgroundImageChanged(const QnLayoutResourcePtr &)), this, SLOT(updateBackground(const QnLayoutResourcePtr &)));
        connect(layout->resource(), SIGNAL(backgroundSizeChanged(const QnLayoutResourcePtr &)), this, SLOT(updateBackground(const QnLayoutResourcePtr &)));
        connect(layout->resource(), SIGNAL(backgroundOpacityChanged(const QnLayoutResourcePtr &)), this, SLOT(updateBackground(const QnLayoutResourcePtr &)));
    }
    updateBackground(layout->resource());
    synchronizeSceneBounds();
    fitInView(false);
}

void QnWorkbenchDisplay::at_loader_thumbnailLoaded(const QnThumbnail &thumbnail) {
    QnThumbnailsSearchState searchState = workbench()->currentLayout()->data(Qn::LayoutSearchStateRole).value<QnThumbnailsSearchState>();
    if(searchState.step <= 0)
        return;
  
    int index = (thumbnail.time() - searchState.period.startTimeMs) / searchState.step;
    QList<QnResourceWidget *> widgets = this->widgets();
    if(index < 0)
        return;

    qSort(widgets.begin(), widgets.end(), WidgetPositionLess());

    if(index < widgets.size()) {

        // when we have received thumbnail for an item, check if it can be used for the previous item
        for (int checkedIdx = qMax(index - 1, 0); checkedIdx <= index; checkedIdx++) {
            if(QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widgets[checkedIdx])) {
                qint64 time = mediaWidget->item()->data<qint64>(Qn::ItemTimeRole, -1);

                if (time > 0 && qAbs(time - thumbnail.actualTime()) > searchState.step / 2)
                    continue;

                qint64 existingThumbnailTime = mediaWidget->item()->data<qint64>(Qn::ItemThumbnailTimestampRole, 0);
                if (qAbs(time - existingThumbnailTime) < qAbs(time - thumbnail.actualTime()))   // if value not present automatically advance =)
                    continue;

                mediaWidget->item()->setData(Qn::ItemThumbnailTimestampRole, thumbnail.actualTime());

                mediaWidget->display()->archiveReader()->jumpTo(thumbnail.actualTime() * 1000, 0);
                mediaWidget->display()->camDisplay()->setMTDecoding(false);
                mediaWidget->display()->camDisplay()->putData(thumbnail.data());
                mediaWidget->display()->camDisplay()->start();
                mediaWidget->display()->archiveReader()->startPaused();
            }
        }
    }

    if(index >= widgets.size() - 1) {
        int i = 0;
        foreach(QnResourceWidget *widget, widgets) {
            if(QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget)) {
                if(!mediaWidget->display()->camDisplay()->isRunning()) {
                    mediaWidget->display()->archiveReader()->jumpTo((searchState.period.startTimeMs + searchState.step * i) * 1000, 0);
                    mediaWidget->display()->camDisplay()->setMTDecoding(false);
                    mediaWidget->display()->camDisplay()->start();
                    mediaWidget->display()->archiveReader()->startPaused();
                }
            }
            i++;
        }
        return;
    }
}

void QnWorkbenchDisplay::at_item_dataChanged(int role) {
    if(role == Qn::ItemFlipRole)
        synchronizeGeometry(static_cast<QnWorkbenchItem *>(sender()), false);
}

void QnWorkbenchDisplay::at_item_geometryChanged() {
    synchronizeGeometry(static_cast<QnWorkbenchItem *>(sender()), true);
    synchronizeSceneBounds();
}

void QnWorkbenchDisplay::at_item_geometryDeltaChanged() {
    synchronizeGeometry(static_cast<QnWorkbenchItem *>(sender()), true);
}

void QnWorkbenchDisplay::at_item_zoomRectChanged() {
    synchronizeZoomRect(static_cast<QnWorkbenchItem *>(sender()));
}

void QnWorkbenchDisplay::at_item_rotationChanged() {
    QnWorkbenchItem *item = static_cast<QnWorkbenchItem *>(sender());

    synchronizeGeometry(item, true);
    if(m_widgetByRole[Qn::ZoomedRole] && m_widgetByRole[Qn::ZoomedRole]->item() == item)
        synchronizeSceneBounds();
}

void QnWorkbenchDisplay::at_item_flagChanged(Qn::ItemFlag flag, bool value) {
    switch(flag) {
    case Qn::Pinned:
        synchronizeLayer(static_cast<QnWorkbenchItem *>(sender()));
        break;
    case Qn::PendingGeometryAdjustment:
        if(value)
            adjustGeometryLater(static_cast<QnWorkbenchItem *>(sender())); /* Changing item flags here may confuse the callee, so we do it through the event loop. */
        break;
    default:
        qnWarning("Invalid item flag '%1'.", static_cast<int>(flag));
        break;
    }
}

void QnWorkbenchDisplay::at_curtainActivityInstrument_activityStopped() {
    m_curtainAnimator->curtain(m_widgetByRole[Qn::ZoomedRole]);
}

void QnWorkbenchDisplay::at_curtainActivityInstrument_activityStarted() {
    m_curtainAnimator->uncurtain();
}

void QnWorkbenchDisplay::at_widgetActivityInstrument_activityStopped() {
    foreach(QnResourceWidget *widget, m_widgets)
        widget->setOption(QnResourceWidget::DisplayActivity, true);
}

void QnWorkbenchDisplay::at_widgetActivityInstrument_activityStarted() {
    foreach(QnResourceWidget *widget, m_widgets)
        widget->setOption(QnResourceWidget::DisplayActivity, false);
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
    if(m_instrumentManager->scene() == NULL)
        return; /* Do nothing if scene is being destroyed. */

    m_frameWidthsDirty = true;

    /* Update single selected item. */
    QList<QGraphicsItem *> selection = m_scene->selectedItems();
    if(selection.size() == 1) {
        QGraphicsItem *item = selection.front();
        QnResourceWidget *widget = item->isWidget() ? qobject_cast<QnResourceWidget *>(item->toGraphicsObject()) : NULL;

        workbench()->setItem(Qn::SingleSelectedRole, widget ? widget->item() : NULL);
    } else {
        workbench()->setItem(Qn::SingleSelectedRole, NULL);
    }
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

void QnWorkbenchDisplay::at_notificationsHandler_businessActionAdded(const QnAbstractBusinessActionPtr &businessAction) {
    if (m_lightMode & Qn::LightModeNoNotifications)
        return;

    QnResourcePtr resource = qnResPool->getResourceById(businessAction->getRuntimeParams().getEventResourceId());
    if (!resource)
        return;

    // TODO: #Elric copypasta
    QnWorkbenchLayout *layout = workbench()->currentLayout();
    QnThumbnailsSearchState searchState = layout->data(Qn::LayoutSearchStateRole).value<QnThumbnailsSearchState>();
    bool thumbnailed = searchState.step > 0 && !layout->items().empty();
    if(thumbnailed)
        return;

    int type = businessAction->getRuntimeParams().getEventType();

    at_notificationTimer_timeout(resource, type);
    QnVariantTimer::singleShot(500, this, SLOT(at_notificationTimer_timeout(const QVariant &, const QVariant &)), QVariant::fromValue<QnResourcePtr>(resource), type);
    QnVariantTimer::singleShot(1000, this, SLOT(at_notificationTimer_timeout(const QVariant &, const QVariant &)), QVariant::fromValue<QnResourcePtr>(resource), type);
}

void QnWorkbenchDisplay::at_notificationTimer_timeout(const QVariant &resource, const QVariant &type) {
    at_notificationTimer_timeout(resource.value<QnResourcePtr>(), type.toInt());
}

void QnWorkbenchDisplay::at_notificationTimer_timeout(const QnResourcePtr &resource, int type) {
    if (m_lightMode & Qn::LightModeNoNotifications)
        return;

    foreach(QnResourceWidget *widget, this->widgets(resource)) {
        if(widget->zoomTargetWidget())
            continue; /* Don't draw notification on zoom widgets. */

        QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget);
        if(mediaWidget && !mediaWidget->display()->camDisplay()->isRealTimeSource())
            continue;

        QRectF rect = widget->rect();
        qreal expansion = qMin(rect.width(), rect.height()) / 2.0;

        QnSplashItem *splashItem = new QnSplashItem();
        splashItem->setSplashType(QnSplashItem::Rectangular);
        splashItem->setPos(rect.center() + widget->pos());
        splashItem->setRect(QRectF(-toPoint(rect.size()) / 2, rect.size()));
        splashItem->setColor(withAlpha(QnNotificationLevels::notificationColor(static_cast<QnBusiness::EventType>(type)), 128));
        splashItem->setOpacity(0.0);
        splashItem->setRotation(widget->rotation());
        splashItem->animate(1000, QnGeometry::dilated(splashItem->rect(), expansion), 0.0, true, 200, 1.0);
        scene()->addItem(splashItem);
        setLayer(splashItem, Qn::EffectsLayer);
    }
}
