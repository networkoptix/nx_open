// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_controller.h"

#include <algorithm>
#include <limits>

#include <QtCore/QFileInfo>
#include <QtCore/QPropertyAnimation>
#include <QtGui/QAction>
#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>

#include <camera/cam_display.h>
#include <camera/resource_display.h>
#include <client/client_globals.h>
#include <client/client_runtime_settings.h>
#include <core/resource/camera_resource.h>
#include <core/resource/resource_directory_browser.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/log.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/client/core/ptz/hotkey_resource_property_adaptor.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_resource.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/access/caching_access_controller.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/help/help_handler.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/resource/layout_password_management.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/resource/resource_descriptor.h>
#include <nx/vms/client/desktop/scene/resource_widget/overlays/rewind_widget.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_target_provider.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/ui/scene/widgets/scene_banners.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/animation/animator_group.h>
#include <ui/animation/opacity_animator.h>
#include <ui/animation/viewport_animator.h>
#include <ui/common/weak_graphics_item_pointer.h>
#include <ui/dialogs/common/custom_file_dialog.h> //for QnCustomFileDialog::fileDialogOptions() constant
#include <ui/dialogs/sign_dialog.h> // TODO: move out.
#include <ui/graphics/instruments/animation_instrument.h>
#include <ui/graphics/instruments/bounding_instrument.h>
#include <ui/graphics/instruments/click_instrument.h>
#include <ui/graphics/instruments/drag_instrument.h>
#include <ui/graphics/instruments/drop_instrument.h>
#include <ui/graphics/instruments/forwarding_instrument.h>
#include <ui/graphics/instruments/grid_adjustment_instrument.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/instruments/move_instrument.h>
#include <ui/graphics/instruments/object_tracking_instrument.h>
#include <ui/graphics/instruments/ptz_instrument.h>
#include <ui/graphics/instruments/redirecting_instrument.h>
#include <ui/graphics/instruments/resizing_instrument.h>
#include <ui/graphics/instruments/rotation_instrument.h>
#include <ui/graphics/instruments/rubber_band_instrument.h>
#include <ui/graphics/instruments/selection_fixup_instrument.h>
#include <ui/graphics/instruments/selection_overlay_tune_instrument.h>
#include <ui/graphics/instruments/signaling_instrument.h>
#include <ui/graphics/instruments/stop_accepted_instrument.h>
#include <ui/graphics/instruments/stop_instrument.h>
#include <ui/graphics/instruments/transform_listener_instrument.h>
#include <ui/graphics/instruments/wheel_zoom_instrument.h>
#include <ui/graphics/instruments/zoom_window_instrument.h>
#include <ui/graphics/items/grid/grid_item.h>
#include <ui/graphics/items/overlays/overlayed.h>
#include <ui/graphics/items/resource/button_ids.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/web_resource_widget.h>
#include <ui/graphics/items/standard/graphics_web_view.h>
#include <ui/widgets/main_window.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/checked_cast.h>
#include <utils/common/delayed.h>
#include <utils/common/delete_later.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/util.h>

#include "toggle.h"
#include "workbench_context.h"
#include "workbench_display.h"
#include "workbench_grid_mapper.h"
#include "workbench_item.h"
#include "workbench_layout.h"
#include "workbench_utility.h"

using namespace nx::vms::client;
using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;
using nx::vms::client::core::Geometry;

