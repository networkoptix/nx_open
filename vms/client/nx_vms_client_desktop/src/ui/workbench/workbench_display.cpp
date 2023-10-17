// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_display.h"

#include <cmath> //< For std::fmod.

#include <QtCore/QScopedValueRollback>
#include <QtCore/QTimer>
#include <QtCore/QtAlgorithms>
#include <QtGui/QAction>
#include <QtGui/QScreen>
#include <QtGui/QWindow>
#include <QtOpenGL/private/qopengltexturecache_p.h>
#include <QtOpenGLWidgets/QOpenGLWidget>
#include <QtWidgets/QGraphicsProxyWidget>

#include <camera/client_video_camera.h>
#include <camera/resource_display.h>
#include <client/client_meta_types.h>
#include <client/client_runtime_settings.h>
#include <common/common_meta_types.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/streaming/abstract_archive_stream_reader.h>
#include <nx/streaming/abstract_stream_data_provider.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/log_main.h>
#include <nx/utils/math/math.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/camera/storage_location_camera_controller.h>
#include <nx/vms/client/desktop/common/utils/audio_dispatcher.h>
#include <nx/vms/client/desktop/common/widgets/webview_controller.h>
#include <nx/vms/client/desktop/debug_utils/instruments/frame_time_points_provider_instrument.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/integrations/integrations.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_target_provider.h>
#include <nx/vms/client/desktop/radass/radass_controller.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/handlers/notification_action_executor.h>
#include <nx/vms/client/desktop/workbench/resource/resource_widget_factory.h>
#include <nx/vms/client/desktop/workbench/state/thumbnail_search_state.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/client/desktop/workbench/workbench_animations.h>
#include <nx/vms/rules/actions/notification_action_base.h>
#include <nx/vms/time/formatter.h>
#include <ui/animation/curtain_animator.h>
#include <ui/animation/opacity_animator.h>
#include <ui/animation/viewport_animator.h>
#include <ui/animation/widget_animator.h>
#include <ui/common/notification_levels.h>
#include <ui/graphics/instruments/bounding_instrument.h>
#include <ui/graphics/instruments/focus_listener_instrument.h>
#include <ui/graphics/instruments/forwarding_instrument.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/instruments/selection_overlay_tune_instrument.h>
#include <ui/graphics/instruments/signaling_instrument.h>
#include <ui/graphics/instruments/stop_instrument.h>
#include <ui/graphics/instruments/transform_listener_instrument.h>
#include <ui/graphics/instruments/widget_layout_instrument.h>
#include <ui/graphics/items/generic/splash_item.h>
#include <ui/graphics/items/grid/curtain_item.h>
#include <ui/graphics/items/grid/grid_background_item.h>
#include <ui/graphics/items/grid/grid_item.h>
#include <ui/graphics/items/resource/button_ids.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/web_resource_widget.h>
#include <ui/graphics/items/standard/graphics_web_view.h>
#include <ui/graphics/view/graphics_view.h>
#include <utils/common/aspect_ratio.h>
#include <utils/common/checked_cast.h>
#include <utils/common/delayed.h>
#include <utils/common/delete_later.h>
#include <utils/common/util.h>
#include <utils/math/color_transformations.h>

#include "extensions/workbench_stream_synchronizer.h"
#include "workbench_context.h"
#include "workbench_grid_mapper.h"
#include "workbench_item.h"
#include "workbench_layout.h"
#include "workbench_utility.h"

using namespace nx;
using namespace nx::vms::client::desktop;
using namespace ui;
using nx::vms::client::core::Geometry;
using nx::vms::client::core::MotionSelection;

namespace {

using namespace std::chrono;
/**
 * Show paused timeline notification in fullscreen mode after this inactivity period.
 */
static constexpr milliseconds kActivityTimeout = 30s;

QnWorkbenchItem* getWorkbenchItem(QGraphicsItem* item)
{
    const auto resourceWidget = item->isWidget()
        ? qobject_cast<QnResourceWidget*>(item->toGraphicsObject())
        : nullptr;

    return resourceWidget
        ? resourceWidget->item()
        : nullptr;
}

void calculateExpansionValues(qreal start, qreal end, qreal center, qreal newLength, qreal *deltaStart, qreal *deltaEnd)
{
    qreal newStart = center - newLength / 2;
    qreal newEnd = center + newLength / 2;

    if (newStart > start)
    {
        newEnd += start - newStart;
        newStart = start;
    }

    if (newEnd < end)
    {
        newStart += end - newEnd;
        newEnd = end;
    }

    *deltaStart = newStart - start;
    *deltaEnd = newEnd - end;
}

/** Size multiplier for raised widgets. */
const qreal focusExpansion = 100.0;

// Raised widget will take 50% of the viewport.
static constexpr qreal kRaisedWidgetViewportPercentage = 0.5;

// Raised widget in videowall mode will take 80% of the viewport.
static constexpr qreal kVideoWallRaisedWidgetViewportPercentage = 0.8;

// Raised widget in E-Mapping mode will take 33% of the viewport.
static constexpr qreal kEMappingRaisedWidgetViewportPercentage = 0.33;

/** The amount of z-space that one layer occupies. */
const qreal layerZSize = 10000000.0;

/**
 * The amount that is added to maximal Z value each time a move to frontoperation is performed.
 */
const qreal zStep = 1.0;

/** How often splashes should be painted on items when notification appears. */
const int splashPeriodMs = 500;

/** How long splashes should be painted on items when notification appears. */
const int splashTotalLengthMs = 1000;

/** Viewport lower size boundary, in scene coordinates. */
static const QSizeF kViewportLowerSizeBound(500.0, 500.0);

enum
{
    ITEM_LAYER_KEY = 0x93A7FA71,    /**< Key for item layer. */
    ITEM_ANIMATOR_KEY = 0x81AFD591  /**< Key for item animator. */
};

void setWidgetScreen(QWidget* target, QScreen* screen)
{
    if (!target)
        return;

    const auto window = target->window();
    if (!window)
        return;

    if (const auto handle = window->windowHandle())
        handle->setScreen(screen);
}

void setScreenRecursive(QWidget* target, QScreen* screen)
{
    if (!target)
        return;

    for (const auto& child : target->children())
    {
        if (const auto widget = qobject_cast<QWidget*>(child))
            setScreenRecursive(widget, screen);
    }

    setWidgetScreen(target, screen);
}

void setScreenRecursive(QGraphicsItem* target, QScreen* screen)
{
    if (!target)
        return;

    for (const auto& child : target->childItems())
        setScreenRecursive(child, screen);

    if (!target->isWidget())
        return;

    if (const auto proxy = dynamic_cast<QGraphicsProxyWidget*>(target))
        setScreenRecursive(proxy->widget(), screen);
};

} // namespace

using namespace nx::vms::client::desktop::ui::workbench;