namespace {

static auto kDoubleClickPeriodMs = 250;
static auto kSingleClickPeriodMs = 200;
static auto kAnimationRestartPeriodMs = 500;

QPoint invalidDragDelta()
{
    return QPoint(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
}

QPoint invalidCursorPos()
{
    return invalidDragDelta();
}

/** Opacity of video items when they are dragged / resized. */
const qreal widgetManipulationOpacity = 0.3;

const qreal raisedGeometryThreshold = 0.002;

bool tourIsRunning(QnWorkbenchContext* context)
{
    return context->action(action::ToggleShowreelModeAction)->isChecked();
}

bool inViewportPtzMode(QnResourceWidget* resourceWidget, Qt::KeyboardModifiers keyboardModifiers)
{
    const auto mediaResourceWidget = qobject_cast<QnMediaResourceWidget*>(resourceWidget);
    if (!mediaResourceWidget)
        return false;

    return mediaResourceWidget->item()
        && mediaResourceWidget->item()->controlPtz()
        && mediaResourceWidget->ptzController()->hasCapabilities(Ptz::ViewportPtzCapability)
        && (appContext()->localSettings()->ptzAimOverlayEnabled()
            || keyboardModifiers.testFlag(Qt::ShiftModifier));
}

} // namespace

QnWorkbenchController::QnWorkbenchController(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_manager(display()->instrumentManager()),
    m_cursorPos(invalidCursorPos()),
    m_resizedWidget(nullptr),
    m_dragDelta(invalidDragDelta()),
    m_menuEnabled(true),
    m_rewindTimer(new QTimer(this)),
    m_rewindShortPressTimer(new QTimer(this))
{
    m_rewindDoubleClickTimer.invalidate();
    QEvent::Type mouseEventTypeArray[] = {
        QEvent::GraphicsSceneMousePress,
        QEvent::GraphicsSceneMouseMove,
        QEvent::GraphicsSceneMouseRelease,
        QEvent::GraphicsSceneMouseDoubleClick,
        QEvent::GraphicsSceneHoverEnter,
        QEvent::GraphicsSceneHoverMove,
        QEvent::GraphicsSceneHoverLeave
    };

    Instrument::EventTypeSet widgetMouseEventTypes = Instrument::makeSet(QEvent::MouseButtonPress, QEvent::MouseButtonDblClick, QEvent::MouseMove, QEvent::MouseButtonRelease);
    Instrument::EventTypeSet mouseEventTypes = Instrument::makeSet(mouseEventTypeArray);
    Instrument::EventTypeSet wheelEventTypes = Instrument::makeSet(QEvent::GraphicsSceneWheel);
    Instrument::EventTypeSet dndEventTypes = Instrument::makeSet(QEvent::GraphicsSceneDragEnter, QEvent::GraphicsSceneDragMove, QEvent::GraphicsSceneDragLeave, QEvent::GraphicsSceneDrop);
    Instrument::EventTypeSet keyEventTypes = Instrument::makeSet(QEvent::KeyPress, QEvent::KeyRelease);
    Instrument::EventTypeSet focusEventTypes = Instrument::makeSet(QEvent::FocusIn, QEvent::FocusOut);

    /* Install and configure instruments. */
    m_itemLeftClickInstrument = new ClickInstrument(Qt::LeftButton, 300, Instrument::Item, this);
    m_itemRightClickInstrument = new ClickInstrument(Qt::RightButton, 0, Instrument::Item, this);
    ClickInstrument *itemMiddleClickInstrument = new ClickInstrument(Qt::MiddleButton, 0, Instrument::Item, this);
    m_sceneClickInstrument = new ClickInstrument(Qt::LeftButton | Qt::RightButton, 0, Instrument::Scene, this);
    m_handScrollInstrument = new HandScrollInstrument(this);
    m_wheelZoomInstrument = new WheelZoomInstrument(this);
    m_rubberBandInstrument = new RubberBandInstrument(this);
    m_rotationInstrument = new RotationInstrument(this);
    m_resizingInstrument = new ResizingInstrument(this);
    m_dropInstrument = new DropInstrument(false, display()->context(), this);
    m_dragInstrument = new DragInstrument(this);
    BoundingInstrument *boundingInstrument = display()->boundingInstrument();
    SelectionOverlayTuneInstrument *selectionOverlayTuneInstrument = display()->selectionOverlayTuneInstrument();
    m_moveInstrument = new MoveInstrument(this);
    m_itemMouseForwardingInstrument = new ForwardingInstrument(Instrument::Item, mouseEventTypes, this);
    SelectionFixupInstrument *selectionFixupInstrument = new SelectionFixupInstrument(this);
    m_motionSelectionInstrument = new MotionSelectionInstrument(this);

    m_gridAdjustmentInstrument = new GridAdjustmentInstrument(workbench(), this);
    m_gridAdjustmentInstrument->setSpeed(0.25 / 360.0);
    m_gridAdjustmentInstrument->setMaxSpacing(0.15);

    SignalingInstrument* sceneKeySignalingInstrument = new SignalingInstrument(Instrument::Scene,
        Instrument::makeSet(QEvent::KeyPress, QEvent::KeyRelease), this);
    SignalingInstrument* sceneFocusSignalingInstrument = new SignalingInstrument(Instrument::Scene,
        Instrument::makeSet(QEvent::FocusIn), this);

    m_ptzInstrument = new PtzInstrument(this);
    m_zoomWindowInstrument = new ZoomWindowInstrument(this);
    m_objectTrackingInstrument = new ObjectTrackingInstrument(this);

    m_motionSelectionInstrument->setBrush(core::colorTheme()->color("camera.motionSelection.brush"));
    m_motionSelectionInstrument->setPen(core::colorTheme()->color("camera.motionSelection.pen"));
    m_motionSelectionInstrument->setSelectionModifiers(Qt::ShiftModifier);

    m_rubberBandInstrument->setRubberBandZValue(display()->layerZValue(QnWorkbenchDisplay::EffectsLayer));
    m_rotationInstrument->setRotationItemZValue(display()->layerZValue(QnWorkbenchDisplay::EffectsLayer));
    m_resizingInstrument->setInnerEffectRadius(4);
    m_resizingInstrument->setOuterEffectRadius(8);

    auto notRaisedCondition =
        [this](QGraphicsItem* item)
        {
            return item != display()->widget(Qn::RaisedRole);
        };

    m_moveInstrument->addItemCondition(new InstrumentItemConditionAdaptor(notRaisedCondition));
    m_resizingInstrument->addItemCondition(new InstrumentItemConditionAdaptor(notRaisedCondition));

    m_rotationInstrument->addItemCondition(new InstrumentItemConditionAdaptor(
        [](QGraphicsItem* item)
        {
            const auto widget = dynamic_cast<QnResourceWidget*>(item);
            if (!widget || widget->options().testFlag(QnResourceWidget::WindowRotationForbidden))
                return false;

            const auto workbenchItem = widget->item();
            if (!workbenchItem)
                return false;

            const auto layout = workbenchItem->layout();
            return layout && !layout->locked();
        }));

    /* Item instruments. */
    m_manager->installInstrument(new StopInstrument(Instrument::Item, mouseEventTypes, this));
    m_manager->installInstrument(selectionFixupInstrument);
    m_manager->installInstrument(m_itemMouseForwardingInstrument);
    m_manager->installInstrument(selectionFixupInstrument->preForwardingInstrument());
    m_manager->installInstrument(m_itemLeftClickInstrument);
    m_manager->installInstrument(m_itemRightClickInstrument);
    m_manager->installInstrument(itemMiddleClickInstrument);

    /* Scene instruments. */
    m_manager->installInstrument(new StopInstrument(Instrument::Scene, dndEventTypes, this));
    m_manager->installInstrument(m_dropInstrument);
    m_manager->installInstrument(new StopAcceptedInstrument(Instrument::Scene, dndEventTypes, this));
    m_manager->installInstrument(new ForwardingInstrument(Instrument::Scene, dndEventTypes, this));
    m_manager->installInstrument(new StopInstrument(Instrument::Scene, wheelEventTypes, this));

    m_manager->installInstrument(m_wheelZoomInstrument);
    m_manager->installInstrument(m_gridAdjustmentInstrument);
    m_manager->installInstrument(new StopAcceptedInstrument(Instrument::Scene, wheelEventTypes, this));
    m_manager->installInstrument(new ForwardingInstrument(Instrument::Scene, wheelEventTypes, this));

    m_manager->installInstrument(new StopInstrument(Instrument::Scene, mouseEventTypes, this));
    m_manager->installInstrument(m_sceneClickInstrument);
    m_manager->installInstrument(new StopAcceptedInstrument(Instrument::Scene, mouseEventTypes, this));
    m_manager->installInstrument(new ForwardingInstrument(Instrument::Scene, mouseEventTypes, this));

    m_manager->installInstrument(new StopInstrument(Instrument::Scene, keyEventTypes, this));
    m_manager->installInstrument(sceneKeySignalingInstrument);
    m_manager->installInstrument(new StopAcceptedInstrument(Instrument::Scene, keyEventTypes, this));
    m_manager->installInstrument(new ForwardingInstrument(Instrument::Scene, keyEventTypes, this));

    m_manager->installInstrument(sceneFocusSignalingInstrument);

    m_manager->installInstrument(new RedirectingInstrument(Instrument::Scene, wheelEventTypes,
        m_ptzInstrument, this));

    /* View/viewport instruments. */
    m_manager->installInstrument(m_rotationInstrument);
    m_manager->installInstrument(m_handScrollInstrument);
    m_manager->installInstrument(m_resizingInstrument);
    m_manager->installInstrument(m_moveInstrument);
    m_manager->installInstrument(m_dragInstrument);
    m_manager->installInstrument(m_rubberBandInstrument);
    m_manager->installInstrument(m_motionSelectionInstrument);
    m_manager->installInstrument(m_zoomWindowInstrument);
    m_manager->installInstrument(m_ptzInstrument);
    m_manager->installInstrument(m_objectTrackingInstrument);

    display()->setLayer(m_dropInstrument->surface(), QnWorkbenchDisplay::BackLayer);

    // Handle all clicks queued to avoid crash in the instument scene items enumeration code if
    // clicked item is destroyed in the handler (e.g. because of layout switching).
    connect(m_itemLeftClickInstrument,
        &ClickInstrument::itemPressed,
        this,
        &QnWorkbenchController::at_item_leftPressed,
        Qt::QueuedConnection);
    connect(m_itemLeftClickInstrument,
        &ClickInstrument::itemClicked,
        this,
        &QnWorkbenchController::at_item_leftClicked,
        Qt::QueuedConnection);
    connect(m_itemLeftClickInstrument,
        &ClickInstrument::itemDoubleClicked,
        this,
        &QnWorkbenchController::at_item_doubleClicked,
        Qt::QueuedConnection);
    connect(m_ptzInstrument,
        &PtzInstrument::doubleClicked,
        this,
        &QnWorkbenchController::at_resourceWidget_doubleClicked,
        Qt::QueuedConnection);
    connect(m_itemRightClickInstrument,
        &ClickInstrument::itemClicked,
        this,
        &QnWorkbenchController::at_item_rightClicked,
        Qt::QueuedConnection);
    connect(itemMiddleClickInstrument,
        &ClickInstrument::itemClicked,
        this,
        &QnWorkbenchController::at_item_middleClicked,
        Qt::QueuedConnection);
    connect(m_sceneClickInstrument,
        &ClickInstrument::sceneClicked,
        this,
        &QnWorkbenchController::at_scene_clicked,
        Qt::QueuedConnection);
    connect(m_sceneClickInstrument,
        &ClickInstrument::sceneDoubleClicked,
        this,
        &QnWorkbenchController::at_scene_doubleClicked,
        Qt::QueuedConnection);

    connect(m_moveInstrument,           SIGNAL(moveStarted(QGraphicsView *, QList<QGraphicsItem *>)),                               this,                           SLOT(at_moveStarted(QGraphicsView *, QList<QGraphicsItem *>)));
    connect(m_moveInstrument,           SIGNAL(move(QGraphicsView *, const QPointF &)),                                             this,                           SLOT(at_move(QGraphicsView *, const QPointF &)));
    connect(m_moveInstrument,           SIGNAL(moveFinished(QGraphicsView *, QList<QGraphicsItem *>)),                              this,                           SLOT(at_moveFinished(QGraphicsView *, QList<QGraphicsItem *>)));
    connect(m_resizingInstrument,       SIGNAL(resizingStarted(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),                this,                           SLOT(at_resizingStarted(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)));
    connect(m_resizingInstrument,       SIGNAL(resizing(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),                       this,                           SLOT(at_resizing(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)));
    connect(m_resizingInstrument,       SIGNAL(resizingFinished(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),               this,                           SLOT(at_resizingFinished(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)));
    connect(m_rotationInstrument,       SIGNAL(rotationStarted(QGraphicsView *, QGraphicsWidget *)),                                this,                           SLOT(at_rotationStarted(QGraphicsView *, QGraphicsWidget *)));
    connect(m_rotationInstrument,       SIGNAL(rotationFinished(QGraphicsView *, QGraphicsWidget *)),                               this,                           SLOT(at_rotationFinished(QGraphicsView *, QGraphicsWidget *)));
    connect(m_motionSelectionInstrument, SIGNAL(selectionProcessStarted(QGraphicsView *, QnMediaResourceWidget *)),                 this,                           SLOT(at_motionSelectionProcessStarted(QGraphicsView *, QnMediaResourceWidget *)));
    connect(m_motionSelectionInstrument, SIGNAL(motionRegionSelected(QGraphicsView *, QnMediaResourceWidget *, const QRect &)),     this,                           SLOT(at_motionRegionSelected(QGraphicsView *, QnMediaResourceWidget *, const QRect &)));
    connect(m_motionSelectionInstrument, SIGNAL(motionRegionCleared(QGraphicsView *, QnMediaResourceWidget *)),                     this,                           SLOT(at_motionRegionCleared(QGraphicsView *, QnMediaResourceWidget *)));
    connect(sceneKeySignalingInstrument, SIGNAL(activated(QGraphicsScene *, QEvent *)),                                             this,                           SLOT(at_scene_keyPressedOrReleased(QGraphicsScene *, QEvent *)));
    connect(sceneFocusSignalingInstrument, SIGNAL(activated(QGraphicsScene *, QEvent *)),                                           this,                           SLOT(at_scene_focusIn(QGraphicsScene *, QEvent *)));
    connect(m_zoomWindowInstrument,       SIGNAL(zoomRectChanged(QnMediaResourceWidget *, const QRectF &)),                           this,                           SLOT(at_zoomRectChanged(QnMediaResourceWidget *, const QRectF &)));
    connect(m_zoomWindowInstrument,       SIGNAL(zoomRectCreated(QnMediaResourceWidget *, const QColor &, const QRectF &)),           this,                           SLOT(at_zoomRectCreated(QnMediaResourceWidget *, const QColor &, const QRectF &)));
    connect(m_zoomWindowInstrument,       SIGNAL(zoomTargetChanged(QnMediaResourceWidget *, const QRectF &, QnMediaResourceWidget *)),this,                           SLOT(at_zoomTargetChanged(QnMediaResourceWidget *, const QRectF &, QnMediaResourceWidget *)));

    connect(m_handScrollInstrument,     SIGNAL(scrollStarted(QGraphicsView *)),                                                     boundingInstrument,             SLOT(dontEnforcePosition(QGraphicsView *)));
    connect(m_handScrollInstrument,     SIGNAL(scrollFinished(QGraphicsView *)),                                                    boundingInstrument,             SLOT(enforcePosition(QGraphicsView *)));

    connect(display(),                  SIGNAL(viewportGrabbed()),                                                                  m_handScrollInstrument,         SLOT(recursiveDisable()));
    connect(display(),                  SIGNAL(viewportUngrabbed()),                                                                m_handScrollInstrument,         SLOT(recursiveEnable()));
    connect(display(),                  SIGNAL(viewportGrabbed()),                                                                  m_wheelZoomInstrument,          SLOT(recursiveDisable()));
    connect(display(),                  SIGNAL(viewportUngrabbed()),                                                                m_wheelZoomInstrument,          SLOT(recursiveEnable()));

    connect(m_handScrollInstrument,     SIGNAL(scrollProcessStarted(QGraphicsView *)),                                              m_itemMouseForwardingInstrument, SLOT(recursiveDisable()));
    connect(m_handScrollInstrument,     SIGNAL(scrollProcessFinished(QGraphicsView *)),                                             m_itemMouseForwardingInstrument, SLOT(recursiveEnable()));
    connect(m_rubberBandInstrument,     SIGNAL(rubberBandProcessStarted(QGraphicsView *)),                                          m_itemMouseForwardingInstrument, SLOT(recursiveDisable()));
    connect(m_rubberBandInstrument,     SIGNAL(rubberBandProcessFinished(QGraphicsView *)),                                         m_itemMouseForwardingInstrument, SLOT(recursiveEnable()));
    connect(m_motionSelectionInstrument, SIGNAL(selectionProcessStarted(QGraphicsView *, QnMediaResourceWidget *)),                 m_itemMouseForwardingInstrument, SLOT(recursiveDisable()));
    connect(m_motionSelectionInstrument, SIGNAL(selectionProcessFinished(QGraphicsView *, QnMediaResourceWidget *)),                m_itemMouseForwardingInstrument, SLOT(recursiveEnable()));

    connect(m_rubberBandInstrument,     SIGNAL(rubberBandStarted(QGraphicsView *)),                                                 selectionOverlayTuneInstrument, SLOT(recursiveDisable()));
    connect(m_rubberBandInstrument,     SIGNAL(rubberBandFinished(QGraphicsView *)),                                                selectionOverlayTuneInstrument, SLOT(recursiveEnable()));

    connect(m_dragInstrument,           SIGNAL(dragProcessStarted(QGraphicsView *)),                                                m_moveInstrument,               SLOT(recursiveDisable()));
    connect(m_dragInstrument,           SIGNAL(dragProcessFinished(QGraphicsView *)),                                               m_moveInstrument,               SLOT(recursiveEnable()));

    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),         m_moveInstrument,               SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),        m_moveInstrument,               SLOT(recursiveEnable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),         m_dragInstrument,               SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),        m_dragInstrument,               SLOT(recursiveEnable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),         m_rubberBandInstrument,         SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),        m_rubberBandInstrument,         SLOT(recursiveEnable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),         m_ptzInstrument,                SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),        m_ptzInstrument,                SLOT(recursiveEnable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),         m_motionSelectionInstrument,    SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),        m_motionSelectionInstrument,    SLOT(recursiveEnable()));

    connect(m_rotationInstrument,       SIGNAL(rotationProcessStarted(QGraphicsView *, QGraphicsWidget *)),                         m_handScrollInstrument,         SLOT(recursiveDisable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessFinished(QGraphicsView *, QGraphicsWidget *)),                        m_handScrollInstrument,         SLOT(recursiveEnable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessStarted(QGraphicsView *, QGraphicsWidget *)),                         m_moveInstrument,               SLOT(recursiveDisable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessFinished(QGraphicsView *, QGraphicsWidget *)),                        m_moveInstrument,               SLOT(recursiveEnable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessStarted(QGraphicsView *, QGraphicsWidget *)),                         m_dragInstrument,               SLOT(recursiveDisable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessFinished(QGraphicsView *, QGraphicsWidget *)),                        m_dragInstrument,               SLOT(recursiveEnable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessStarted(QGraphicsView *, QGraphicsWidget *)),                         m_rubberBandInstrument,         SLOT(recursiveDisable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessFinished(QGraphicsView *, QGraphicsWidget *)),                        m_rubberBandInstrument,         SLOT(recursiveEnable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessStarted(QGraphicsView *, QGraphicsWidget *)),                         m_resizingInstrument,           SLOT(recursiveDisable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessFinished(QGraphicsView *, QGraphicsWidget *)),                        m_resizingInstrument,           SLOT(recursiveEnable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessStarted(QGraphicsView *, QGraphicsWidget *)),                         m_ptzInstrument,                SLOT(recursiveDisable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessFinished(QGraphicsView *, QGraphicsWidget *)),                        m_ptzInstrument,                SLOT(recursiveEnable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessStarted(QGraphicsView *, QGraphicsWidget *)),                         m_motionSelectionInstrument,    SLOT(recursiveDisable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessFinished(QGraphicsView *, QGraphicsWidget *)),                        m_motionSelectionInstrument,    SLOT(recursiveEnable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessStarted(QGraphicsView *, QGraphicsWidget *)),                         boundingInstrument,             SLOT(recursiveDisable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessFinished(QGraphicsView *, QGraphicsWidget *)),                        boundingInstrument,             SLOT(recursiveEnable()));

    connect(m_motionSelectionInstrument, SIGNAL(selectionProcessStarted(QGraphicsView *, QnMediaResourceWidget *)),                 m_handScrollInstrument,         SLOT(recursiveDisable()));
    connect(m_motionSelectionInstrument, SIGNAL(selectionProcessFinished(QGraphicsView *, QnMediaResourceWidget *)),                m_handScrollInstrument,         SLOT(recursiveEnable()));
    connect(m_motionSelectionInstrument, SIGNAL(selectionProcessStarted(QGraphicsView *, QnMediaResourceWidget *)),                 m_moveInstrument,               SLOT(recursiveDisable()));
    connect(m_motionSelectionInstrument, SIGNAL(selectionProcessFinished(QGraphicsView *, QnMediaResourceWidget *)),                m_moveInstrument,               SLOT(recursiveEnable()));
    connect(m_motionSelectionInstrument, SIGNAL(selectionProcessStarted(QGraphicsView *, QnMediaResourceWidget *)),                 m_dragInstrument,               SLOT(recursiveDisable()));
    connect(m_motionSelectionInstrument, SIGNAL(selectionProcessFinished(QGraphicsView *, QnMediaResourceWidget *)),                m_dragInstrument,               SLOT(recursiveEnable()));
    connect(m_motionSelectionInstrument, &MotionSelectionInstrument::selectionProcessStarted,
        m_resizingInstrument, &ResizingInstrument::recursiveDisable);
    connect(m_motionSelectionInstrument, &MotionSelectionInstrument::selectionProcessFinished,
        m_resizingInstrument, &ResizingInstrument::recursiveEnable);

    connect(m_ptzInstrument, &PtzInstrument::ptzProcessStarted, m_handScrollInstrument, &Instrument::recursiveDisable);
    connect(m_ptzInstrument, &PtzInstrument::ptzProcessFinished, m_handScrollInstrument, &Instrument::recursiveEnable);
    connect(m_ptzInstrument, &PtzInstrument::ptzProcessStarted, m_moveInstrument, &Instrument::recursiveDisable);
    connect(m_ptzInstrument, &PtzInstrument::ptzProcessFinished, m_moveInstrument, &Instrument::recursiveEnable);
    connect(m_ptzInstrument, &PtzInstrument::ptzProcessStarted, m_motionSelectionInstrument, &Instrument::recursiveDisable);
    connect(m_ptzInstrument, &PtzInstrument::ptzProcessFinished, m_motionSelectionInstrument, &Instrument::recursiveEnable);
    connect(m_ptzInstrument, &PtzInstrument::ptzProcessStarted, m_itemLeftClickInstrument, &Instrument::recursiveDisable);
    connect(m_ptzInstrument, &PtzInstrument::ptzProcessFinished, m_itemLeftClickInstrument, &Instrument::recursiveEnable);
    connect(m_ptzInstrument, &PtzInstrument::ptzProcessStarted, m_resizingInstrument, &Instrument::recursiveDisable);
    connect(m_ptzInstrument, &PtzInstrument::ptzProcessFinished, m_resizingInstrument, &Instrument::recursiveEnable);
    connect(m_ptzInstrument, &PtzInstrument::ptzProcessStarted, this, &QnWorkbenchController::at_ptzProcessStarted);

    connect(m_zoomWindowInstrument,       SIGNAL(zoomWindowProcessStarted(QnMediaResourceWidget *)),                                  m_handScrollInstrument,         SLOT(recursiveDisable()));
    connect(m_zoomWindowInstrument,       SIGNAL(zoomWindowProcessFinished(QnMediaResourceWidget *)),                                 m_handScrollInstrument,         SLOT(recursiveEnable()));
    connect(m_zoomWindowInstrument,       SIGNAL(zoomWindowProcessStarted(QnMediaResourceWidget *)),                                  m_moveInstrument,               SLOT(recursiveDisable()));
    connect(m_zoomWindowInstrument,       SIGNAL(zoomWindowProcessFinished(QnMediaResourceWidget *)),                                 m_moveInstrument,               SLOT(recursiveEnable()));
    connect(m_zoomWindowInstrument,       SIGNAL(zoomWindowProcessStarted(QnMediaResourceWidget *)),                                  m_motionSelectionInstrument,    SLOT(recursiveDisable()));
    connect(m_zoomWindowInstrument,       SIGNAL(zoomWindowProcessFinished(QnMediaResourceWidget *)),                                 m_motionSelectionInstrument,    SLOT(recursiveEnable()));
    connect(m_zoomWindowInstrument,       SIGNAL(zoomWindowProcessStarted(QnMediaResourceWidget *)),                                  m_itemLeftClickInstrument,      SLOT(recursiveDisable()));
    connect(m_zoomWindowInstrument,       SIGNAL(zoomWindowProcessFinished(QnMediaResourceWidget *)),                                 m_itemLeftClickInstrument,      SLOT(recursiveEnable()));
    connect(m_zoomWindowInstrument,       SIGNAL(zoomWindowProcessStarted(QnMediaResourceWidget *)),                                  m_ptzInstrument,                SLOT(recursiveDisable()));
    connect(m_zoomWindowInstrument,       SIGNAL(zoomWindowProcessFinished(QnMediaResourceWidget *)),                                 m_ptzInstrument,                SLOT(recursiveEnable()));
    connect(m_zoomWindowInstrument, &ZoomWindowInstrument::zoomWindowProcessStarted,
        m_resizingInstrument, &ResizingInstrument::recursiveDisable);
    connect(m_zoomWindowInstrument, &ZoomWindowInstrument::zoomWindowProcessFinished,
        m_resizingInstrument, &ResizingInstrument::recursiveEnable);

    /* Connect to display. */
    connect(display(),                  SIGNAL(widgetChanged(Qn::ItemRole)),                                                        this,                           SLOT(at_display_widgetChanged(Qn::ItemRole)));
    connect(display(),                  SIGNAL(widgetAdded(QnResourceWidget *)),                                                    this,                           SLOT(at_display_widgetAdded(QnResourceWidget *)));
    connect(display(),                  SIGNAL(widgetAboutToBeRemoved(QnResourceWidget *)),                                         this,                           SLOT(at_display_widgetAboutToBeRemoved(QnResourceWidget *)));
    connect(workbench(),                SIGNAL(currentLayoutAboutToBeChanged()),                                                    this,                           SLOT(at_workbench_currentLayoutAboutToBeChanged()));
    connect(workbench(),                SIGNAL(currentLayoutChanged()),                                                             this,                           SLOT(at_workbench_currentLayoutChanged()));

    /* Set up zoom toggle. */
    m_zoomedToggle = new QnToggle(false, this);
    connect(m_zoomedToggle,             SIGNAL(activated()),                                                                        m_moveInstrument,               SLOT(recursiveDisable()));
    connect(m_zoomedToggle,             SIGNAL(deactivated()),                                                                      m_moveInstrument,               SLOT(recursiveEnable()));
    connect(m_zoomedToggle,             SIGNAL(activated()),                                                                        m_resizingInstrument,           SLOT(recursiveDisable()));
    connect(m_zoomedToggle,             SIGNAL(deactivated()),                                                                      m_resizingInstrument,           SLOT(recursiveEnable()));
    connect(m_zoomedToggle,             SIGNAL(activated()),                                                                        m_rubberBandInstrument,         SLOT(recursiveDisable()));
    connect(m_zoomedToggle,             SIGNAL(deactivated()),                                                                      m_rubberBandInstrument,         SLOT(recursiveEnable()));
    connect(m_zoomedToggle,             SIGNAL(activated()),                                                                        this,                           SLOT(at_zoomedToggle_activated()));
    connect(m_zoomedToggle,             SIGNAL(deactivated()),                                                                      this,                           SLOT(at_zoomedToggle_deactivated()));
    m_zoomedToggle->setActive(display()->widget(Qn::ZoomedRole) != nullptr);

    connect(action(action::SelectAllAction), SIGNAL(triggered()),                                                                       this,                           SLOT(at_selectAllAction_triggered()));
    connect(action(action::StartSmartSearchAction), SIGNAL(triggered()),                                                                this,                           SLOT(at_startSmartSearchAction_triggered()));
    connect(action(action::StopSmartSearchAction), SIGNAL(triggered()),                                                                 this,                           SLOT(at_stopSmartSearchAction_triggered()));
    connect(action(action::ToggleSmartSearchAction), SIGNAL(triggered()),                                                               this,                           SLOT(at_toggleSmartSearchAction_triggered()));
    connect(action(action::ClearMotionSelectionAction), SIGNAL(triggered()),                                                            this,                           SLOT(at_clearMotionSelectionAction_triggered()));
    connect(action(action::ToggleInfoAction), SIGNAL(triggered()),                                                                      this,                           SLOT(at_toggleInfoAction_triggered()));
    connect(action(action::CheckFileSignatureAction), SIGNAL(triggered()),                                                              this,                           SLOT(at_checkFileSignatureAction_triggered()));
    connect(action(action::MaximizeItemAction), SIGNAL(triggered()),                                                                    this,                           SLOT(at_maximizeItemAction_triggered()));
    connect(action(action::UnmaximizeItemAction), SIGNAL(triggered()),                                                                  this,                           SLOT(at_unmaximizeItemAction_triggered()));
    connect(action(action::FitInViewAction), SIGNAL(triggered()),                                                                       this,                           SLOT(at_fitInViewAction_triggered()));
    connect(action(action::ToggleShowreelModeAction), &QAction::toggled, this,
        [this](bool on)
        {
            m_motionSelectionInstrument->setEnabled(!on);
        });

    connect(action(action::EscapeHotkeyAction), &QAction::triggered, this,
        &QnWorkbenchController::at_unmaximizeItemAction_triggered);

    const auto cachingController = qobject_cast<CachingAccessController*>(accessController());
    if (NX_ASSERT(cachingController))
    {
        connect(cachingController, &CachingAccessController::permissionsChanged,
            this, &QnWorkbenchController::at_accessController_permissionsChanged);
    }

    connect(
        action(action::GoToNextItemAction), &QAction::triggered,
        this, &QnWorkbenchController::at_nextItemAction_triggered);

    connect(
        action(action::GoToPreviousItemAction), &QAction::triggered,
        this, &QnWorkbenchController::at_previousItemAction_triggered);

    connect(
        action(action::GoToNextRowAction), &QAction::triggered,
        this, &QnWorkbenchController::at_nextRowAction_triggered);

    connect(
        action(action::GoToPreviousRowAction), &QAction::triggered,
        this, &QnWorkbenchController::at_previousRowAction_triggered);

    connect(
        action(action::RaiseCurrentItemAction), &QAction::triggered,
        this, &QnWorkbenchController::at_riseCurrentItemAction_triggered);

    connect(action(action::ToggleCurrentItemMaximizationStateAction), &QAction::triggered, this,
        &QnWorkbenchController::toggleCurrentItemMaximizationState);

    connect(m_rewindShortPressTimer, &QTimer::timeout,
        [this]()
        {
            m_rewindShortPressTimer->stop();
            m_rewindTimer->start(kAnimationRestartPeriodMs);

            if (m_rewindDirection == ShiftDirection::fastForward)
            {
                if (navigator()->isLive())
                {
                    m_rewindTimer->stop();
                    return;
                }
                menu()->trigger(action::FastForwardAction);
                navigator()->fastForward();
            }
            else if (m_rewindDirection == ShiftDirection::rewind)
            {
                menu()->trigger(action::RewindAction);
                navigator()->rewind(false);
            }
        });

    connect(m_rewindTimer, &QTimer::timeout,
        [this]()
        {
            if (m_rewindDirection == ShiftDirection::fastForward)
            {
                if (navigator()->isLive())
                {
                    m_rewindTimer->stop();
                    return;
                }
                menu()->trigger(action::FastForwardAction);
                navigator()->fastForward();
            }
            else if (m_rewindDirection == ShiftDirection::rewind)
            {
                menu()->trigger(action::RewindAction);
                navigator()->rewind(true);
            }
        });

    updateCurrentLayoutInstruments();
}

QnWorkbenchGridMapper* QnWorkbenchController::mapper() const
{
    return workbench()->mapper();
}

Instrument* QnWorkbenchController::motionSelectionInstrument() const
{
    return m_motionSelectionInstrument;
}

bool QnWorkbenchController::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Close)
    {
        if (QnResourceWidget* widget = qobject_cast<QnResourceWidget*>(watched))
        {
            /* Clicking on close button of a widget that is not selected should clear selection. */
            if (!widget->isSelected())
                mainWindow()->scene()->clearSelection();

            // Explicitely disabled actions cannot be triggered in Qt6.
            menu()->action(action::RemoveLayoutItemFromSceneAction)->setEnabled(true);
            menu()->trigger(action::RemoveLayoutItemFromSceneAction, widget);
            event->ignore();
            return true;
        }
    }

    return QObject::eventFilter(watched, event);
}

void QnWorkbenchController::updateGeometryDelta(QnResourceWidget* widget)
{
    if(widget->item()->isPinned())
        return;

    QRectF widgetGeometry = widget->geometry();

    QRectF gridGeometry = mapper()->mapFromGrid(widget->item()->geometry());

    QSizeF step = mapper()->step();
    QRectF geometryDelta = QRectF(
        (widgetGeometry.left()   - gridGeometry.left())   / step.width(),
        (widgetGeometry.top()    - gridGeometry.top())    / step.height(),
        (widgetGeometry.width()  - gridGeometry.width())  / step.width(),
        (widgetGeometry.height() - gridGeometry.height()) / step.height()
    );

    widget->item()->setGeometryDelta(geometryDelta);
}