QnWorkbenchDisplay::QnWorkbenchDisplay(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_scene(nullptr),
    m_view(nullptr),
    m_frontZ(0.0),
    m_inChangeLayout(false),
    m_instrumentManager(new InstrumentManager(this)),
    m_viewportAnimator(nullptr),
    m_curtainAnimator(nullptr),
    m_playbackPositionBlinkTimer(new QTimer(this))
{
    m_widgetByRole.fill(nullptr);

    AnimationTimer *animationTimer = m_instrumentManager->animationTimer();

    /* Create and configure instruments. */
    Instrument::EventTypeSet paintEventTypes = Instrument::makeSet(QEvent::Paint);

    SignalingInstrument *resizeSignalingInstrument = new SignalingInstrument(Instrument::Viewport, Instrument::makeSet(QEvent::Resize), this);
    m_beforePaintInstrument = new SignalingInstrument(Instrument::Viewport, paintEventTypes, this);
    m_afterPaintInstrument = new SignalingInstrument(Instrument::Viewport, paintEventTypes, this);
    m_boundingInstrument = new BoundingInstrument(this);
    m_transformListenerInstrument = new TransformListenerInstrument(this);
    m_focusListenerInstrument = new FocusListenerInstrument(this);
    m_paintForwardingInstrument = new ForwardingInstrument(Instrument::Viewport, paintEventTypes, this);
    m_selectionOverlayTuneInstrument = new SelectionOverlayTuneInstrument(this);
    m_frameTimePointsInstrument = new FrameTimePointsProviderInstrument(this);

    m_instrumentManager->installInstrument(new StopInstrument(Instrument::Viewport, paintEventTypes, this));
    m_instrumentManager->installInstrument(m_afterPaintInstrument);
    m_instrumentManager->installInstrument(m_paintForwardingInstrument);
    m_instrumentManager->installInstrument(new WidgetLayoutInstrument(this));
    m_instrumentManager->installInstrument(m_beforePaintInstrument);
    m_instrumentManager->installInstrument(m_transformListenerInstrument);
    m_instrumentManager->installInstrument(m_focusListenerInstrument);
    m_instrumentManager->installInstrument(resizeSignalingInstrument);
    m_instrumentManager->installInstrument(m_boundingInstrument);
    m_instrumentManager->installInstrument(m_selectionOverlayTuneInstrument);
    m_instrumentManager->installInstrument(
        m_frameTimePointsInstrument,
        InstallationMode::InstallBefore,
        paintForwardingInstrument());

    connect(m_transformListenerInstrument, SIGNAL(transformChanged(QGraphicsView *)), this, SLOT(synchronizeRaisedGeometry()));
    connect(resizeSignalingInstrument, SIGNAL(activated(QWidget *, QEvent *)), this, SLOT(synchronizeRaisedGeometry()));
    connect(resizeSignalingInstrument, SIGNAL(activated(QWidget *, QEvent *)), this, SLOT(synchronizeSceneBoundsExtension()));

    connect(resizeSignalingInstrument, QnSignalingInstrumentActivated, this,
        [this]()
        {
            fitInView(false); //< Direct call to immediate reaction
            /**
             * Since we don't animate fit in view we have to execute fitInView second time
             * after some delay, because some OS make resize call before move widget to correct
             * position while expanding it to fullscreen.
             */
            executeLater([this]() { fitInView(false); }, this);
        });

    /* We should disable all one-key shortcuts while web view is in the focus. */
    connect(m_focusListenerInstrument, &FocusListenerInstrument::focusItemChanged, this,
        [this]()
        {
            if (!NX_ASSERT(m_view) || !NX_ASSERT(m_scene))
                return;

            const bool isWebView = m_view->hasFocus()
                && dynamic_cast<GraphicsWebEngineView*>(m_scene->focusItem());

            for (auto actionId: {
                menu::NotificationsTabAction, //< N
                menu::ResourcesTabAction, //< C
                menu::MotionTabAction, //< M
                menu::BookmarksTabAction, //< B
                menu::EventsTabAction, //< E
                menu::ObjectsTabAction, //< O
                menu::JumpToLiveAction, //< L
                menu::ToggleMuteAction, //< U
                menu::ToggleSyncAction, //< S
                menu::JumpToEndAction,  //< X
                menu::JumpToStartAction,//< Z
                menu::ToggleInfoAction, //< I

                /* "Delete" button */
                menu::DeleteVideowallMatrixAction,
                menu::RemoveLayoutItemAction,
                menu::RemoveLayoutItemFromSceneAction,
                menu::RemoveFromServerAction,
                menu::RemoveShowreelAction})
            {
                action(actionId)->setEnabled(!isWebView);
            }
        });

    /* Create curtain animator. */
    m_curtainAnimator = new QnCurtainAnimator(this);
    m_curtainAnimator->setSpeed(1.0); /* (255, 0, 0) -> (0, 0, 0) in 1 second. */
    m_curtainAnimator->setTimer(animationTimer);

    /* Create viewport animator. */
    m_viewportAnimator = new ViewportAnimator(this); // ANIMATION: viewport.
    static constexpr qreal kDefaultScalingSpeed = 1.01; // cannot be 1.0
    m_viewportAnimator->setAbsoluteMovementSpeed(0.0); /* Viewport movement speed in scene coordinates. */
    m_viewportAnimator->setRelativeMovementSpeed(1.0); /* Viewport movement speed in viewports per second. */
    m_viewportAnimator->setScalingSpeed(kDefaultScalingSpeed); /* Viewport scaling speed, scale factor per second. */
    m_viewportAnimator->setTimer(animationTimer);
    connect(m_viewportAnimator, SIGNAL(started()), this, SIGNAL(viewportGrabbed()));
    connect(m_viewportAnimator, SIGNAL(started()), m_boundingInstrument, SLOT(recursiveDisable()));
    connect(m_viewportAnimator, SIGNAL(finished()), this, SIGNAL(viewportUngrabbed()));
    connect(m_viewportAnimator, SIGNAL(finished()), m_boundingInstrument, SLOT(recursiveEnable()));
    connect(m_viewportAnimator, SIGNAL(finished()), this, SLOT(at_viewportAnimator_finished()));

    // Connect to context.
    connect(&appContext()->localSettings()->playAudioForAllItems,
        &nx::utils::property_storage::BaseProperty::changed,
        this,
        &QnWorkbenchDisplay::updateAudioPlayback);

    connect(AudioDispatcher::instance(), &AudioDispatcher::currentAudioSourceChanged,
        this, &QnWorkbenchDisplay::updateAudioPlayback);

    m_playbackPositionBlinkTimer->start(kActivityTimeout);
}

QnWorkbenchDisplay::~QnWorkbenchDisplay()
{
    NX_ASSERT(!m_scene);
    NX_ASSERT(!m_view);
}

void QnWorkbenchDisplay::updateAudioPlayback()
{
    const auto centralWidget = m_widgetByRole[Qn::CentralRole];
    const auto audioEnabled =
        AudioDispatcher::instance()->currentAudioSource() == mainWindowWidget()->windowHandle();

    for (auto& w: m_widgets)
    {
        auto widget = qobject_cast<QnMediaResourceWidget*>(w);
        auto camDisplay = widget ? widget->display()->camDisplay() : nullptr;
        if (camDisplay)
        {
            camDisplay->playAudio(audioEnabled
                && (appContext()->localSettings()->playAudioForAllItems() || w == centralWidget));
        }
    }
}

InstrumentManager* QnWorkbenchDisplay::instrumentManager() const
{
    return m_instrumentManager;
}

BoundingInstrument* QnWorkbenchDisplay::boundingInstrument() const
{
    return m_boundingInstrument;
}

TransformListenerInstrument* QnWorkbenchDisplay::transformationListenerInstrument() const
{
    return m_transformListenerInstrument;
}

FocusListenerInstrument* QnWorkbenchDisplay::focusListenerInstrument() const
{
    return m_focusListenerInstrument;
}

ForwardingInstrument* QnWorkbenchDisplay::paintForwardingInstrument() const
{
    return m_paintForwardingInstrument;
}

SelectionOverlayTuneInstrument* QnWorkbenchDisplay::selectionOverlayTuneInstrument() const
{
    return m_selectionOverlayTuneInstrument;
}

SignalingInstrument* QnWorkbenchDisplay::beforePaintInstrument() const
{
    return m_beforePaintInstrument;
}

SignalingInstrument* QnWorkbenchDisplay::afterPaintInstrument() const
{
    return m_afterPaintInstrument;
}

FrameTimePointsProviderInstrument* QnWorkbenchDisplay::frameTimePointsInstrument() const
{
    return m_frameTimePointsInstrument;
}

void QnWorkbenchDisplay::deinitialize()
{
    NX_ASSERT(m_scene && m_view);

    /* Deinit view. */
    m_instrumentManager->unregisterView(m_view);

    m_viewportAnimator->disconnect(this);
    m_viewportAnimator->setView(nullptr);

    m_view->disconnect(this);

    /* Deinit scene. */
    m_instrumentManager->unregisterScene(m_scene);

    m_scene->disconnect(this);
    m_scene->disconnect(action(menu::SelectionChangeAction));
    action(menu::SelectionChangeAction)->disconnect(this);

    /* Clear curtain. */
    if (!m_curtainItem.isNull())
    {
        delete m_curtainItem.data();
        m_curtainItem.clear();
    }
    m_curtainAnimator->setCurtainItem(nullptr);

    /* Clear grid. */
    if (!m_gridItem.isNull())
        delete m_gridItem.data();

    /* Deinit workbench. */
    workbench()->disconnect(this);

    for (int i = 0; i < Qn::ItemRoleCount; i++)
        clearWidget(static_cast<Qn::ItemRole>(i));

    foreach(QnWorkbenchItem *item, workbench()->currentLayout()->items())
        removeItemInternal(item);

    if (gridBackgroundItem())
        delete gridBackgroundItem();

    m_scene = nullptr;
    m_view = nullptr;
}

QSet<QnWorkbenchItem*> QnWorkbenchDisplay::draggedItems() const
{
    return m_draggedItems;
}

void QnWorkbenchDisplay::setDraggedItems(const QSet<QnWorkbenchItem*>& value, bool updateGeometry)
{
    if (m_draggedItems == value)
        return;

    auto stoppedDragging = m_draggedItems - value;
    m_draggedItems = value;

    if (updateGeometry)
    {
        for (auto item: stoppedDragging)
        {
            const bool isValidItem = item && item->layout();
            if (!NX_ASSERT(isValidItem))
                continue;

            synchronizeGeometry(item, true);
        }
    }
}

bool QnWorkbenchDisplay::animationAllowed() const
{
    return !qnRuntime->lightMode().testFlag(Qn::LightModeNoAnimation) && !forceNoAnimation();
}

bool QnWorkbenchDisplay::forceNoAnimation() const
{
    return m_forceNoAnimation;
}

void QnWorkbenchDisplay::setForceNoAnimation(bool noAnimation)
{
    m_forceNoAnimation = noAnimation;
}

// ------------------------------------------------------------------------------------------------
// #TODO: #FIXME: #vkutin THIS SHOULD BE REPLACED WITH A QT PATCH.
class QnOpenGLTextureCache: public QOpenGLSharedResource
{
public:
    QMutex m_mutex;
    QCache<quint64, QOpenGLCachedTexture> m_cache;
};
// ------------------------------------------------------------------------------------------------

void QnWorkbenchDisplay::initialize(QGraphicsScene* scene, QGraphicsView* view)
{
    m_scene = scene;
    m_view = view;
    if (!NX_ASSERT(m_scene && m_view))
        return;

    /* Init scene. */
    m_instrumentManager->registerScene(m_scene);

    connect(m_scene, &QGraphicsScene::selectionChanged, this,
        [this]
        {
            menu()->trigger(menu::SelectionChangeAction);
        });
    connect(m_scene, SIGNAL(selectionChanged()), this, SLOT(at_scene_selectionChanged()));

    connect(action(menu::SelectionChangeAction), &QAction::triggered, this, &QnWorkbenchDisplay::updateSelectionFromTree);

    /* Scene indexing will only slow everything down. */
    m_scene->setItemIndexMethod(QGraphicsScene::NoIndex);

    /* Init view. */
    m_view->setScene(m_scene);
    m_instrumentManager->registerView(m_view);

    /* Configure OpenGL */
    static const char *qn_viewInitializedPropertyName = "_qn_viewInitialized";
    if (!m_view->property(qn_viewInitializedPropertyName).toBool())
    {
        const auto updateViewScreens =
            [this](QScreen* screen)
            {
                if (screen->devicePixelRatio() != mainWindowWidget()->devicePixelRatio())
                {
                    NX_WARNING(this, "Screen and window pixel ratio mismatch.");

                    // Emit screenChanged() again so main window can catch up with the screen.

                    QPointer<QScreen> screenPointer(screen);
                    const auto emitScreenChanged =
                        [this, screenPointer]
                        {
                            if (!screenPointer)
                                return;
                            mainWindowWidget()->windowHandle()->screenChanged(screenPointer.data());
                        };

                    // Call with small delay, just in case.
                    executeDelayedParented(emitScreenChanged, 100ms, this);
                    return;
                }

                for (auto& item: m_view->items())
                    setScreenRecursive(item, screen);
            };

        executeDelayedParented(
            [this, updateViewScreens]()
            {
                const auto mainWindowWidget = this->mainWindowWidget();
                const auto window = mainWindowWidget ? mainWindowWidget->windowHandle() : nullptr;
                if (!NX_ASSERT(window))
                    return;

                connect(window, &QWindow::screenChanged, this, updateViewScreens);
                updateViewScreens(window->screen());

            }, 1, this);

        const auto viewport = new QOpenGLWidget(m_view);
        viewport->makeCurrent();
        viewport->setAttribute(Qt::WA_Hover);
        m_view->setViewport(viewport);

        /* Turn on antialiasing at QPainter level. */
        m_view->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);

        // All our items save and restore painter state. Required by framed widgets.
        m_view->setOptimizationFlag(QGraphicsView::DontSavePainterState, false);

        // ----------------------------------------------------------------------------------------
        // #TODO: #FIXME: #vkutin THIS SHOULD BE REPLACED WITH A QT PATCH.
        QSharedPointer<QMetaObject::Connection> connection(new QMetaObject::Connection);
        const auto localCacheInit =
            [/*required for QObject::disconnect*/ this, connection, viewport]()
            {
                if (!NX_ASSERT(viewport->context(), "QOpenGLWidget::aboutToCompose with no context"))
                    return;
                auto cache = QOpenGLTextureCache::cacheForContext(viewport->context());
                auto localCache = reinterpret_cast<QnOpenGLTextureCache*>(cache);
                localCache->m_cache.setMaxCost(2 * 1024 * 1024);
                QObject::disconnect(*connection);
            };

        *connection = connect(viewport, &QOpenGLWidget::aboutToCompose, this, localCacheInit);
        // ----------------------------------------------------------------------------------------

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
    setLayer(m_curtainItem.data(), QnWorkbenchDisplay::BackLayer);
    m_curtainAnimator->setCurtainItem(m_curtainItem.data());

    /* Set up grid. */
    m_gridItem = new QnGridItem();
    m_scene->addItem(m_gridItem.data());
    setLayer(m_gridItem.data(), QnWorkbenchDisplay::BackLayer);
    opacityAnimator(m_gridItem.data())->setTimeLimit(300);
    m_gridItem.data()->setOpacity(0.0);
    m_gridItem.data()->setMapper(workbench()->mapper());

    if (canShowLayoutBackground())
    {
        //
        m_gridBackgroundItem = new QnGridBackgroundItem(nullptr, context());
        m_scene->addItem(gridBackgroundItem());
        setLayer(gridBackgroundItem(), QnWorkbenchDisplay::EMappingLayer);
        gridBackgroundItem()->setMapper(workbench()->mapper());
    }

    /* Connect to context. */
    connect(workbench(), &Workbench::itemChanged, this,
        &QnWorkbenchDisplay::at_workbench_itemChanged);
    connect(workbench(), &Workbench::currentLayoutAboutToBeChanged, this,
        &QnWorkbenchDisplay::at_workbench_currentLayoutAboutToBeChanged);
    connect(workbench(), &Workbench::currentLayoutChanged, this,
        &QnWorkbenchDisplay::at_workbench_currentLayoutChanged);
    connect(workbench(), &Workbench::currentLayoutItemAdded, this,
        &QnWorkbenchDisplay::at_layout_itemAdded);

    connect(workbench(), &Workbench::currentLayoutItemRemoved, this,
        &QnWorkbenchDisplay::at_layout_itemRemoved);

    /* Connect to grid mapper. */
    QnWorkbenchGridMapper *mapper = workbench()->mapper();
    connect(mapper, SIGNAL(originChanged()), this, SLOT(at_mapper_originChanged()));
    connect(mapper, SIGNAL(cellSizeChanged()), this, SLOT(at_mapper_cellSizeChanged()));
    connect(mapper, SIGNAL(spacingChanged()), this, SLOT(at_mapper_spacingChanged()));

    /* Run handlers. */
    at_workbench_currentLayoutChanged();
    at_mapper_spacingChanged();
}

void QnWorkbenchDisplay::initBoundingInstrument()
{
    NX_ASSERT(m_view);

    m_boundingInstrument->setSizeEnforced(m_view, true);
    m_boundingInstrument->setPositionEnforced(m_view, true);
    m_boundingInstrument->setScalingSpeed(m_view, 32.0);
    m_boundingInstrument->setMovementSpeed(m_view, 4.0);
}

QnGridItem *QnWorkbenchDisplay::gridItem() const
{
    return m_gridItem.data();
}

QnCurtainItem* QnWorkbenchDisplay::curtainItem() const
{
    return m_curtainItem.data();
}

QnCurtainAnimator* QnWorkbenchDisplay::curtainAnimator() const
{
    return m_curtainAnimator;
}

QnGridBackgroundItem *QnWorkbenchDisplay::gridBackgroundItem() const
{
    return m_gridBackgroundItem.data();
}

// -------------------------------------------------------------------------- //
// QnWorkbenchDisplay :: item properties
// -------------------------------------------------------------------------- //
QnWorkbenchDisplay::ItemLayer QnWorkbenchDisplay::layer(QGraphicsItem *item) const
{
    bool ok;
    ItemLayer layer = static_cast<ItemLayer>(item->data(ITEM_LAYER_KEY).toInt(&ok));
    return ok ? layer : BackLayer;
}

void QnWorkbenchDisplay::setLayer(QGraphicsItem *item, QnWorkbenchDisplay::ItemLayer layer)
{
    if (!NX_ASSERT(item))
        return;

    /* Moving items back and forth between layers should preserve their relative
     * z order. Hence the fmod. */
    item->setData(ITEM_LAYER_KEY, static_cast<int>(layer));
    item->setZValue((int)layer * layerZSize + std::fmod(item->zValue(), layerZSize));
}

void QnWorkbenchDisplay::setLayer(const QList<QGraphicsItem *> &items, QnWorkbenchDisplay::ItemLayer layer)
{
    foreach(QGraphicsItem *item, items)
        setLayer(item, layer);
}

WidgetAnimator *QnWorkbenchDisplay::animator(QnResourceWidget *widget)
{
    WidgetAnimator *animator = widget->data(ITEM_ANIMATOR_KEY).value<WidgetAnimator *>();
    if (animator)
        return animator;

    /* Create if it's not there.
     *
     * Note that widget is set as animator's parent. */
    animator = new WidgetAnimator(widget, "geometry", "rotation", widget);
    animator->setTimer(m_instrumentManager->animationTimer());

    qnWorkbenchAnimations->setupAnimator(animator, Animations::Id::SceneItemGeometryChange);

    widget->setData(ITEM_ANIMATOR_KEY, QVariant::fromValue<WidgetAnimator *>(animator));
    return animator;
}

QnResourceWidget *QnWorkbenchDisplay::widget(QnWorkbenchItem *item) const
{
    return m_widgetByItem.value(item, nullptr);
}

QnResourceWidget *QnWorkbenchDisplay::widget(Qn::ItemRole role) const
{
    if (role < 0 || role >= Qn::ItemRoleCount)
    {
        NX_ASSERT(false, "Invalid item role '%1'.", static_cast<int>(role));
        return nullptr;
    }

    return m_widgetByRole[role];
}

QnResourceWidget *QnWorkbenchDisplay::widget(const QnUuid &uuid) const
{
    return widget(workbench()->currentLayout()->item(uuid));
}

QnResourceWidget *QnWorkbenchDisplay::zoomTargetWidget(QnResourceWidget *widget) const
{
    return m_zoomTargetWidgetByWidget.value(widget);
}

QRectF QnWorkbenchDisplay::raisedGeometry(const QRectF &widgetGeometry, qreal rotation) const
{
    const auto occupiedGeometry = Geometry::rotated(widgetGeometry, rotation);
    const auto viewportGeometry = mapRectToScene(m_view, m_view->viewport()->rect());

    QSizeF newWidgetSize = occupiedGeometry.size() * focusExpansion;

    qreal viewportPercentage = kRaisedWidgetViewportPercentage;

    if (qnRuntime->isVideoWallMode())
    {
        viewportPercentage = kVideoWallRaisedWidgetViewportPercentage;
    }
    else if (canShowLayoutBackground()
        && workbench()->currentLayout()->resource()
        && !workbench()->currentLayout()->resource()->backgroundImageFilename().isEmpty())
    {
        viewportPercentage = kEMappingRaisedWidgetViewportPercentage;
    }

    QSizeF maxWidgetSize = viewportGeometry.size() * viewportPercentage;

    QPointF viewportCenter = viewportGeometry.center();

    /* Allow expansion no further than the maximal size, but no less than current size. */
    newWidgetSize = Geometry::bounded(newWidgetSize, maxWidgetSize, Qt::KeepAspectRatio);
    newWidgetSize = Geometry::expanded(newWidgetSize, occupiedGeometry.size(), Qt::KeepAspectRatio);

    /* Calculate expansion values. Expand towards the screen center. */
    qreal xp1 = 0.0, xp2 = 0.0, yp1 = 0.0, yp2 = 0.0;
    calculateExpansionValues(occupiedGeometry.left(),
        occupiedGeometry.right(),
        viewportCenter.x(),
        newWidgetSize.width(),
        &xp1,
        &xp2);
    calculateExpansionValues(occupiedGeometry.top(),
        occupiedGeometry.bottom(),
        viewportCenter.y(),
        newWidgetSize.height(),
        &yp1,
        &yp2);

    return widgetGeometry.adjusted(xp1, yp1, xp2, yp2);
}

void QnWorkbenchDisplay::setWidget(Qn::ItemRole role, QnResourceWidget *widget)
{
    if (role < 0 || role >= Qn::ItemRoleCount)
    {
        NX_ASSERT(false, "Invalid item role '%1'.", static_cast<int>(role));
        return;
    }

    const bool animate = animationAllowed();

    QnResourceWidget *oldWidget = m_widgetByRole[role];
    QnResourceWidget *newWidget = widget;
    if (oldWidget == newWidget)
        return;

    emit widgetAboutToBeChanged(role);

    m_widgetByRole[role] = widget;

    switch (role)
    {
        case Qn::RaisedRole:
        {
            /* Sync new & old geometry. */
            if (oldWidget)
                synchronize(oldWidget, animate);

            if (newWidget)
            {
                bringToFront(newWidget);
                synchronize(newWidget, animate);
            }

            break;
        }
        case Qn::ZoomedRole:
        {
            /* Sync new & old items. */
            if (oldWidget)
                synchronize(oldWidget, animate);

            m_viewportAnimator->stop();
            if (newWidget)
            {
                bringToFront(newWidget);
                synchronize(newWidget, animate);

                qnWorkbenchAnimations->setupAnimator(m_viewportAnimator, Animations::Id::SceneZoomIn);
                m_viewportAnimator->moveTo(itemGeometry(newWidget->item()), animate);
                m_curtainAnimator->curtain(newWidget);
            }
            else
            {
                qnWorkbenchAnimations->setupAnimator(m_viewportAnimator, Animations::Id::SceneZoomOut);
                m_viewportAnimator->moveTo(fitInViewGeometry(), animate);
                m_curtainAnimator->uncurtain();
            }

            /* Sync scene geometry. */
            synchronizeSceneBounds();
            synchronizeSceneBoundsExtension();

            /* Un-raise on un-zoom. */
            if (!newWidget)
                workbench()->setItem(Qn::RaisedRole, nullptr);

            /* Update media quality. */
            if (QnMediaResourceWidget *oldMediaWidget = dynamic_cast<QnMediaResourceWidget *>(oldWidget))
            {
                oldMediaWidget->display()->camDisplay()->setFullScreen(false);
            }
            if (QnMediaResourceWidget *newMediaWidget = dynamic_cast<QnMediaResourceWidget *>(newWidget))
            {
                newMediaWidget->display()->camDisplay()->setFullScreen(true);
                if (newMediaWidget->display()->archiveReader())
                {
                    newMediaWidget->display()->archiveReader()->setQuality(MEDIA_Quality_High, true);
                }
            }

            if (oldWidget)
            {
                oldWidget->setOption(QnResourceWidget::FullScreenMode, false);
                oldWidget->setOption(QnResourceWidget::ActivityPresence, false);
            }
            if (newWidget)
            {
                newWidget->setOption(QnResourceWidget::FullScreenMode, true);
                newWidget->setOption(QnResourceWidget::ActivityPresence, true);
            }

            /* Hide / show other items when zoomed. */
            static const qreal kOpaque = 1.0;
            static const qreal kTransparent = 0.0;

            auto setOpacity = [animate](QGraphicsObject* item, qreal value)
                {
                    if (animate)
                    {
                        opacityAnimator(item)->animateTo(value);
                    }
                    else
                    {
                        if (hasOpacityAnimator(item))
                            opacityAnimator(item)->stop();
                        item->setOpacity(value);
                    }
                };

            if (newWidget)
                setOpacity(newWidget, kOpaque);

            const auto opacity = (newWidget ? kTransparent : kOpaque);
            for(auto widget: m_widgets)
            {
                if (widget != newWidget)
                    setOpacity(widget, opacity);
            }

            /* Update margin flags. */
            updateCurrentMarginFlags();

            break;
        }
        case Qn::ActiveRole:
        {
            if (oldWidget)
                oldWidget->setLocalActive(false);
            if (newWidget)
                newWidget->setLocalActive(true);
            break;
        }
        case Qn::CentralRole:
        {
            /* Update audio playback. */
            if (QnMediaResourceWidget *oldMediaWidget = dynamic_cast<QnMediaResourceWidget *>(oldWidget))
            {
                QnCamDisplay *oldCamDisplay = oldMediaWidget ? oldMediaWidget->display()->camDisplay() : nullptr;
                if (oldCamDisplay)
                    oldCamDisplay->playAudio(appContext()->localSettings()->playAudioForAllItems());
            }

            if (QnMediaResourceWidget *newMediaWidget = dynamic_cast<QnMediaResourceWidget *>(newWidget))
            {
                QnCamDisplay *newCamDisplay = newMediaWidget ? newMediaWidget->display()->camDisplay() : nullptr;
                if (newCamDisplay)
                    newCamDisplay->playAudio(true);
            }

            break;
        }
        default:
            break;
    }

    emit widgetChanged(role);

    if (role == Qn::SingleSelectedRole && !m_inChangeSelection && widget && widget->isVisible()
        && m_scene->selectedItems() != QList<QGraphicsItem*>({widget}))
    {
        QScopedValueRollback updateGuard(m_inChangeSelection, true);
        widget->selectThisWidget(true);
    }
}

void QnWorkbenchDisplay::clearWidget(Qn::ItemRole role)
{
    const bool animate = animationAllowed();

    QnResourceWidget* oldWidget = m_widgetByRole[role];
    if (!oldWidget)
        return;

    emit widgetAboutToBeChanged(role);

    m_widgetByRole[role] = nullptr;

    if (role == Qn::ZoomedRole)
    {
        m_viewportAnimator->stop();
        qnWorkbenchAnimations->setupAnimator(m_viewportAnimator, Animations::Id::SceneZoomOut);
        m_viewportAnimator->moveTo(fitInViewGeometry(), animate);
        m_curtainAnimator->uncurtain();

        /* Sync scene geometry. */
        synchronizeSceneBounds();
        synchronizeSceneBoundsExtension();

        /* Un-raise on un-zoom. */
        workbench()->setItem(Qn::RaisedRole, nullptr);

        /* Show other items when zoomed item is removed. */
        auto setOpacity =
            [animate](QGraphicsObject* item, qreal value)
            {
                if (animate)
                {
                    opacityAnimator(item)->animateTo(value);
                }
                else
                {
                    if (hasOpacityAnimator(item))
                        opacityAnimator(item)->stop();
                    item->setOpacity(value);
                }
            };

        for (auto widget: m_widgets)
        {
            if (widget)
                setOpacity(widget, 1.0);
        }

        /* Update margin flags. */
        updateCurrentMarginFlags();
    }

    emit widgetChanged(role);
}

void QnWorkbenchDisplay::updateBackground(const LayoutResourcePtr& layout)
{
    if (!layout)
        return;

    if (!canShowLayoutBackground())
        return;

    if (gridBackgroundItem())
        gridBackgroundItem()->update(layout);

    synchronizeSceneBounds();

    static const bool kDontAnimate = false;
    fitInView(kDontAnimate);
}

void QnWorkbenchDisplay::updateSelectionFromTree()
{
    auto provider = menu()->targetProvider();
    if (!provider)
        return;

    auto scope = provider->currentScope();
    if (scope != menu::TreeScope)
        return;

    /* Just deselect all items for now. See #4480. */
    foreach(QGraphicsItem *item, m_scene->selectedItems())
        item->setSelected(false);
}


QList<QnResourceWidget *> QnWorkbenchDisplay::widgets() const
{
    return m_widgets;
}

QnResourceWidget* QnWorkbenchDisplay::activeWidget() const
{
    foreach(QnResourceWidget * widget, m_widgets)
    {
        if (widget->isLocalActive())
            return widget;
    }
    return nullptr;
}

QList<QnResourceWidget *> QnWorkbenchDisplay::widgets(const QnResourcePtr &resource) const
{
    return m_widgetsByResource.value(resource);
}

QnResourceDisplayPtr QnWorkbenchDisplay::display(QnWorkbenchItem *item) const
{
    if (QnMediaResourceWidget *widget = dynamic_cast<QnMediaResourceWidget *>(this->widget(item)))
        return widget->display();
    return QnResourceDisplayPtr();
}

QnClientVideoCamera *QnWorkbenchDisplay::camera(QnWorkbenchItem *item) const
{
    auto display = this->display(item);
    if (!display)
        return nullptr;

    return display->camera();
}

QnCamDisplay* QnWorkbenchDisplay::camDisplay(QnWorkbenchItem* item) const
{
    const auto camera = this->camera(item);
    if (!camera)
        return nullptr;

    return camera->getCamDisplay();
}

// -------------------------------------------------------------------------- //
// QnWorkbenchDisplay :: mutators
// -------------------------------------------------------------------------- //
void QnWorkbenchDisplay::fitInView(bool animate)
{
    QRectF targetGeometry;

    QnResourceWidget *zoomedWidget = m_widgetByRole[Qn::ZoomedRole];
    if (zoomedWidget)
    {
        targetGeometry = itemGeometry(zoomedWidget->item());
        qnWorkbenchAnimations->setupAnimator(m_viewportAnimator, Animations::Id::SceneZoomIn);
    }
    else
    {
        targetGeometry = fitInViewGeometry();
        qnWorkbenchAnimations->setupAnimator(m_viewportAnimator, Animations::Id::SceneZoomOut);
    }

    if (animate)
    {
        m_viewportAnimator->moveTo(targetGeometry, true);
    }
    else
    {
        m_boundingInstrument->recursiveDisable();
        m_viewportAnimator->moveTo(targetGeometry, false);
        m_boundingInstrument->recursiveEnable(); /* So that caches are updated. */
        synchronizeSceneBounds();
    }
}

void QnWorkbenchDisplay::bringToFront(const QList<QGraphicsItem *> &items)
{
    QList<QGraphicsItem *> localItems = items;

    /* Sort by z order first, so that relative z order is preserved. */
    std::sort(localItems.begin(), localItems.end(),
        [](QGraphicsItem *l, QGraphicsItem *r)
        {
            return l->zValue() < r->zValue();
        });

    foreach(QGraphicsItem *item, localItems)
        bringToFront(item);
}

void QnWorkbenchDisplay::bringToFront(QGraphicsItem *item)
{
    if (!NX_ASSERT(item))
        return;

    m_frontZ += zStep;
    item->setZValue(layerFrontZValue(layer(item)));
}

void QnWorkbenchDisplay::bringToFront(QnWorkbenchItem* item)
{
    if (!NX_ASSERT(item))
        return;

    const auto widget = this->widget(item);
    if (!widget)
        return; //< Widget was not created for the given item.

    bringToFront(widget);
}

bool QnWorkbenchDisplay::addItemInternal(QnWorkbenchItem *item, bool animate, bool startDisplay)
{
    int maxItems = qnRuntime->maxSceneItems();

    if (m_widgets.size() >= maxItems)
    {
        NX_DEBUG(this, lit("QnWorkbenchDisplay::addItemInternal: item count limit exceeded %1")
            .arg(maxItems));
        qnDeleteLater(item);
        return false;
    }

    auto widget = ResourceWidgetFactory::createWidget(item);
    if (!widget)
    {
        qnDeleteLater(item);
        return false;
    }

    widget->setParent(this); /* Just to feel totally safe and not to leak memory no matter what happens. */
    widget->setAttribute(Qt::WA_DeleteOnClose);

    widget->setFlag(QGraphicsItem::ItemIgnoresParentOpacity, true); /* Optimization. */
    widget->setFlag(QGraphicsItem::ItemIsSelectable, true);
    widget->setFlag(QGraphicsItem::ItemIsFocusable, true);
    widget->setFlag(QGraphicsItem::ItemIsMovable, true);

    widget->setFocusPolicy(Qt::StrongFocus);
    widget->setWindowFlags(Qt::Window);

    /* Unsetting this flag is VERY important. If it is set, graphics scene will clash with widget's
     * z value and bring it to front every time it is clicked. This will make our layered system
     * effectively unusable.
     *
     * Note that this flag must be unset after Qt::Window window flag is set because the latter
     * automatically sets the former. */
    widget->setFlag(QGraphicsItem::ItemIsPanel, false);

    m_scene->addItem(widget);

    connect(item, &QnWorkbenchItem::geometryChanged, this, &QnWorkbenchDisplay::at_item_geometryChanged);
    connect(item, &QnWorkbenchItem::geometryDeltaChanged, this, &QnWorkbenchDisplay::at_item_geometryDeltaChanged);
    connect(item, &QnWorkbenchItem::rotationChanged, this, &QnWorkbenchDisplay::at_item_rotationChanged);
    connect(item, &QnWorkbenchItem::flagChanged, this, &QnWorkbenchDisplay::at_item_flagChanged);
    connect(item, &QnWorkbenchItem::zoomRectChanged, this, &QnWorkbenchDisplay::at_item_zoomRectChanged);
    connect(item, &QnWorkbenchItem::dataChanged, this, &QnWorkbenchDisplay::at_item_dataChanged);

    m_widgets.push_back(widget);
    m_widgetByItem.insert(item, widget);

    /* Note that it is important to query resource from the widget as it may differ from the one passed
     * here because of enabled / disabled state effects. */
    QList<QnResourceWidget *> &widgetsForResource = m_widgetsByResource[widget->resource()];
    widgetsForResource.push_back(widget);

    // TODO: #sivanov Fix inconsistency between options and buttons by using flux model.

    // Restore buttons state, saved while switching layouts.
    int checkedButtons = widget->item()->data<int>(Qn::ItemCheckedButtonsRole, -1);
    if (checkedButtons != -1)
    {
        checkedButtons &= ~Qn::MotionSearchButton; //< Handled by MotionSearchSynchronizer.
        widget->setCheckedButtons(checkedButtons);
    }
    // If we are opening the widget for the first time, show info button by default if needed.
    else if (appContext()->localSettings()->showInfoByDefault())
    {
        // Block item signals to avoid dataChanged propagation to layout synchronizer. Otherwise it
        // will queue changes and post them to the snapshot manager, which will display '*'.
        QSignalBlocker blocker(widget->item());
        widget->setCheckedButtons(Qn::InfoButton);
    }

    synchronize(widget, false);
    bringToFront(widget);

    if (item->hasFlag(Qn::PendingGeometryAdjustment))
        adjustGeometryLater(item, animate); /* Changing item flags here may confuse the callee, so we do it through the event loop. */

    connect(widget, &QnResourceWidget::aspectRatioChanged, this, &QnWorkbenchDisplay::at_widget_aspectRatioChanged);

    QColor frameColor = item->data(Qn::ItemFrameDistinctionColorRole).value<QColor>();
    if (frameColor.isValid())
        widget->setFrameDistinctionColor(frameColor);

    if (auto mediaWidget = qobject_cast<QnMediaResourceWidget*>(widget))
    {
        const auto motionRegions = item->data(Qn::ItemMotionSelectionRole).value<MotionSelection>();
        if (!motionRegions.empty())
            mediaWidget->setMotionSelection(motionRegions);

        const auto analyticsRect = item->data(Qn::ItemAnalyticsSelectionRole).value<QRectF>();
        if (analyticsRect.isValid())
            mediaWidget->setAnalyticsFilterRect(analyticsRect);
    }

    emit widgetAdded(widget);
    menu()->trigger(menu::WidgetAddedEvent, menu::Parameters(widget));

    for (int i = 0; i < Qn::ItemRoleCount; i++)
    {
        if (item == workbench()->item(static_cast<Qn::ItemRole>(i)))
            setWidget(static_cast<Qn::ItemRole>(i), widget);
    }

    if (auto mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget))
    {
        if (mediaWidget->display()->archiveReader())
        {
            auto layout = item->layout()->resource();
            if (NX_ASSERT(layout) && !layout->localRange().isEmpty())
            {
                mediaWidget->display()->archiveReader()->setPlaybackRange(
                    item->layout()->resource()->localRange());
            }

            if (startDisplay)
            {
                qint64 time = item->data(Qn::ItemTimeRole).toLongLong();
                if (time > 0 && time != DATETIME_NOW)
                    time *= 1000;
                if (time > 0)
                {
                    mediaWidget->display()->archiveReader()->jumpTo(time, time);
                }
                else
                {
                    if (m_widgets.size() == 1 && !mediaWidget->resource()->toResource()->hasFlags(Qn::live))
                        mediaWidget->display()->archiveReader()->jumpTo(0, 0);
                }
            }

            // Zoom windows must not be controlled by radass or storage location controller.
            if (!mediaWidget->isZoomWindow())
            {
                appContext()->radassController()->registerConsumer(
                    mediaWidget->display()->camDisplay());
                context()->instance<StorageLocationCameraController>()->registerConsumer(
                    mediaWidget->display());
            }

        }
        if (startDisplay)
            mediaWidget->display()->start();

        integrations::registerWidget(mediaWidget);
    }

    return true;
}

bool QnWorkbenchDisplay::removeItemInternal(QnWorkbenchItem *item)
{
    item->disconnect(this);

    // Check if the widget wasn't created.
    const auto widget = this->widget(item);
    if (!widget)
        return false;

    /* Remove all linked zoom items. */
    removeZoomLinksInternal(item);

    widget->disconnect(this);

    if (const auto mediaWidget = dynamic_cast<QnMediaResourceWidget*>(widget))
        integrations::unregisterWidget(mediaWidget);

    for (int i = 0; i < Qn::ItemRoleCount; i++)
    {
        if (widget == m_widgetByRole[i])
            clearWidget(static_cast<Qn::ItemRole>(i));
    }

    emit widgetAboutToBeRemoved(widget);

    m_scene->removeItem(widget);

    const auto resource = widget->resource();

    auto widgetsForResource = m_widgetsByResource.find(resource);
    // We may have already clean the widget in permissionsChange handler
    if (widgetsForResource != m_widgetsByResource.end())
    {
        widgetsForResource->removeOne(widget);
        if (widgetsForResource->empty())
        {
            m_widgetsByResource.erase(widgetsForResource);
        }
    }

    m_widgets.removeOne(widget);
    m_widgetByItem.remove(item);

    const auto mediaWidget = qobject_cast<QnMediaResourceWidget*>(widget);
    if (mediaWidget && !mediaWidget->isZoomWindow())
    {
        appContext()->radassController()->unregisterConsumer(
            mediaWidget->display()->camDisplay());
        context()->instance<StorageLocationCameraController>()->unregisterConsumer(
            mediaWidget->display());
    }

    delete widget;
    return true;
}