void QnWorkbenchController::displayMotionGrid(const QList<QnResourceWidget*>& widgets, bool display)
{
    foreach(QnResourceWidget *widget, widgets) {
        if(!widget->resource()->hasFlags(Qn::motion))
            continue;
        if (widget->isZoomWindow())
            continue;
        widget->setOption(QnResourceWidget::DisplayMotion, display);
    }
}

void QnWorkbenchController::moveCursor(const QPoint& aAxis, const QPoint& bAxis)
{
    QnWorkbenchItem *centerItem = m_cursorItem.data();
    if(!centerItem)
        centerItem = workbench()->currentLayout()->item(m_cursorPos);

    QPoint center = m_cursorPos;
    if(centerItem && !centerItem->geometry().contains(center))
        center = centerItem->geometry().topLeft();

    QRect boundingRect = workbench()->currentLayout()->boundingRect();
    if(boundingRect.isEmpty())
        return;

    QPoint aReturn = -aAxis * qAbs(Geometry::dot(Geometry::toPoint(boundingRect.size()), aAxis) / Geometry::dot(aAxis, aAxis));
    QPoint bReturn = -bAxis * qAbs(Geometry::dot(Geometry::toPoint(boundingRect.size()), bAxis) / Geometry::dot(bAxis, bAxis));

    QPoint pos = center;
    QnWorkbenchItem *item = nullptr;
    while(true) {
        pos += aAxis;
        if(!boundingRect.contains(pos))
            pos += aReturn + bAxis;
        if(!boundingRect.contains(pos))
            pos += bReturn;
        if(pos == center)
            return; /* No other items on layout. */

        item = workbench()->currentLayout()->item(pos);
        if(item && item != centerItem)
            break;
    }

    Qn::ItemRole role = Qn::ZoomedRole;
    if (workbench()->currentLayout()->flags().testFlag(QnLayoutFlag::NoResize))
        role = Qn::SingleSelectedRole;
    else if (!workbench()->item(role))
        role = Qn::RaisedRole;

    mainWindow()->scene()->clearSelection();
    display()->widget(item)->setSelected(true);
    workbench()->setItem(role, item);
    m_cursorPos = pos;
    m_cursorItem = item;
}

void QnWorkbenchController::showContextMenuAt(const QPoint &pos)
{
    if (!m_menuEnabled)
        return;

    WeakGraphicsItemPointerList items(mainWindow()->scene()->selectedItems());
    executeDelayedParented(
        [this, pos, items]()
        {
            const auto materializedItems = items.materialized();
            QScopedPointer<QMenu> menu(this->menu()->newMenu(
                action::SceneScope, mainWindowWidget(), materializedItems));
            if (menu->isEmpty())
                return;

            const bool isWebWidget = std::all_of(
                materializedItems.cbegin(),
                materializedItems.cend(),
                [](WeakGraphicsItemPointer p)
                {
                    return dynamic_cast<QnWebResourceWidget*>(p.data());
                });

            std::vector<QnScopedTypedPropertyRollback<bool, QAction>> states;

            if (isWebWidget)
            {
                for (QAction* a: menu.data()->actions())
                {
                    states.emplace_back(
                        a,
                        &QAction::setShortcutVisibleInContextMenu,
                        &QAction::isShortcutVisibleInContextMenu,
                        false);
                }
            }

            QnHiDpiWorkarounds::showMenu(menu.data(), pos);
        }, this);
}