bool QnWorkbenchDisplay::addZoomLinkInternal(QnWorkbenchItem *item, QnWorkbenchItem *zoomTargetItem)
{
    return addZoomLinkInternal(widget(item), widget(zoomTargetItem));
}

bool QnWorkbenchDisplay::removeZoomLinkInternal(QnWorkbenchItem *item, QnWorkbenchItem *zoomTargetItem)
{
    return removeZoomLinkInternal(widget(item), widget(zoomTargetItem));
}

bool QnWorkbenchDisplay::addZoomLinkInternal(QnResourceWidget *widget, QnResourceWidget *zoomTargetWidget)
{
    if (widget && zoomTargetWidget)
    {
        if (QnResourceWidget *oldZoomTargetWidget = m_zoomTargetWidgetByWidget.value(widget))
        {
            if (oldZoomTargetWidget == zoomTargetWidget)
                return false;
            removeZoomLinkInternal(widget, oldZoomTargetWidget);
        }

        m_zoomTargetWidgetByWidget.insert(widget, zoomTargetWidget);
        emit zoomLinkAdded(widget, zoomTargetWidget);
        emit widget->zoomTargetWidgetChanged();
        return true;
    }
    else
    {
        return false;
    }
}

bool QnWorkbenchDisplay::removeZoomLinkInternal(QnResourceWidget *widget, QnResourceWidget *zoomTargetWidget)
{
    if (widget && zoomTargetWidget)
    {
        if (m_zoomTargetWidgetByWidget.contains(widget))
        {
            emit zoomLinkAboutToBeRemoved(widget, zoomTargetWidget);
            m_zoomTargetWidgetByWidget.remove(widget);
            emit widget->zoomTargetWidgetChanged();
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}

bool QnWorkbenchDisplay::removeZoomLinksInternal(QnWorkbenchItem *item)
{
    QnResourceWidget *widget = this->widget(item);
    if (!widget)
        return false;

    removeZoomLinkInternal(widget, m_zoomTargetWidgetByWidget.value(widget));
    foreach(QnResourceWidget *zoomWidget, m_zoomTargetWidgetByWidget.keys(widget))
        removeZoomLinkInternal(zoomWidget, widget);
    return true;
}

QMargins QnWorkbenchDisplay::viewportMargins(Qn::MarginTypes marginTypes) const
{
    return m_viewportAnimator->viewportMargins(marginTypes);
}

void QnWorkbenchDisplay::setViewportMargins(const QMargins &margins, Qn::MarginType marginType)
{
    if (viewportMargins(marginType) == margins)
        return;

    m_viewportAnimator->setViewportMargins(margins, marginType);

    synchronizeSceneBoundsExtension();
}

Qn::MarginFlags QnWorkbenchDisplay::currentMarginFlags() const
{
    return m_viewportAnimator->marginFlags();
}

Qn::MarginFlags QnWorkbenchDisplay::zoomedMarginFlags() const
{
    return m_zoomedMarginFlags;
}

Qn::MarginFlags QnWorkbenchDisplay::normalMarginFlags() const
{
    return m_normalMarginFlags;
}

void QnWorkbenchDisplay::setZoomedMarginFlags(Qn::MarginFlags flags)
{
    if (m_zoomedMarginFlags == flags)
        return;

    m_zoomedMarginFlags = flags;

    updateCurrentMarginFlags();
}

void QnWorkbenchDisplay::setNormalMarginFlags(Qn::MarginFlags flags)
{
    if (m_normalMarginFlags == flags)
        return;

    m_normalMarginFlags = flags;

    updateCurrentMarginFlags();
}

void QnWorkbenchDisplay::updateCurrentMarginFlags()
{
    Qn::MarginFlags flags;
    if (!m_widgetByRole[Qn::ZoomedRole])
        flags = m_normalMarginFlags;
    else
        flags = m_zoomedMarginFlags;

    if (flags == currentMarginFlags())
        return;

    m_viewportAnimator->setMarginFlags(flags);

    synchronizeSceneBoundsExtension();
}

// -------------------------------------------------------------------------- //
// QnWorkbenchDisplay :: calculators
// -------------------------------------------------------------------------- //
qreal QnWorkbenchDisplay::layerFrontZValue(ItemLayer layer) const
{
    return layerZValue(layer) + m_frontZ;
}

qreal QnWorkbenchDisplay::layerZValue(ItemLayer layer) const
{
    return (int)layer * layerZSize;
}

QnWorkbenchDisplay::ItemLayer QnWorkbenchDisplay::shadowLayer(ItemLayer itemLayer) const
{
    switch (itemLayer)
    {
        case PinnedRaisedLayer:
            return PinnedLayer;
        case UnpinnedRaisedLayer:
            return UnpinnedLayer;
        default:
            return itemLayer;
    }
}

QnWorkbenchDisplay::ItemLayer QnWorkbenchDisplay::synchronizedLayer(QnResourceWidget *widget) const
{
    NX_ASSERT(widget);

    if (widget == m_widgetByRole[Qn::ZoomedRole])
        return ZoomedLayer;

    if (widget->item()->isPinned())
    {
        if (widget == m_widgetByRole[Qn::RaisedRole])
            return PinnedRaisedLayer;

        return PinnedLayer;
    }

    if (widget == m_widgetByRole[Qn::RaisedRole])
        return UnpinnedRaisedLayer;

    return UnpinnedLayer;
}

QRectF QnWorkbenchDisplay::itemEnclosingGeometry(QnWorkbenchItem *item) const
{
    if (!NX_ASSERT(item))
        return QRectF();

    QRectF result = workbench()->mapper()->mapFromGrid(item->geometry());

    QSizeF step = workbench()->mapper()->step();
    QRectF delta = item->geometryDelta();
    result = QRectF(
        result.left() + delta.left()   * step.width(),
        result.top() + delta.top()    * step.height(),
        result.width() + delta.width()  * step.width(),
        result.height() + delta.height() * step.height()
    );

    return result;
}

QRectF QnWorkbenchDisplay::itemGeometry(QnWorkbenchItem *item, QRectF *enclosingGeometry) const
{
    if (!NX_ASSERT(item))
        return QRectF();

    QnResourceWidget *widget = this->widget(item);
    if (!widget)
    {
        /* A perfectly normal situation - the widget was not created. */
        if (enclosingGeometry)
            *enclosingGeometry = QRectF();
        return QRectF();
    }

    QRectF geometry =
        Geometry::rotated(widget->calculateGeometry(itemEnclosingGeometry(item)), item->rotation());
    return geometry;
}

QRectF QnWorkbenchDisplay::fitInViewGeometry() const
{
    const auto layout = workbench()->currentLayout();

    QRect boundingRect = layout->boundingRect();
    if (boundingRect.isNull())
        boundingRect = QRect(0, 0, 1, 1);

    QRect background = gridBackgroundItem() ? gridBackgroundItem()->sceneBoundingRect() : QRect();
    if (!background.isEmpty())
        boundingRect = boundingRect.united(background);

    QRect minimalBoundingRect = layout->data(Qn::LayoutMinimalBoundingRectRole).value<QRect>();
    if (!minimalBoundingRect.isEmpty())
        boundingRect = boundingRect.united(minimalBoundingRect);

    if (const auto layoutResource = layout->resource())
    {
        QSize fixedSize = layoutResource->fixedSize();
        if (!fixedSize.isEmpty())
            boundingRect = boundingRect.united(QnLayoutResource::backgroundRect(fixedSize));
    }

    /* Do not add additional spacing in following cases: */
    const bool adjustSpace = !qnRuntime->isVideoWallMode()  //< Videowall client.
        && background.isEmpty(           )                  //< There is a layout background.
        && !workbench()->currentLayout()->flags().testFlag(QnLayoutFlag::FillViewport);

    QRectF sceneBoundingRect(boundingRect);
    static const qreal d = 0.015;
    if (adjustSpace)
        sceneBoundingRect = sceneBoundingRect.adjusted(-d, -d, d, d);

    return workbench()->mapper()->mapFromGridF(sceneBoundingRect);
}

QRectF QnWorkbenchDisplay::viewportGeometry() const
{
    return m_view
        ? m_viewportAnimator->accessor()->get(m_view).toRectF()
        : QRectF();
}

QRectF QnWorkbenchDisplay::boundedViewportGeometry(Qn::MarginTypes marginTypes) const
{
    if (!m_view)
        return QRectF();

    const auto boundedRect = Geometry::eroded(m_view->viewport()->rect(),
        viewportMargins(marginTypes));
    return QnSceneTransformations::mapRectToScene(m_view, boundedRect);
}

QPoint QnWorkbenchDisplay::mapViewportToGrid(const QPoint &viewportPoint) const
{
    if (!m_view)
        return QPoint();

    return workbench()->mapper()->mapToGrid(m_view->mapToScene(viewportPoint));
}

QPoint QnWorkbenchDisplay::mapGlobalToGrid(const QPoint &globalPoint) const
{
    if (!m_view)
        return QPoint();

    return mapViewportToGrid(m_view->mapFromGlobal(globalPoint));
}

QPointF QnWorkbenchDisplay::mapViewportToGridF(const QPoint &viewportPoint) const
{
    if (!m_view)
        return QPointF();

    return workbench()->mapper()->mapToGridF(m_view->mapToScene(viewportPoint));

}

QPointF QnWorkbenchDisplay::mapGlobalToGridF(const QPoint &globalPoint) const
{
    if (!m_view)
        return QPoint();

    return mapViewportToGridF(m_view->mapFromGlobal(globalPoint));
}



// -------------------------------------------------------------------------- //
// QnWorkbenchDisplay :: synchronizers
// -------------------------------------------------------------------------- //
void QnWorkbenchDisplay::synchronize(QnWorkbenchItem *item, bool animate)
{
    if (!NX_ASSERT(item))
        return;

    QnResourceWidget *widget = this->widget(item);
    if (!widget)
        return; /* No widget was created for the item provided. */

    synchronize(widget, animate);
}

void QnWorkbenchDisplay::synchronize(QnResourceWidget *widget, bool animate)
{
    if (!NX_ASSERT(widget))
        return;

    synchronizeZoomRect(widget);
    synchronizeGeometry(widget, animate);
    synchronizeLayer(widget);
    synchronizePlaceholder(widget);
}

void QnWorkbenchDisplay::synchronizeGeometry(QnWorkbenchItem *item, bool animate)
{
    QnResourceWidget *widget = this->widget(item);
    if (!widget)
        return; /* No widget was created for the given item. */

    synchronizeGeometry(widget, animate);
}

void QnWorkbenchDisplay::synchronizeGeometry(QnResourceWidget *widget, bool animate)
{
    if (!NX_ASSERT(widget))
        return;

    QnWorkbenchItem *item = widget->item();

    if (m_draggedItems.contains(item))
        return;

    QnResourceWidget *zoomedWidget = m_widgetByRole[Qn::ZoomedRole];
    QnResourceWidget *raisedWidget = m_widgetByRole[Qn::RaisedRole];

    /* Calculate rotation. */
    qreal rotation = item->rotation();
    if (item->data<bool>(Qn::ItemFlipRole, false))
        rotation += 180;

    QRectF enclosingGeometry = itemEnclosingGeometry(item);

    /* Adjust for raise. */
    if (widget == raisedWidget && widget != zoomedWidget && m_view)
    {
        QRectF targetGeometry = widget->calculateGeometry(enclosingGeometry, rotation);
        QRectF raisedGeometry = this->raisedGeometry(targetGeometry, rotation);
        qreal scale = Geometry::scaleFactor(
            targetGeometry.size(), raisedGeometry.size(), Qt::KeepAspectRatio);
        enclosingGeometry = Geometry::scaled(enclosingGeometry, scale, enclosingGeometry.center());
        enclosingGeometry.moveCenter(enclosingGeometry.center() + raisedGeometry.center() - targetGeometry.center());
    }

    /* Update Z value. */
    if (widget == raisedWidget || widget == zoomedWidget)
        bringToFront(widget);

    /* Move! */
    WidgetAnimator *animator = this->animator(widget);
    if (animate)
    {
        widget->setEnclosingGeometry(enclosingGeometry, false);
        animator->moveTo(widget->calculateGeometry(enclosingGeometry, rotation), rotation);
    }
    else
    {
        animator->stop();
        widget->setRotation(rotation);
        widget->setEnclosingGeometry(enclosingGeometry);
    }
}

void QnWorkbenchDisplay::synchronizeZoomRect(QnWorkbenchItem *item)
{
    const auto widget = this->widget(item);
    if (!widget)
        return; //< No widget was created for the given item.

    synchronizeZoomRect(widget);
}

void QnWorkbenchDisplay::synchronizeZoomRect(QnResourceWidget *widget)
{
    if (QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget))
        mediaWidget->setZoomRect(widget->item()->zoomRect());
}

void QnWorkbenchDisplay::synchronizeAllGeometries(bool animate)
{
    foreach(QnResourceWidget *widget, m_widgetByItem)
        synchronizeGeometry(widget, animate);
}

void QnWorkbenchDisplay::synchronizeLayer(QnWorkbenchItem *item)
{
    synchronizeLayer(widget(item));
}

void QnWorkbenchDisplay::synchronizeLayer(QnResourceWidget *widget)
{
    setLayer(widget, synchronizedLayer(widget));
}

void QnWorkbenchDisplay::synchronizePlaceholder(QnResourceWidget* widget)
{
    widget->setPlaceholderPixmap(widget->item()->data(Qn::ItemPlaceholderRole).value<QPixmap>());
}

void QnWorkbenchDisplay::synchronizeSceneBounds()
{
    if (!m_instrumentManager->scene())
        return; /* Do nothing if scene is being destroyed. */

    auto zoomedItem = m_widgetByRole[Qn::ZoomedRole]
        ? m_widgetByRole[Qn::ZoomedRole]->item()
        : nullptr;

    const QRectF sizeRect = zoomedItem
        ? itemGeometry(zoomedItem)
        : fitInViewGeometry();

    m_boundingInstrument->setPositionBounds(m_view, sizeRect);
    m_boundingInstrument->setSizeBounds(m_view,
        kViewportLowerSizeBound,
        Qt::KeepAspectRatioByExpanding,
        sizeRect.size(),
        Qt::KeepAspectRatioByExpanding);
}

void QnWorkbenchDisplay::synchronizeSceneBoundsExtension()
{
    QMarginsF marginsExtension(0.0, 0.0, 0.0, 0.0);

    /* If an item is zoomed then the margins should be null because all panels are hidden. */
    if (currentMarginFlags() != 0 && !m_widgetByRole[Qn::ZoomedRole])
    {
        marginsExtension = Geometry::cwiseDiv(
            m_viewportAnimator->viewportMargins(), m_view->viewport()->size());
    }

    /* Sync position extension. */
    {
        QMarginsF positionExtension(0.0, 0.0, 0.0, 0.0);

        if (currentMarginFlags() & Qn::MarginsAffectPosition)
            positionExtension = marginsExtension;

        if (m_widgetByRole[Qn::ZoomedRole])
        {
            m_boundingInstrument->setPositionBoundsExtension(m_view, positionExtension);
        }
        else
        {
            m_boundingInstrument->setPositionBoundsExtension(m_view, positionExtension + QMarginsF(0.5, 0.5, 0.5, 0.5));
        }
    }

    /* Sync size extension. */
    if (currentMarginFlags() & Qn::MarginsAffectSize)
    {
        QSizeF sizeExtension = Geometry::sizeDelta(marginsExtension);
        sizeExtension = Geometry::cwiseDiv(sizeExtension, QSizeF(1.0, 1.0) - sizeExtension);

        m_boundingInstrument->setSizeBoundsExtension(m_view, sizeExtension, sizeExtension);
        if (!m_widgetByRole[Qn::ZoomedRole])
            m_boundingInstrument->stickScale(m_view);
    }
}

void QnWorkbenchDisplay::synchronizeRaisedGeometry()
{
    QnResourceWidget *raisedWidget = m_widgetByRole[Qn::RaisedRole];
    QnResourceWidget *zoomedWidget = m_widgetByRole[Qn::ZoomedRole];
    if (!raisedWidget || raisedWidget == zoomedWidget)
        return;

    synchronizeGeometry(raisedWidget, animator(raisedWidget)->isRunning());
}

void QnWorkbenchDisplay::adjustGeometryLater(QnWorkbenchItem *item, bool animate)
{
    if (!item->hasFlag(Qn::PendingGeometryAdjustment))
        return;

    const auto widget = this->widget(item);
    if (!widget)
        return;

    widget->hide(); //< So that it won't appear where it shouldn't.

    executeDelayedParented(
        [this, item, animate]
        {
            adjustGeometry(item, animate);
        },
        item); //< Making item the parent to avoid call when it is deleted already.
}

void QnWorkbenchDisplay::adjustGeometry(QnWorkbenchItem *item, bool animate)
{
    if (!item->hasFlag(Qn::PendingGeometryAdjustment))
        return;

    QnResourceWidget *widget = this->widget(item);
    if (!widget)
        return; //< No widget was created for the given item.

    widget->show(); //< It may have been hidden in a call to adjustGeometryLater.

    if (m_widgetByRole[Qn::SingleSelectedRole] == widget)
    {
        QScopedValueRollback updateGuard(m_inChangeSelection, true);
        widget->selectThisWidget(true);
    }

    /* Calculate target position. */
    QPointF newPos;
    if (item->geometry().width() < 0 || item->geometry().height() < 0)
    {
        newPos = mapViewportToGridF(m_view->viewport()->geometry().center());

        /* Initialize item's geometry with adequate values. */
        item->setFlag(Qn::Pinned, false);
        item->setCombinedGeometry(QRectF(newPos, QSizeF(0.0, 0.0)));
        synchronizeGeometry(widget, false);
    }
    else
    {
        newPos = item->combinedGeometry().center();

        if (qFuzzyIsNull(item->combinedGeometry().size()))
            synchronizeGeometry(widget, false);
    }

    /* Calculate items size. */
    QSize size;
    if (item->layout()->items().size() == 1)
    {
        /* Layout containing only one item (current) is supposed to have the same AR as the item.
         * So we just set item size to its video layout size. */
        size = widget->channelLayout()->size();
        if (QnAspectRatio::isRotated90(item->rotation()))
            size = size.transposed();
    }
    else
    {
        qreal widgetAspectRatio = widget->visualAspectRatio();
        if (widgetAspectRatio <= 0)
        {
            QnConstResourceVideoLayoutPtr videoLayout = widget->channelLayout();
            /* Assume 4:3 AR of a single channel. In most cases, it will work fine. */
            widgetAspectRatio = Geometry::aspectRatio(
                videoLayout->size())
                    * (item->zoomRect().isNull() ? 1.0 : Geometry::aspectRatio(item->zoomRect()))
                    * (4.0 / 3.0);
            if (QnAspectRatio::isRotated90(item->rotation()))
                widgetAspectRatio = 1 / widgetAspectRatio;
        }
        const QRectF combinedGeometry = item->combinedGeometry();
        const int boundingValue =
            qMax(1, qMax(combinedGeometry.size().width(), combinedGeometry.size().height()));
        Qt::Orientation orientation = widgetAspectRatio > 1.0 ? Qt::Vertical : Qt::Horizontal;
        if (qFuzzyEquals(widgetAspectRatio, 1.0))
            orientation = Geometry::aspectRatio(workbench()->mapper()->cellSize()) > 1.0 ? Qt::Horizontal : Qt::Vertical;
        size = bestSingleBoundedSize(
            workbench()->mapper(), boundingValue, orientation, widgetAspectRatio);
    }

    /* Adjust item's geometry for the new size. */
    if (size != item->geometry().size())
    {
        QRectF combinedGeometry = item->combinedGeometry();
        combinedGeometry.moveTopLeft(
            combinedGeometry.topLeft() - Geometry::toPoint(size - combinedGeometry.size()) / 2.0);
        combinedGeometry.setSize(size);
        item->setCombinedGeometry(combinedGeometry);
    }

    /* Pin the item. */
    QnAspectRatioMagnitudeCalculator metric(
        newPos,
        size,
        item->layout()->boundingRect(),
        Geometry::aspectRatio(m_view->viewport()->size())
            / Geometry::aspectRatio(workbench()->mapper()->step()));
    QRect geometry = item->layout()->closestFreeSlot(newPos, size, &metric);
    item->layout()->pinItem(item, geometry);
    item->setFlag(Qn::PendingGeometryAdjustment, false);

    /* Synchronize. */
    synchronizeGeometry(item, animate);
}


// -------------------------------------------------------------------------- //
// QnWorkbenchDisplay :: handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchDisplay::at_viewportAnimator_finished()
{
    synchronizeSceneBounds();
}

void QnWorkbenchDisplay::at_layout_itemAdded(QnWorkbenchItem *item)
{
    const bool animate = animationAllowed();
    static const bool kStartDisplay = true;
    if (addItemInternal(item, animate, kStartDisplay))
    {
        synchronizeSceneBounds();
        fitInView(animate);

        workbench()->setItem(Qn::ZoomedRole, nullptr); //< Unzoom & fit in view on item addition.
        if (!item->data<bool>(Qn::ItemSkipFocusOnAdditionRole, false))
        {
            // Newly added item should become selected.
            workbench()->setItem(Qn::SingleSelectedRole, item);
        }
    }
}

void QnWorkbenchDisplay::at_layout_itemRemoved(QnWorkbenchItem *item)
{
    if (removeItemInternal(item))
        synchronizeSceneBounds();
}

void QnWorkbenchDisplay::at_layout_zoomLinkAdded(QnWorkbenchItem *item, QnWorkbenchItem *zoomTargetItem)
{
    addZoomLinkInternal(item, zoomTargetItem);
}

void QnWorkbenchDisplay::at_layout_zoomLinkRemoved(QnWorkbenchItem *item, QnWorkbenchItem *zoomTargetItem)
{
    removeZoomLinkInternal(item, zoomTargetItem);
}

void QnWorkbenchDisplay::at_layout_boundingRectChanged(const QRect &oldRect, const QRect &newRect)
{
    QRect backgroundBoundingRect = gridBackgroundItem() ? gridBackgroundItem()->sceneBoundingRect() : QRect();

    QRect oldBoundingRect = (backgroundBoundingRect.isNull())
        ? oldRect
        : oldRect.united(backgroundBoundingRect);
    QRect newBoundingRect = (backgroundBoundingRect.isNull())
        ? newRect
        : newRect.united(backgroundBoundingRect);
    if (oldBoundingRect != newBoundingRect)
        fitInView(animationAllowed());
}

void QnWorkbenchDisplay::at_workbench_itemChanged(Qn::ItemRole role)
{
    setWidget(role, widget(workbench()->item(role)));
}

void QnWorkbenchDisplay::at_workbench_currentLayoutAboutToBeChanged()
{
    if (m_inChangeLayout)
        return;

    m_inChangeLayout = true;
    QnWorkbenchLayout *layout = workbench()->currentLayout();

    layout->disconnect(this);
    if (layout->resource())
        layout->resource()->disconnect(this);

    auto streamSynchronizer = workbench()->windowContext()->streamSynchronizer();
    layout->setStreamSynchronizationState(streamSynchronizer->state());

    QVector<QnUuid> selectedUuids;
    foreach(QnResourceWidget *widget, widgets())
    {
        if (widget->isSelected())
            selectedUuids.push_back(widget->item()->uuid());
    }
    layout->setData(Qn::LayoutSelectionRole, QVariant::fromValue<QVector<QnUuid> >(selectedUuids));

    foreach(QnResourceWidget *widget, widgets())
    {
        if (auto mediaWidget = dynamic_cast<QnMediaResourceWidget*>(widget))
        {
            mediaWidget->beforeDestroy();

            auto item = mediaWidget->item();
            qint64 timeUSec = mediaWidget->display()->camDisplay()->getExternalTime();
            if (timeUSec != AV_NOPTS_VALUE)
            {
                const qint64 actualTime = mediaWidget->display()->camDisplay()->isRealTimeSource()
                    ? DATETIME_NOW
                    : timeUSec / 1000;
                item->setData(Qn::ItemTimeRole, actualTime);
            }

            item->setData(Qn::ItemPausedRole, mediaWidget->display()->isPaused());

            if (const auto reader = mediaWidget->display()->archiveReader())
                item->setData(Qn::ItemSpeedRole, reader->getSpeed());
        }
        else if (auto webWidget = dynamic_cast<QnWebResourceWidget*>(widget))
        {
            webWidget->item()->setData(
                Qn::ItemWebPageSavedStateDataRole,
                webWidget->webView()->controller()->suspend());
        }
    }

    foreach(QnWorkbenchItem *item, layout->items())
        at_layout_itemRemoved(item);

    m_inChangeLayout = false;
}

void QnWorkbenchDisplay::at_workbench_currentLayoutChanged()
{
    const auto workbenchLayout = workbench()->currentLayout();

    const auto layoutSearchStateData = workbenchLayout->data(Qn::LayoutSearchStateRole);
    const bool isPreviewSearchLayout = !layoutSearchStateData.isNull()
        && layoutSearchStateData.value<ThumbnailsSearchState>().step > 0
        && !workbenchLayout->items().empty();

    const auto streamSynchronizer = workbench()->windowContext()->streamSynchronizer();
    streamSynchronizer->setState(workbenchLayout->streamSynchronizationState());

    // Sort items to guarantee the same item placement for each time the same new layout is opened.
    const auto items = workbenchLayout->items();
    QVector<QnWorkbenchItem*> sortedItems(items.cbegin(), items.cend());
    std::sort(sortedItems.begin(), sortedItems.end(),
        [](const QnWorkbenchItem* lhs, const QnWorkbenchItem* rhs)
        {
            const auto lhsResourceDescriptor = lhs->data().resource;
            const auto rhsResourceDescriptor = rhs->data().resource;

            return std::make_pair(lhsResourceDescriptor.id, lhsResourceDescriptor.path)
                < std::make_pair(rhsResourceDescriptor.id, rhsResourceDescriptor.path);
        });

    for (auto item: std::as_const(sortedItems))
        addItemInternal(item, /*animate*/ false, /*startDisplay*/ !isPreviewSearchLayout);

    for (auto item: std::as_const(sortedItems))
        addZoomLinkInternal(item, item->zoomTargetItem());

    auto resourceWidgets = widgets();
    if (isPreviewSearchLayout)
    {
        std::sort(resourceWidgets.begin(), resourceWidgets.end(),
            [](QnResourceWidget* lhs, QnResourceWidget* rhs)
            {
                const auto lhsGeometry = lhs->item()->geometry();
                const auto rhsGeometry = rhs->item()->geometry();

                return std::make_pair(lhsGeometry.y(), lhsGeometry.x())
                    < std::make_pair(rhsGeometry.y(), rhsGeometry.x());
            });
    }

    for (auto resourceWidget: std::as_const(resourceWidgets))
    {
        if (auto webWidget = dynamic_cast<QnWebResourceWidget*>(resourceWidget))
        {
            auto item = webWidget->item();

            // Resume the web page from the saved state.
            webWidget->webView()->controller()->resume(
                item->data(Qn::ItemWebPageSavedStateDataRole));

            // We just moved the saved state to the web page but QVariant cannot handle move
            // semantics, so set the saved state to null.
            item->setData(Qn::ItemWebPageSavedStateDataRole, QVariant());
            continue;
        }

        const auto mediaResourceWidget = dynamic_cast<QnMediaResourceWidget*>(resourceWidget);
        if (!mediaResourceWidget)
            continue;

        const auto resource = mediaResourceWidget->resource()->toResourcePtr();

        std::optional<qint64> timeMs;
        if (const auto timeData = mediaResourceWidget->item()->data(Qn::ItemTimeRole);
            !timeData.isNull())
        {
            timeMs = timeData.value<qint64>();
        }

        // Item reported DATETIME_NOW position in milliseconds, but it does not have live.
        // Jump to 0 (default position).
        if (timeMs && (timeMs.value() == DATETIME_NOW / 1000) && !resource->hasFlags(Qn::live))
            timeMs.reset();

        if (const auto archiveReader = mediaResourceWidget->display()->archiveReader())
        {
            if (!isPreviewSearchLayout)
            {
                if (timeMs)
                {
                    const qint64 timeUSec = timeMs.value() == DATETIME_NOW
                        ? DATETIME_NOW
                        : timeMs.value() * 1000;

                    archiveReader->jumpTo(timeUSec, timeUSec);
                }
                else if (!resource->hasFlags(Qn::live))
                {
                    // Default position in SyncPlay is LIVE. If current resource is synchronized
                    // and it is not camera (does not has live) than seek to 0 (default position).
                    archiveReader->jumpTo(0, 0);
                }
            }

            if (const auto speedData = mediaResourceWidget->item()->data(Qn::ItemSpeedRole);
                !speedData.isNull() && speedData.canConvert<qreal>())
            {
                archiveReader->setSpeed(speedData.toReal());
            }

            if (mediaResourceWidget->item()->data(Qn::ItemPausedRole).toBool())
            {
                archiveReader->pauseMedia();
                archiveReader->setSpeed(0.0); // TODO: #vasilenko check that this call doesn't break anything.
            }

            if (isPreviewSearchLayout && timeMs)
            {
                const auto camDisplay = mediaResourceWidget->display()->camDisplay();
                camDisplay->setMTDecoding(false);
                camDisplay->start();
                archiveReader->startPaused(timeMs.value() * 1000);
            }
        }

        const bool hasTimeLabels = workbenchLayout->data(Qn::LayoutTimeLabelsRole).toBool();
        if (hasTimeLabels && timeMs)
        {
            mediaResourceWidget->setOverlayVisible(/*visible*/ true, /*animate*/ false);
            mediaResourceWidget->setInfoVisible(/*visible*/ true, /*animate*/ false);

            const auto timeWatcher = mediaResourceWidget->systemContext()->serverTimeWatcher();
            const qint64 displayTimeMs =
                timeMs.value() + timeWatcher->displayOffset(mediaResourceWidget->resource());

            // TODO: #sivanov Common code, another copy is in QnWorkbenchScreenshotHandler.
            QString timeString = resource->hasFlags(Qn::utc)
                ? nx::vms::time::toString(displayTimeMs)
                : nx::vms::time::toString(displayTimeMs, nx::vms::time::Format::hh_mm_ss_zzz);

            mediaResourceWidget->setTitleTextFormat(QLatin1String("%1\t") + timeString);
        }

        if (isPreviewSearchLayout)
        {
            mediaResourceWidget->item()->setData(
                Qn::ItemDisabledButtonsRole, static_cast<int>(Qn::PtzButton));
        }
    }

    const auto selectedUuids = workbenchLayout->data(Qn::LayoutSelectionRole).value<QVector<QnUuid>>();
    for (const auto& selectedUuid: selectedUuids)
    {
        if (auto resourceWidget = widget(selectedUuid))
            resourceWidget->setSelected(true);
    }

    connect(workbenchLayout, &QnWorkbenchLayout::zoomLinkAdded,
        this, &QnWorkbenchDisplay::at_layout_zoomLinkAdded);

    connect(workbenchLayout, &QnWorkbenchLayout::zoomLinkRemoved,
        this, &QnWorkbenchDisplay::at_layout_zoomLinkRemoved);

    connect(workbenchLayout, &QnWorkbenchLayout::boundingRectChanged,
        this, &QnWorkbenchDisplay::at_layout_boundingRectChanged);

    const auto layoutResource = workbenchLayout->resource();
    auto updateBackgroundCasted =
        [this](const QnLayoutResourcePtr& resource)
        {
            auto layout = resource.dynamicCast<LayoutResource>();
            if (NX_ASSERT(layout))
                updateBackground(layout);
        };

    if (NX_ASSERT(layoutResource))
    {
        connect(layoutResource.get(), &QnLayoutResource::lockedChanged,
            this, &QnWorkbenchDisplay::layoutAccessChanged);

        connect(layoutResource.get(), &QnLayoutResource::backgroundImageChanged, this,
            updateBackgroundCasted);

        connect(layoutResource.get(), &QnLayoutResource::backgroundSizeChanged, this,
            updateBackgroundCasted);

        connect(layoutResource.get(), &QnLayoutResource::backgroundOpacityChanged, this,
            updateBackgroundCasted);

        connect(layoutResource.get(), &QnLayoutResource::fixedSizeChanged, this,
            [this]
            {
                synchronizeSceneBounds();
                fitInView(/*animate*/ false);
            });

        updateBackground(layoutResource);
    }

    synchronizeSceneBounds();
    fitInView(/*animate*/ false);
}

void QnWorkbenchDisplay::at_item_dataChanged(Qn::ItemDataRole role)
{
    const auto item = static_cast<QnWorkbenchItem*>(sender());

    switch (role)
    {
        case Qn::ItemFlipRole:
            synchronizeGeometry(item, false);
            break;

        case Qn::ItemPlaceholderRole:
            synchronizePlaceholder(widget(item));
            break;

        default:
            break;
    }
}

void QnWorkbenchDisplay::at_item_geometryChanged()
{
    synchronizeGeometry(static_cast<QnWorkbenchItem *>(sender()), true);
    synchronizeSceneBounds();
}

void QnWorkbenchDisplay::at_item_geometryDeltaChanged()
{
    synchronizeGeometry(static_cast<QnWorkbenchItem *>(sender()), true);
}

void QnWorkbenchDisplay::at_item_zoomRectChanged()
{
    synchronizeZoomRect(static_cast<QnWorkbenchItem *>(sender()));
}

void QnWorkbenchDisplay::at_item_rotationChanged()
{
    QnWorkbenchItem *item = static_cast<QnWorkbenchItem *>(sender());

    synchronizeGeometry(item, true);
    if (m_widgetByRole[Qn::ZoomedRole] && m_widgetByRole[Qn::ZoomedRole]->item() == item)
        synchronizeSceneBounds();
}

void QnWorkbenchDisplay::at_item_flagChanged(Qn::ItemFlag flag, bool value)
{
    switch (flag)
    {
        case Qn::Pinned:
            synchronizeLayer(static_cast<QnWorkbenchItem *>(sender()));
            break;
        case Qn::PendingGeometryAdjustment:
            /* Changing item flags here may confuse the callee, so we do it through the event loop. */
            if (value)
                adjustGeometryLater(static_cast<QnWorkbenchItem *>(sender()), animationAllowed());
            break;
        default:
            NX_ASSERT(false, "Invalid item flag '%1'.", static_cast<int>(flag));
            break;
    }
}

void QnWorkbenchDisplay::at_widget_aspectRatioChanged()
{
    const bool animate = animationAllowed();
    synchronizeGeometry(static_cast<QnResourceWidget*>(sender()), animate);
}

void QnWorkbenchDisplay::at_scene_selectionChanged()
{
    if (!m_instrumentManager->scene() || m_inChangeSelection)
        return; /* Do nothing if scene is being destroyed. */

    QScopedValueRollback updateGuard(m_inChangeSelection, true);

    // Update single selected item. Since some items should not be focused on selection
    // (like web pages) we have to find the next suitable one.
    const auto selection = m_scene->selectedItems();
    if (selection.size() == 1)
    {
        workbench()->setItem(Qn::SingleSelectedRole, getWorkbenchItem(selection.front()));
        return;
    }

    QnWorkbenchItem* suitableItem = nullptr;
    if (const auto& centralItem = workbench()->item(Qn::CentralRole))
    {
        // Checks if central item is in selection already and looks for the any suitable
        // one if it is not.
        for (const auto& graphicsItem: selection)
        {
            const auto selectedWorkbenchItem = getWorkbenchItem(graphicsItem);
            if (selectedWorkbenchItem == centralItem)
                return; //< Central item is already in selection, do nothing.

            if (suitableItem)
                continue;

            const auto widgetOptions = widget(selectedWorkbenchItem)->options();
            if (widgetOptions.testFlag(QnResourceWidget::AllowFocus))
                suitableItem = selectedWorkbenchItem;
        }
    }

    workbench()->setItem(Qn::SingleSelectedRole, suitableItem);
}

void QnWorkbenchDisplay::at_mapper_originChanged()
{
    const bool animate = animationAllowed();

    synchronizeAllGeometries(animate);
    synchronizeSceneBounds();
    fitInView(animate);
}

void QnWorkbenchDisplay::at_mapper_cellSizeChanged()
{
    const bool animate = animationAllowed();

    synchronizeAllGeometries(animate);
    synchronizeSceneBounds();
    fitInView(animate);
}

void QnWorkbenchDisplay::at_mapper_spacingChanged()
{
    synchronizeAllGeometries(false);
    synchronizeSceneBounds();
    fitInView(false);
}

void QnWorkbenchDisplay::showNotificationSplash(
    const QnResourceList& resources, QnNotificationLevel::Value level)
{
    if (qnRuntime->lightMode().testFlag(Qn::LightModeNoNotifications))
        return;

    if (workbench()->currentLayout()->isPreviewSearchLayout())
        return;

    if (workbench()->currentLayout()->isShowreelReviewLayout())
        return;

    for (const QnResourcePtr& resource: resources)
    {
        const auto callback =
            [this, resource, level]
            {
                showSplashOnResource(resource, level);
            };

        for (int timeMs = 0; timeMs <= splashTotalLengthMs; timeMs += splashPeriodMs)
            executeDelayedParented(callback, timeMs, this);
    }
}

void QnWorkbenchDisplay::showSplashOnResource(
    const QnResourcePtr& resource,
    QnNotificationLevel::Value level)
{
    if (qnRuntime->lightMode().testFlag(Qn::LightModeNoNotifications))
        return;

    foreach(QnResourceWidget *widget, this->widgets(resource))
    {
        if (widget->zoomTargetWidget())
            continue; /* Don't draw notification on zoom widgets. */

        QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget);
        if (mediaWidget && !mediaWidget->display()->camDisplay()->isRealTimeSource())
            continue;

        QRectF rect = widget->rect();
        qreal expansion = qMin(rect.width(), rect.height()) / 2.0;

        QnSplashItem *splashItem = new QnSplashItem();
        splashItem->setSplashType(QnSplashItem::Rectangular);
        splashItem->setPos(rect.center() + widget->pos());
        splashItem->setRect(QRectF(-Geometry::toPoint(rect.size()) / 2, rect.size()));
        splashItem->setColor(withAlpha(QnNotificationLevel::notificationColor(level), 128));
        splashItem->setOpacity(0.0);
        splashItem->setRotation(widget->rotation());
        splashItem->animate(1000, Geometry::dilated(splashItem->rect(), expansion), 0.0, true, 200, 1.0);
        m_scene->addItem(splashItem);
        setLayer(splashItem, QnWorkbenchDisplay::EffectsLayer);
    }
}

bool QnWorkbenchDisplay::canShowLayoutBackground() const
{
    if (qnRuntime->isAcsMode())
        return false;

    if (qnRuntime->lightMode().testFlag(Qn::LightModeNoLayoutBackground))
        return false;

    return true;
}

QTimer* QnWorkbenchDisplay::playbackPositionBlinkTimer() const
{
    return m_playbackPositionBlinkTimer;
}