void QnWorkbenchController::updateDraggedItems()
{
    display()->setDraggedItems(nx::utils::toQSet(m_draggedWorkbenchItems));
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //

void QnWorkbenchController::at_scene_keyPressedOrReleased(QGraphicsScene* scene, QEvent* event)
{
    switch (event->type())
    {
        case QEvent::KeyPress:
            at_scene_keyPressed(scene, event);
            break;

        case QEvent::KeyRelease:
            at_scene_keyReleased(scene, event);
            break;

        default:
            NX_ASSERT(false);
            break;
    }
}

void QnWorkbenchController::at_scene_keyPressed(QGraphicsScene* /*scene*/, QEvent* event)
{
    if (!NX_ASSERT(event->type() == QEvent::KeyPress))
        return;

    event->accept(); /* Accept by default. */

    QKeyEvent *e = static_cast<QKeyEvent *>(event);

    auto w = mainWindow();
    NX_ASSERT(w);
    if (w && w->handleKeyPress(e->key()))
        return;

    const auto showNavigationMessage =
        [this]()
        {
            if (m_cameraSwitchKeysMessageBox)
                return;

            m_cameraSwitchKeysMessageBox.reset(SceneBanner::show(
                tr("To switch between cameras press Shift + Arrow")));
        };

    const auto tryStartPtz =
        [this](PtzInstrument::DirectionFlag direction) -> bool
        {
            const auto widget = qobject_cast<QnMediaResourceWidget*>(
                display()->widget(Qn::CentralRole));

            if (!m_ptzInstrument->supportsContinuousPtz(widget, direction))
                return false;

            m_ptzInstrument->toggleContinuousPtz(widget, direction, true);
            return true;
        };

    const auto shift =
        [this](ShiftDirection direction) -> bool
        {
            // Check that it can be rewinded or fast forwarded in general.
            if (!navigator()->canJump() ||
                (navigator()->isLive() && direction == ShiftDirection::fastForward))
            {
                return false;
            }

            // If two arrow keys are pressed simultaneously, change direction to the next one.
            if (m_rewindTimer->isActive() && m_rewindDirection == direction)
                return true;

            if (direction == ShiftDirection::fastForward)
                menu()->trigger(ui::action::FastForwardAction);
            if (direction == ShiftDirection::rewind)
                menu()->trigger(ui::action::RewindAction);

            // Start timer to define which action should be executed.
            m_rewindShortPressTimer->start(kSingleClickPeriodMs);
            m_rewindDirection = direction;
            return true;
        };

    // AZERTY keyboard passes non-numeric keycodes when caps lock is pressed.
    int key = e->key();
    bool isNumKey = false;
    const int hotkey = e->text().toInt(&isNumKey);
    if (isNumKey && qBetween(0, hotkey, 10))
        key = Qt::Key_0 + hotkey;

    switch (key)
    {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        {
            toggleCurrentItemMaximizationState();
            break;
        }
        case Qt::Key_Up:
        {
            if (e->modifiers() & Qt::AltModifier)
                m_handScrollInstrument->emulate(QPoint(0, -15));
            else if (e->modifiers() & Qt::ShiftModifier)
                moveCursor(QPoint(0, -1), QPoint(-1, 0));
            else if (e->isAutoRepeat())
                break;
            else if (!tryStartPtz(PtzInstrument::DirectionFlag::tiltUp))
                showNavigationMessage();
            break;
        }
        case Qt::Key_Down:
        {
            if (e->modifiers() & Qt::AltModifier)
                m_handScrollInstrument->emulate(QPoint(0, 15));
            else if (e->modifiers() & Qt::ShiftModifier)
                moveCursor(QPoint(0, 1), QPoint(1, 0));
            else if (e->isAutoRepeat())
                break;
            else if (!tryStartPtz(PtzInstrument::DirectionFlag::tiltDown))
                showNavigationMessage();
            break;
        }
        case Qt::Key_Left:
        {
            if (e->modifiers() & Qt::AltModifier)
                m_handScrollInstrument->emulate(QPoint(-15, 0));
            else if (e->modifiers() & Qt::ShiftModifier)
                moveCursor(QPoint(-1, 0), QPoint(0, -1));
            else if (e->isAutoRepeat())
                break;
            else if (tryStartPtz(PtzInstrument::DirectionFlag::panLeft))
                break;
            else if (!shift(ShiftDirection::rewind))
                showNavigationMessage();
            break;
        }
        case Qt::Key_Right:
        {
            if (e->modifiers() & Qt::AltModifier)
                m_handScrollInstrument->emulate(QPoint(15, 0));
            else if (e->modifiers() & Qt::ShiftModifier)
                moveCursor(QPoint(1, 0), QPoint(0, 1));
            else if (e->isAutoRepeat())
                break;
            else if (tryStartPtz(PtzInstrument::DirectionFlag::panRight))
                break;
            else if (!shift(ShiftDirection::fastForward))
                showNavigationMessage();
            break;
        }
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            if (e->modifiers() & Qt::AltModifier)
                m_wheelZoomInstrument->emulate(30);
            else if (e->isAutoRepeat())
                break;
            else
                tryStartPtz(PtzInstrument::DirectionFlag::zoomIn);
            break;
        case Qt::Key_Minus:
            if (e->modifiers() & Qt::AltModifier)
                m_wheelZoomInstrument->emulate(-30);
            else if (e->isAutoRepeat())
                break;
            else
                tryStartPtz(PtzInstrument::DirectionFlag::zoomOut);
            break;
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            break;
        case Qt::Key_Menu:
        {
            QGraphicsView *view = mainWindow()->view();
            QList<QGraphicsItem *> items = mainWindow()->scene()->selectedItems();
            QPoint offset = view->mapToGlobal(QPoint(0, 0));
            if (items.count() == 0)
            {
                showContextMenuAt(offset);
            }
            else
            {
                QRectF rect = items[0]->mapToScene(items[0]->boundingRect()).boundingRect();
                QRect testRect = QnSceneTransformations::mapRectFromScene(view, rect); /* Where is the static analogue? */
                showContextMenuAt(offset + testRect.bottomRight());
            }
            break;
        }
        case Qt::Key_0:
        case Qt::Key_1:
        case Qt::Key_2:
        case Qt::Key_3:
        case Qt::Key_4:
        case Qt::Key_5:
        case Qt::Key_6:
        case Qt::Key_7:
        case Qt::Key_8:
        case Qt::Key_9:
        {
            QnMediaResourceWidget *widget = dynamic_cast<QnMediaResourceWidget*>(display()->widget(Qn::CentralRole));
            if (!widget || !widget->ptzController())
                break;

            nx::vms::client::core::ptz::HotkeysResourcePropertyAdaptor adaptor;
            adaptor.setResource(widget->resource()->toResourcePtr());

            QString objectId = adaptor.value().value(hotkey);
            if (objectId.isEmpty())
                break;

            menu()->trigger(action::PtzActivateObjectAction, action::Parameters(widget).withArgument(Qn::PtzObjectIdRole, objectId));
            break;
        }
        default:
            event->ignore(); /* Wasn't recognized? Ignore. */
            break;
    }
}

void QnWorkbenchController::at_scene_keyReleased(QGraphicsScene* /*scene*/, QEvent* event)
{
    if (!NX_ASSERT(event->type() == QEvent::KeyRelease))
        return;

    event->accept();

    const auto e = static_cast<QKeyEvent*>(event);
    if (e->isAutoRepeat())
        return;

    const auto tryStopPtz =
        [this](PtzInstrument::DirectionFlag direction)
        {
            const auto widget = qobject_cast<QnMediaResourceWidget*>(
                display()->widget(Qn::CentralRole));

            m_ptzInstrument->toggleContinuousPtz(widget, direction, false);
        };

    const auto doubleClick =
        [this]() -> bool
        {
            if (!m_rewindDoubleClickTimer.isValid())
            {
                m_rewindDoubleClickTimer.start();
                return false;
            }

            // This part should be triggered on double click on rewind only.
            if (m_rewindDoubleClickTimer.restart() < kDoubleClickPeriodMs)
            {
                m_rewindTimer->stop();
                m_rewindShortPressTimer->stop();
                if (m_rewindDirection == ShiftDirection::rewind)
                {
                    menu()->trigger(action::RewindAction);
                    navigator()->rewindOnDoubleClick();
                    return true;
                }
            }
            return false;
        };

    const auto stopShift =
        [&]()
        {
            m_rewindTimer->stop();
            m_rewindShortPressTimer->stop();
            if (!doubleClick())
            {
                // Single click.
                if (m_rewindDirection == ShiftDirection::fastForward)
                {
                    if (navigator()->isLive())
                        return;

                    menu()->trigger(action::FastForwardAction);
                    navigator()->fastForward();
                }
                else if (m_rewindDirection == ShiftDirection::rewind)
                {
                    menu()->trigger(action::RewindAction);
                    navigator()->rewind();
                }
            }
        };

    switch (e->key())
    {
        case Qt::Key_Up:
            tryStopPtz(PtzInstrument::DirectionFlag::tiltUp);
            break;

        case Qt::Key_Down:
            tryStopPtz(PtzInstrument::DirectionFlag::tiltDown);
            break;

        case Qt::Key_Left:
            tryStopPtz(PtzInstrument::DirectionFlag::panLeft);
            stopShift();
            break;

        case Qt::Key_Right:
            tryStopPtz(PtzInstrument::DirectionFlag::panRight);
            stopShift();
            break;

        case Qt::Key_Plus:
        case Qt::Key_Equal:
            tryStopPtz(PtzInstrument::DirectionFlag::zoomIn);
            break;

        case Qt::Key_Minus:
            tryStopPtz(PtzInstrument::DirectionFlag::zoomOut);
            break;

        default:
            break;
    }
}

void QnWorkbenchController::at_scene_focusIn(QGraphicsScene* /*scene*/, QEvent* event)
{
    // Prevent focus jumps when scene is focused.

    // In Qt6 the copy operator is deleted for QFocusEvent.
    struct MutableFocusEvent: QEvent
    {
        Qt::FocusReason m_reason;
    };
    static_assert(sizeof(QFocusEvent) == sizeof(MutableFocusEvent), "Wrong size of utility access class");
    reinterpret_cast<MutableFocusEvent *>(event)->m_reason = Qt::OtherFocusReason;
}

void QnWorkbenchController::at_resizingStarted(QGraphicsView *, QGraphicsWidget *item, ResizingInfo *info) {
    m_resizedWidget = qobject_cast<QnResourceWidget *>(item);
    if(m_resizedWidget == nullptr)
        return;

    if(m_resizedWidget->item() == nullptr)
        return;

    m_selectionOverlayTuneInstrumentDisabled = true;
    display()->selectionOverlayTuneInstrument()->recursiveDisable();

    workbench()->setItem(Qn::RaisedRole, nullptr); /* Un-raise currently raised item so that it doesn't interfere with resizing. */

    display()->bringToFront(m_resizedWidget);
    opacityAnimator(display()->gridItem())->animateTo(1.0);
    opacityAnimator(m_resizedWidget)->animateTo(widgetManipulationOpacity);

    /* Calculate snap point */
    Qt::WindowFrameSection grabbedSection = Qn::rotateSection(info->frameSection(), item->rotation());
    QRect initialGeometry = m_resizedWidget->item()->geometry();
    QRect widgetInitialGeometry = mapper()->mapToGrid(
        Geometry::rotated(m_resizedWidget->geometry(), m_resizedWidget->rotation()));
    m_resizingSnapPoint = Qn::calculatePinPoint(initialGeometry.intersected(widgetInitialGeometry), grabbedSection);
}

void QnWorkbenchController::at_resizing(QGraphicsView* /*view*/, QGraphicsWidget* item, ResizingInfo* info)
{
    if(m_resizedWidget != item || item == nullptr)
        return;

    QRectF widgetGeometry =
        Geometry::rotated(m_resizedWidget->geometry(), m_resizedWidget->rotation());

    /* Calculate integer size. */
    QSize gridSize = mapper()->mapToGrid(widgetGeometry).size();
    if (gridSize.isEmpty())
        gridSize = gridSize.expandedTo(QSize(1, 1));

    /* Correct grabbed section according to item rotation. */
    Qt::WindowFrameSection grabbedSection = Qn::rotateSection(info->frameSection(), item->rotation());

    /* Calculate new item position .*/
    QPoint newPosition;
    switch (grabbedSection) {
    case Qt::TopSection:
        newPosition = QPoint(m_resizingSnapPoint.x() - gridSize.width() / 2, m_resizingSnapPoint.y() - gridSize.height());
        break;
    case Qt::LeftSection:
        newPosition = QPoint(m_resizingSnapPoint.x() - gridSize.width(), m_resizingSnapPoint.y() - gridSize.height() / 2);
        break;
    case Qt::TopLeftSection:
        newPosition = QPoint(m_resizingSnapPoint.x() - gridSize.width(), m_resizingSnapPoint.y() - gridSize.height());
        break;
    case Qt::BottomSection:
        newPosition = QPoint(m_resizingSnapPoint.x() - gridSize.width() / 2, m_resizingSnapPoint.y());
        break;
    case Qt::RightSection:
        newPosition = QPoint(m_resizingSnapPoint.x(), m_resizingSnapPoint.y() - gridSize.height() / 2);
        break;
    case Qt::BottomRightSection:
        newPosition = m_resizingSnapPoint;
        break;
    case Qt::TopRightSection:
        newPosition = QPoint(m_resizingSnapPoint.x(), m_resizingSnapPoint.y() - gridSize.height());
        break;
    case Qt::BottomLeftSection:
        newPosition = QPoint(m_resizingSnapPoint.x() - gridSize.width(), m_resizingSnapPoint.y());
        break;
    default:
        newPosition = m_resizingSnapPoint;
    }

    QRect newResizingWidgetRect(newPosition, gridSize);

    if(newResizingWidgetRect != m_resizedWidgetRect) {
        QnWorkbenchLayout::Disposition disposition;
        m_resizedWidget->item()->layout()->canMoveItem(m_resizedWidget->item(), newResizingWidgetRect, &disposition);

        display()->gridItem()->setCellState(m_resizedWidgetRect, QnGridItem::Initial);
        display()->gridItem()->setCellState(disposition.free, QnGridItem::Allowed);
        display()->gridItem()->setCellState(disposition.occupied, QnGridItem::Disallowed);

        m_resizedWidgetRect = newResizingWidgetRect;
    }
}

void QnWorkbenchController::at_resizingFinished(QGraphicsView* /*view*/, QGraphicsWidget* item, ResizingInfo* /*info*/)
{
    opacityAnimator(display()->gridItem())->animateTo(0.0);
    if (m_resizedWidget)
        opacityAnimator(m_resizedWidget)->animateTo(1.0);

    if(m_resizedWidget == item && item != nullptr) {
        QnResourceWidget *widget = m_resizedWidget;

        /* Resize if possible. */
        QSet<QnWorkbenchItem *> entities = widget->item()->layout()->items(m_resizedWidgetRect);
        entities.remove(widget->item());
        if (entities.empty()) {
            widget->item()->setGeometry(m_resizedWidgetRect);
            updateGeometryDelta(widget);
        }

        display()->synchronize(widget->item(), display()->animationAllowed());

        /* Un-raise the raised item if it was the one being resized. */
        QnWorkbenchItem *raisedItem = workbench()->item(Qn::RaisedRole);
        if(raisedItem == widget->item())
            workbench()->setItem(Qn::RaisedRole, nullptr);
    }

    /* Clean up resizing state. */
    display()->gridItem()->setCellState(m_resizedWidgetRect, QnGridItem::Initial);
    m_resizedWidgetRect = QRect();
    m_resizedWidget = nullptr;
    if(m_selectionOverlayTuneInstrumentDisabled) {
        display()->selectionOverlayTuneInstrument()->recursiveEnable();
        m_selectionOverlayTuneInstrumentDisabled = false;
    }
}

void QnWorkbenchController::at_moveStarted(QGraphicsView *, const QList<QGraphicsItem *> &items)
{
    /* Build item lists. */
    for (QGraphicsItem *item: items)
    {
        QnResourceWidget *widget = item->isWidget() ? qobject_cast<QnResourceWidget *>(item->toGraphicsObject()) : nullptr;
        if (!widget)
            continue;

        m_draggedWorkbenchItems.push_back(widget->item());
        opacityAnimator(widget)->animateTo(widgetManipulationOpacity);
        updateDraggedItems();
    }

    if (m_draggedWorkbenchItems.empty())
        return;

    m_selectionOverlayTuneInstrumentDisabled2 = true;
    display()->selectionOverlayTuneInstrument()->recursiveDisable();

    /* Bring to front preserving relative order. */
    display()->bringToFront(items);
    display()->setLayer(items, QnWorkbenchDisplay::FrontLayer);

    /* Show grid. */
    opacityAnimator(display()->gridItem())->animateTo(1.0);
}

void QnWorkbenchController::at_move(QGraphicsView*, const QPointF& totalDelta)
{
    if (m_draggedWorkbenchItems.empty())
        return;

    QnWorkbenchLayout* layout = m_draggedWorkbenchItems[0]->layout();

    QPoint newDragDelta = mapper()->mapDeltaToGridF(totalDelta).toPoint();
    if (newDragDelta == m_dragDelta)
        return;

    display()->gridItem()->setCellState(m_dragGeometries, QnGridItem::Initial);

    m_dragDelta = newDragDelta;
    m_replacedWorkbenchItems.clear();
    m_dragGeometries.clear();

    /* Handle single widget case. */
    bool finished = false;
    if (m_draggedWorkbenchItems.size() == 1 && m_draggedWorkbenchItems[0])
    {
        QnWorkbenchItem* draggedWorkbenchItem = m_draggedWorkbenchItems[0];

        /* Find item that dragged item was dropped on. */
        QSet<QnWorkbenchItem *> replacedWorkbenchItems = layout->items(draggedWorkbenchItem->geometry().adjusted(m_dragDelta.x(), m_dragDelta.y(), m_dragDelta.x(), m_dragDelta.y()));
        if (replacedWorkbenchItems.size() == 1)
        {
            QnWorkbenchItem* replacedWorkbenchItem = *replacedWorkbenchItems.begin();

            /* Switch places if dropping smaller one on a bigger one. */
            if (replacedWorkbenchItem && replacedWorkbenchItem != draggedWorkbenchItem && draggedWorkbenchItem->isPinned())
            {
                QSizeF draggedSize = draggedWorkbenchItem->geometry().size();
                QSizeF replacedSize = replacedWorkbenchItem->geometry().size();
                if (replacedSize.width() > draggedSize.width() && replacedSize.height() > draggedSize.height())
                {
                    QnResourceWidget *draggedWidget = display()->widget(draggedWorkbenchItem);
                    QnResourceWidget *replacedWidget = display()->widget(replacedWorkbenchItem);

                    m_replacedWorkbenchItems.push_back(replacedWorkbenchItem);

                    if (draggedWidget->hasAspectRatio())
                        m_dragGeometries.push_back(bestDoubleBoundedGeometry(mapper(), replacedWorkbenchItem->geometry(), draggedWidget->aspectRatio()));
                    else
                        m_dragGeometries.push_back(replacedWorkbenchItem->geometry());

                    if (replacedWidget->hasAspectRatio())
                        m_dragGeometries.push_back(bestDoubleBoundedGeometry(mapper(), draggedWorkbenchItem->geometry(), replacedWidget->aspectRatio()));
                    else
                        m_dragGeometries.push_back(draggedWorkbenchItem->geometry());

                    finished = true;
                }
            }
        }
    }

    /* Handle all other cases. */
    if (!finished)
    {
        QList<QRect> replacedGeometries;
        for (QnWorkbenchItem* workbenchItem: m_draggedWorkbenchItems)
        {
            QRect geometry = workbenchItem->geometry().adjusted(m_dragDelta.x(), m_dragDelta.y(), m_dragDelta.x(), m_dragDelta.y());
            m_dragGeometries.push_back(geometry);
            if (workbenchItem->isPinned())
                replacedGeometries.push_back(geometry);
        }

        m_replacedWorkbenchItems = layout->items(replacedGeometries)
            .subtract(nx::utils::toQSet(m_draggedWorkbenchItems))
            .values();

        replacedGeometries.clear();
        for (QnWorkbenchItem* workbenchItem: m_replacedWorkbenchItems)
            replacedGeometries.push_back(workbenchItem->geometry().adjusted(-m_dragDelta.x(), -m_dragDelta.y(), -m_dragDelta.x(), -m_dragDelta.y()));

        m_dragGeometries.append(replacedGeometries);
    }

    QnWorkbenchLayout::Disposition disposition;
    layout->canMoveItems(m_draggedWorkbenchItems + m_replacedWorkbenchItems, m_dragGeometries, &disposition); /*< Just calculating disposition. */

    display()->gridItem()->setCellState(disposition.free, QnGridItem::Allowed);
    display()->gridItem()->setCellState(disposition.occupied, QnGridItem::Disallowed);

}

void QnWorkbenchController::at_moveFinished(QGraphicsView* /*view*/, const QList<QGraphicsItem*>& /*items*/)
{
    /* Hide grid. */
    opacityAnimator(display()->gridItem())->animateTo(0.0);

    if(!m_draggedWorkbenchItems.empty()) {
        QnWorkbenchLayout *layout = m_draggedWorkbenchItems[0]->layout();

        /* Do drag. */
        QList<QnWorkbenchItem*> workbenchItems = m_draggedWorkbenchItems + m_replacedWorkbenchItems;
        bool success = layout->moveItems(workbenchItems, m_dragGeometries);

        for (QnWorkbenchItem *item: workbenchItems)
        {
            QnResourceWidget *widget = display()->widget(item);

            /* Adjust geometry deltas if everything went fine. */
            if(success)
                updateGeometryDelta(widget);

            /* Animate opacity back. */
            opacityAnimator(widget)->animateTo(1.0);
        }

        /* Re-sync everything. */
        for (QnWorkbenchItem* workbenchItem: workbenchItems)
            display()->synchronize(workbenchItem, display()->animationAllowed());

        /* Un-raise the raised item if it was among the dragged ones. */
        QnWorkbenchItem *raisedItem = workbench()->item(Qn::RaisedRole);
        if(raisedItem != nullptr && workbenchItems.contains(raisedItem))
            workbench()->setItem(Qn::RaisedRole, nullptr);
    }

    /* Clean up dragging state. */
    display()->gridItem()->setCellState(m_dragGeometries, QnGridItem::Initial);
    m_dragDelta = invalidDragDelta();
    m_draggedWorkbenchItems.clear();
    m_replacedWorkbenchItems.clear();
    m_dragGeometries.clear();
    updateDraggedItems();

    if(m_selectionOverlayTuneInstrumentDisabled2) {
        display()->selectionOverlayTuneInstrument()->recursiveEnable();
        m_selectionOverlayTuneInstrumentDisabled2 = false;
    }
}

void QnWorkbenchController::at_rotationStarted(QGraphicsView* /*view*/, QGraphicsWidget* widget)
{
    display()->bringToFront(widget);
}

void QnWorkbenchController::at_rotationFinished(QGraphicsView* /*view*/, QGraphicsWidget* widget)
{
    const auto resourceWidget = dynamic_cast<QnResourceWidget*>(widget);
    if(!resourceWidget)
        return; /* We may also get nullptr if the widget being rotated gets deleted. */

    resourceWidget->item()->setRotation(widget->rotation() - (resourceWidget->item()->data<bool>(Qn::ItemFlipRole, false) ? 180.0: 0.0));
}

void QnWorkbenchController::at_zoomRectChanged(QnMediaResourceWidget* widget, const QRectF& zoomRect)
{
    widget->item()->setZoomRect(zoomRect);
}

void QnWorkbenchController::at_zoomRectCreated(QnMediaResourceWidget* widget, const QColor& color, const QRectF& zoomRect)
{
    menu()->trigger(action::CreateZoomWindowAction, action::Parameters(widget)
        .withArgument(Qn::ItemZoomRectRole, zoomRect)
        .withArgument(Qn::ItemFrameDistinctionColorRole, color));
    widget->setCheckedButtons(widget->checkedButtons() & ~Qn::ZoomWindowButton);
}

void QnWorkbenchController::at_zoomTargetChanged(
    QnMediaResourceWidget* widget,
    const QRectF& zoomRect,
    QnMediaResourceWidget* zoomTargetWidget)
{
    const auto resource = zoomTargetWidget->resource()->toResourcePtr();
    if (!NX_ASSERT(resource))
        return;

    nx::vms::common::LayoutItemData existing = widget->item()->data();

    // Create new layout item inplace of existing one.
    nx::vms::common::LayoutItemData data = existing;
    data.uuid = QnUuid::createUuid();
    data.resource = descriptor(resource);
    data.zoomTargetUuid = zoomTargetWidget->item()->uuid();
    data.rotation = zoomTargetWidget->item()->rotation();
    data.zoomRect = zoomRect;
    data.frameDistinctionColor = widget->frameDistinctionColor();
    data.dewarpingParams = zoomTargetWidget->item()->dewarpingParams();
    // Zoom target must always be dewarped by 90 degrees.
    data.dewarpingParams.panoFactor = 1;
    // Zoom items on fisheye cameras must always be dewarped.
    data.dewarpingParams.enabled = zoomTargetWidget->resource()->getDewarpingParams().enabled;

    auto currentLayout = workbench()->currentLayoutResource();

    // Unpin existing item to free worbench grid place for the new item.
    existing.flags = 0;
    currentLayout->updateItem(existing);

    // Add a new item. Existing should be deleted later to avoid AV in this method processing.
    currentLayout->addItem(data);
    executeDelayedParented(
        [id = existing.uuid, currentLayout]
        {
            currentLayout->removeItem(id);
        }, this);
}

void QnWorkbenchController::at_motionSelectionProcessStarted(QGraphicsView* /*view*/,
    QnMediaResourceWidget* widget)
{
    if (!menu()->canTrigger(action::StartSmartSearchAction, widget))
        return;

    widget->setOption(QnResourceWidget::DisplayMotion, true);
}

void QnWorkbenchController::at_motionRegionCleared(QGraphicsView *, QnMediaResourceWidget *widget)
{
    widget->clearMotionSelection(false);
}

void QnWorkbenchController::at_motionRegionSelected(
    QGraphicsView* /*view*/,
    QnMediaResourceWidget* widget,
    const QRect& region)
{
    // This will send changed() because it was not sent on at_motionRegionCleared.
    widget->addToMotionSelection(region);
}

void QnWorkbenchController::at_item_leftPressed(QGraphicsView *view, QGraphicsItem *item, ClickInfo info)
{
    Q_UNUSED(view)

    if (info.modifiers != 0)
        return;

    if (workbench()->item(Qn::ZoomedRole))
        return; /* Don't change currently raised item if we're zoomed. It is surprising for the user. */

    QnResourceWidget *widget = item->isWidget() ? qobject_cast<QnResourceWidget *>(item->toGraphicsObject()) : nullptr;
    if (!widget)
        return;

    QnWorkbenchItem *workbenchItem = widget->item();

    if (workbench()->item(Qn::RaisedRole) != workbenchItem)
        workbench()->setItem(Qn::RaisedRole, nullptr);

    workbench()->setItem(Qn::ActiveRole, workbenchItem);
}

void QnWorkbenchController::at_item_leftClicked(
    QGraphicsView*,
    QGraphicsItem* item,
    ClickInfo info)
{
    if (info.modifiers != Qt::NoModifier)
        return;

    const auto resourceWidget = qobject_cast<QnResourceWidget*>(item->toGraphicsObject());
    if (!resourceWidget)
        return;

    // PTZ panning is performed, other modifications of resource widget are blocked.
    if (inViewportPtzMode(resourceWidget, info.modifiers))
        return;

    toggleRaisedState(resourceWidget);
}

void QnWorkbenchController::at_item_rightClicked(
    QGraphicsView* /*view*/,
    QGraphicsItem* item,
    ClickInfo /*info*/)
{
    QnResourceWidget *widget = item->isWidget() ? qobject_cast<QnResourceWidget *>(item->toGraphicsObject()) : nullptr;
    if(widget == nullptr)
        return;

    /* Right click does not select items.
     * However, we need to select the item under mouse for the menu to work as expected. */
    if(!widget->isSelected()) {
        widget->scene()->clearSelection();
        widget->setSelected(true);
    }
    showContextMenuAt(QCursor::pos());
}

void QnWorkbenchController::at_item_middleClicked(
    QGraphicsView* /*view*/,
    QGraphicsItem* item,
    ClickInfo /*info*/)
{
    if (tourIsRunning(context()))
        return;

    QnResourceWidget* widget = item->isWidget() ? qobject_cast<QnResourceWidget*>(item->toGraphicsObject()) : nullptr;
    if(widget == nullptr)
        return;

    if (auto resource = widget->resource())
    {
        if (!widget->isZoomWindow())
            menu()->triggerIfPossible(action::OpenInNewTabAction, resource);
    }
}

void QnWorkbenchController::at_item_doubleClicked(
    QGraphicsView*,
    QGraphicsItem* item,
    ClickInfo info)
{
    if (info.modifiers.testFlag(Qt::KeyboardModifier::ShiftModifier))
        return;

    const auto resourceWidget = qobject_cast<QnResourceWidget*>(item->toGraphicsObject());
    if (!resourceWidget)
        return;

    // PTZ zoom out is performed, other modifications of resource widget are blocked.
    if (inViewportPtzMode(resourceWidget, info.modifiers))
        return;

    at_resourceWidget_doubleClicked(resourceWidget);
}

void QnWorkbenchController::at_resourceWidget_doubleClicked(QnResourceWidget *widget)
{
    mainWindow()->scene()->clearSelection();
    widget->setSelected(true);

    if (workbench()->currentLayout()->isShowreelReviewLayout())
    {
        if (auto resource = widget->resource())
            menu()->triggerIfPossible(action::OpenInNewTabAction, resource);
        return;
    }

    QnWorkbenchItem *workbenchItem = widget->item();
    QnWorkbenchItem *zoomedItem = workbench()->item(Qn::ZoomedRole);
    if (zoomedItem == workbenchItem)
    {
        // Stop single Showreel if it is running.
        if (tourIsRunning(context()))
        {
            menu()->trigger(action::ToggleShowreelModeAction);
            return;
        }

        QRectF viewportGeometry = display()->viewportGeometry();
        QRectF zoomedItemGeometry = display()->itemGeometry(zoomedItem);

        // Magic const from v1.0
        static const qreal kOversizeCoeff{0.975};

        if (viewportGeometry.width() < zoomedItemGeometry.width() * kOversizeCoeff
            || viewportGeometry.height() < zoomedItemGeometry.height() *kOversizeCoeff)
        {
            workbench()->setItem(Qn::ZoomedRole, nullptr);
            workbench()->setItem(Qn::ZoomedRole, workbenchItem);
        }
        else
        {
            workbench()->setItem(Qn::ZoomedRole, nullptr);
        }
    }
    else if (!tourIsRunning(context()))
    {
        workbench()->setItem(Qn::ZoomedRole, workbenchItem);
    }
}

void QnWorkbenchController::at_scene_clicked(QGraphicsView* view, ClickInfo info)
{
    if (!NX_ASSERT(workbench()))
        return;

    if (info.button == Qt::LeftButton)
    {
        workbench()->setItem(Qn::RaisedRole, nullptr);
    }
    else
    {
        view->scene()->clearSelection(); //< Just to feel safe.
        showContextMenuAt(QCursor::pos());
    }
}

void QnWorkbenchController::at_scene_doubleClicked(QGraphicsView* /*view*/, ClickInfo /*info*/)
{
    if(workbench() == nullptr)
        return;

    workbench()->setItem(Qn::ZoomedRole, nullptr);
    display()->fitInView(display()->animationAllowed());
}

void QnWorkbenchController::at_display_widgetChanged(Qn::ItemRole role)
{
    QnResourceWidget *newWidget = display()->widget(role);
    QnResourceWidget *oldWidget = m_widgetByRole[role];
    if(newWidget == oldWidget)
        return;

    m_widgetByRole[role] = newWidget;

    QGraphicsItem *focusItem = mainWindow()->scene()->focusItem();
    bool canMoveFocus = !focusItem
        || dynamic_cast<QnResourceWidget*>(focusItem)
        || dynamic_cast<GraphicsWebEngineView*>(focusItem);

    // If the scene does not have input focus and a web page gets focused, the web page will draw
    // its content as focused, but won't be able to receive any input events. To prevent this,
    // never focus a web page if the scene itself does not have focus.
    const bool invalidFocus =
        qobject_cast<QnWebResourceWidget*>(newWidget) && !mainWindow()->scene()->hasFocus();

    if (newWidget && canMoveFocus && !invalidFocus)
        newWidget->setFocus(); /* Move focus only if it's not already grabbed by some control element. */

    switch(role) {
    case Qn::ZoomedRole:
        m_zoomedToggle->setActive(newWidget != nullptr);
        break;
    case Qn::RaisedRole:
    case Qn::CentralRole:
        if(newWidget) {
            m_cursorItem = newWidget->item();
            m_cursorPos = newWidget->item()->geometry().topLeft();
        }
        break;
    default:
        break;
    }
}

void QnWorkbenchController::at_display_widgetAdded(QnResourceWidget* widget)
{
    widget->installEventFilter(this);

    connect(widget, SIGNAL(rotationStartRequested()),   this,   SLOT(at_widget_rotationStartRequested()));
    connect(widget, SIGNAL(rotationStopRequested()),    this,   SLOT(at_widget_rotationStopRequested()));
}

void QnWorkbenchController::at_display_widgetAboutToBeRemoved(QnResourceWidget* widget)
{
    if(widget == m_resizedWidget)
        m_resizedWidget = nullptr;

    QnWorkbenchItem *item = widget->item();
    if (m_draggedWorkbenchItems.contains(item))
    {
        m_draggedWorkbenchItems.removeOne(item);
        display()->setDraggedItems(
            nx::utils::toQSet(m_draggedWorkbenchItems),
            /*updateGeometry*/ false);
        if (m_draggedWorkbenchItems.empty())
            m_moveInstrument->resetLater();
    }
    if (m_replacedWorkbenchItems.contains(item))
        m_replacedWorkbenchItems.removeOne(item);

    widget->removeEventFilter(this);
    widget->disconnect(this);
}

void QnWorkbenchController::at_widget_rotationStartRequested(QnResourceWidget* widget)
{
    m_rotationInstrument->start(mainWindow()->view(), widget);
}

void QnWorkbenchController::at_widget_rotationStartRequested()
{
    at_widget_rotationStartRequested(checked_cast<QnResourceWidget *>(sender()));
}

void QnWorkbenchController::at_widget_rotationStopRequested()
{
    m_rotationInstrument->reset();
}

void QnWorkbenchController::at_selectAllAction_triggered()
{
    foreach(QnResourceWidget *widget, display()->widgets())
        widget->setSelected(true);

    /* Move focus to scene if it's not there. */
    if (menu()->targetProvider() && menu()->targetProvider()->currentScope() != action::SceneScope)
        mainWindow()->scene()->setFocusItem(nullptr);
}

void QnWorkbenchController::at_stopSmartSearchAction_triggered()
{
    displayMotionGrid(menu()->currentParameters(sender()).widgets(), false);
}

void QnWorkbenchController::at_startSmartSearchAction_triggered() {
    displayMotionGrid(menu()->currentParameters(sender()).widgets(), true);
}

void QnWorkbenchController::at_checkFileSignatureAction_triggered()
{
    QnResourceWidgetList widgets = menu()->currentParameters(sender()).widgets();
    if (widgets.isEmpty())
        return;
    auto widget = widgets.at(0);
    if(widget->resource()->flags() & Qn::network)
        return;
    QScopedPointer<SignDialog> dialog(new SignDialog(widget->resource(), mainWindowWidget()));
    dialog->exec();
}

void QnWorkbenchController::at_nextItemAction_triggered()
{
    // Move right. If cursor reaches end of the row, move down.
    moveCursor(QPoint(1, 0), QPoint(0, 1));
}

void QnWorkbenchController::at_previousItemAction_triggered()
{
    // Move left. If cursor reaches start of the row, move up.
    moveCursor(QPoint(-1, 0), QPoint(0, -1));
}

void QnWorkbenchController::at_nextRowAction_triggered()
{
    // Move down. If cursor reaches end of the column, move right.
    moveCursor(QPoint(0, 1), QPoint(1, 0));
}

void QnWorkbenchController::at_previousRowAction_triggered()
{
    // Move up. If cursor reaches start of the column, move left.
    moveCursor(QPoint(0, -1), QPoint(-1, 0));
}

void QnWorkbenchController::at_riseCurrentItemAction_triggered()
{
    toggleRaisedState(
        qobject_cast<QnMediaResourceWidget*>(display()->widget(Qn::CentralRole)));
}

void QnWorkbenchController::at_toggleSmartSearchAction_triggered()
{
    QnResourceWidgetList widgets = menu()->currentParameters(sender()).widgets();

    for (auto widget: widgets)
    {
        if (!widget->resource()->hasFlags(Qn::motion))
            continue;

        if (widget->isZoomWindow())
            continue;

        if(!(widget->options() & QnResourceWidget::DisplayMotion))
        {
            at_startSmartSearchAction_triggered();
            return;
        }
    }
    at_stopSmartSearchAction_triggered();
}

void QnWorkbenchController::at_clearMotionSelectionAction_triggered()
{
    QnResourceWidgetList widgets = menu()->currentParameters(sender()).widgets();

    for (auto widget: widgets)
    {
        if (auto mediaWidget = dynamic_cast<QnMediaResourceWidget*>(widget))
            mediaWidget->clearMotionSelection();
    }
}

void QnWorkbenchController::at_toggleInfoAction_triggered()
{
    QnResourceWidgetList widgets = menu()->currentParameters(sender()).widgets();

    bool visible = true;
    for (QnResourceWidget* widget: widgets)
    {
        if (!widget->options().testFlag(QnResourceWidget::DisplayInfo))
        {
            visible = false;
            break;
        }
    }

    const bool animate = display()->animationAllowed();
    for (QnResourceWidget* widget: widgets)
        widget->setInfoVisible(!visible, animate);
}


void QnWorkbenchController::at_maximizeItemAction_triggered()
{
    QnResourceWidgetList widgets = menu()->currentParameters(sender()).widgets();
    if(widgets.empty())
        return;

    workbench()->setItem(Qn::ZoomedRole, widgets[0]->item());
}

void QnWorkbenchController::at_unmaximizeItemAction_triggered()
{
    workbench()->setItem(Qn::ZoomedRole, nullptr);
}

void QnWorkbenchController::at_fitInViewAction_triggered()
{
    display()->fitInView(display()->animationAllowed());
}

void QnWorkbenchController::at_workbench_currentLayoutAboutToBeChanged()
{
    if (const auto layout = workbench()->currentLayout()->resource())
        layout->disconnect(this);
}

void QnWorkbenchController::at_workbench_currentLayoutChanged()
{
    if (const auto layout = workbench()->currentLayout()->resource())
    {
        connect(layout.get(), &QnLayoutResource::lockedChanged, this,
            &QnWorkbenchController::updateCurrentLayoutInstruments);
    }
    updateCurrentLayoutInstruments();
}

void QnWorkbenchController::at_accessController_permissionsChanged(const QnResourcePtr& resource)
{
    if (workbench()->currentLayout()->resource() == resource)
        updateCurrentLayoutInstruments();
}

void QnWorkbenchController::at_zoomedToggle_activated() {
    m_handScrollInstrument->setMouseButtons(Qt::LeftButton | Qt::RightButton);
}

void QnWorkbenchController::at_zoomedToggle_deactivated() {
    m_handScrollInstrument->setMouseButtons(Qt::RightButton);
}

void QnWorkbenchController::updateCurrentLayoutInstruments()
{
    const auto layout = workbench()->currentLayout();
    const auto resource = layout->resource();

    const auto permissions = ResourceAccessManager::permissions(resource);
    const auto writable = permissions.testFlag(Qn::WritePermission);

    m_moveInstrument->setEnabled(writable && !resource->locked());
    m_resizingInstrument->setEnabled(writable && !resource->locked()
        && !layout->flags().testFlag(QnLayoutFlag::NoResize));

    const auto fixedViewport = layout->flags().testFlag(QnLayoutFlag::FixedViewport);
    m_wheelZoomInstrument->setEnabled(!fixedViewport);
    m_handScrollInstrument->setEnabled(!fixedViewport);
    m_gridAdjustmentInstrument->setEnabled(!(fixedViewport || layout->locked()));
}

void QnWorkbenchController::at_ptzProcessStarted(QnMediaResourceWidget* widget)
{
    if(widget == nullptr)
        return;

    // skip method if current widget is already raised or zoomed
    if (display()->widget(Qn::RaisedRole) == widget
        || display()->widget(Qn::ZoomedRole) == widget)
        return;

    workbench()->setItem(Qn::RaisedRole, nullptr); /* Un-raise currently raised item so that it doesn't interfere with ptz. */
    mainWindow()->scene()->clearSelection();
    widget->setSelected(true);
    display()->bringToFront(widget);
}

void QnWorkbenchController::toggleRaisedState(QnResourceWidget* widget)
{
    if (workbench()->item(Qn::ZoomedRole))
        return; //< Don't change currently raised item if we're zoomed. It is surprising for the user.

    if (!widget)
        return;

    const auto workbenchItem = widget->item();

    const bool canRaise = !workbench()->currentLayout()->flags().testFlag(QnLayoutFlag::NoResize);
    if (canRaise && workbench()->item(Qn::RaisedRole) != workbenchItem)
    {
        // Don't raise if there's only one item in the layout.
        QRectF occupiedGeometry = widget->geometry();
        occupiedGeometry = Geometry::dilated(
            occupiedGeometry, occupiedGeometry.size() * raisedGeometryThreshold);

        if (occupiedGeometry.contains(display()->raisedGeometry(widget->geometry(), widget->rotation())))
            workbench()->setItem(Qn::RaisedRole, nullptr);
        else
            workbench()->setItem(Qn::RaisedRole, workbenchItem);
    }
    else
    {
        workbench()->setItem(Qn::RaisedRole, nullptr);
    }
}

void QnWorkbenchController::toggleCurrentItemMaximizationState()
{
    QnResourceWidget *widget = display()->widget(Qn::CentralRole);
    if (widget && widget == display()->widget(Qn::ZoomedRole))
    {
        menu()->triggerIfPossible(action::UnmaximizeItemAction, widget);
    }
    else
    {
        menu()->triggerIfPossible(action::MaximizeItemAction, widget);
    }
}
