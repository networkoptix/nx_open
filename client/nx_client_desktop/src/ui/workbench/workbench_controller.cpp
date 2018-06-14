#include "workbench_controller.h"

#include <cassert>
#include <cmath> /* For std::floor. */
#include <limits>

#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsView>
#include <QtOpenGL/QGLWidget>
#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QAction>
#include <QtWidgets/QMenu>
#include <QtWidgets/QLabel>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QFileInfo>
#include <QtCore/QSettings>

#include <client/client_runtime_settings.h>

#include <utils/common/util.h>
#include <utils/common/checked_cast.h>
#include <utils/common/delete_later.h>
#include <utils/common/toggle.h>
#include <nx/utils/log/log.h>
#include <utils/math/color_transformations.h>
#include <utils/resource_property_adaptors.h>

#include <core/resource/resource_directory_browser.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_item_index.h>

#include <camera/resource_display.h>
#include <camera/cam_display.h>

#include <plugins/resource/desktop_camera/desktop_resource_base.h>

#include "client/client_settings.h"

#include <ui/style/globals.h>
#include <ui/style/skin.h>

#include "ui/dialogs/sign_dialog.h" // TODO: move out.
#include <ui/dialogs/common/custom_file_dialog.h>  //for QnCustomFileDialog::fileDialogOptions() constant
#include <ui/dialogs/common/file_dialog.h>

#include <ui/animation/viewport_animator.h>
#include <ui/animation/animator_group.h>
#include <ui/animation/opacity_animator.h>

#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/instruments/wheel_zoom_instrument.h>
#include <ui/graphics/instruments/rubber_band_instrument.h>
#include <ui/graphics/instruments/move_instrument.h>
#include <ui/graphics/instruments/rotation_instrument.h>
#include <ui/graphics/instruments/click_instrument.h>
#include <ui/graphics/instruments/bounding_instrument.h>
#include <ui/graphics/instruments/stop_instrument.h>
#include <ui/graphics/instruments/stop_accepted_instrument.h>
#include <ui/graphics/instruments/forwarding_instrument.h>
#include <ui/graphics/instruments/transform_listener_instrument.h>
#include <ui/graphics/instruments/selection_fixup_instrument.h>
#include <ui/graphics/instruments/drop_instrument.h>
#include <ui/graphics/instruments/drag_instrument.h>
#include <ui/graphics/instruments/resizing_instrument.h>
#include <ui/graphics/instruments/signaling_instrument.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/instruments/animation_instrument.h>
#include <ui/graphics/instruments/selection_overlay_hack_instrument.h>
#include <ui/graphics/instruments/grid_adjustment_instrument.h>
#include <ui/graphics/instruments/ptz_instrument.h>
#include <ui/graphics/instruments/zoom_window_instrument.h>

#include <ui/graphics/items/grid/grid_item.h>
#include <ui/graphics/items/generic/graphics_message_box.h>
#include <ui/graphics/items/resource/button_ids.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/standard/graphics_web_view.h>

#include <ui/help/help_handler.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/ui/actions/action_target_provider.h>

#include <ui/widgets/main_window.h>
#include <ui/workaround/hidpi_workarounds.h>

#include "workbench_layout.h"
#include "workbench_item.h"
#include "workbench_grid_mapper.h"
#include "workbench_utility.h"
#include "workbench_context.h"
#include "workbench.h"
#include "workbench_display.h"
#include "workbench_access_controller.h"

#include <plugins/io_device/joystick/joystick_manager.h>

#include <utils/common/delayed.h>
#include <nx/client/ptz/ptz_hotkey_resource_property_adaptor.h>

using namespace nx::client::desktop::ui;

//#define QN_WORKBENCH_CONTROLLER_DEBUG
#ifdef QN_WORKBENCH_CONTROLLER_DEBUG
#   define TRACE(...) qDebug() << __VA_ARGS__;
#else
#   define TRACE(...)
#endif

namespace {

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
    return context->action(action::ToggleLayoutTourModeAction)->isChecked();
}

} // namespace

QnWorkbenchController::QnWorkbenchController(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_manager(display()->instrumentManager()),
    m_selectionOverlayHackInstrumentDisabled(false),
    m_selectionOverlayHackInstrumentDisabled2(false),
    m_cursorPos(invalidCursorPos()),
    m_resizedWidget(NULL),
    m_dragDelta(invalidDragDelta()),
    m_menuEnabled(true)
{
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
    SelectionOverlayHackInstrument *selectionOverlayHackInstrument = display()->selectionOverlayHackInstrument();
    m_moveInstrument = new MoveInstrument(this);
    m_itemMouseForwardingInstrument = new ForwardingInstrument(Instrument::Item, mouseEventTypes, this);
    SelectionFixupInstrument *selectionFixupInstrument = new SelectionFixupInstrument(this);
    m_motionSelectionInstrument = new MotionSelectionInstrument(this);

    m_gridAdjustmentInstrument = new GridAdjustmentInstrument(workbench(), this);
    m_gridAdjustmentInstrument->setSpeed(0.25 / 360.0);
    m_gridAdjustmentInstrument->setMaxSpacing(0.15);

    SignalingInstrument *sceneKeySignalingInstrument = new SignalingInstrument(Instrument::Scene, Instrument::makeSet(QEvent::KeyPress), this);
    SignalingInstrument *sceneFocusSignalingInstrument = new SignalingInstrument(Instrument::Scene, Instrument::makeSet(QEvent::FocusIn), this);
    PtzInstrument *ptzInstrument = new PtzInstrument(this);
    m_zoomWindowInstrument = new ZoomWindowInstrument(this);


    m_motionSelectionInstrument->setBrush(subColor(qnGlobals->mrsColor(), qnGlobals->selectionOpacityDelta()));
    m_motionSelectionInstrument->setPen(subColor(qnGlobals->mrsColor(), qnGlobals->selectionBorderDelta()));
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
            return widget && !widget->options().testFlag(QnResourceWidget::WindowRotationForbidden);
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

    /* View/viewport instruments. */
    m_manager->installInstrument(m_rotationInstrument);
    m_manager->installInstrument(m_handScrollInstrument);
    m_manager->installInstrument(m_resizingInstrument);
    m_manager->installInstrument(m_moveInstrument);
    m_manager->installInstrument(m_dragInstrument);
    m_manager->installInstrument(m_rubberBandInstrument);
    m_manager->installInstrument(m_motionSelectionInstrument);
    m_manager->installInstrument(m_zoomWindowInstrument);
    m_manager->installInstrument(ptzInstrument);

    display()->setLayer(m_dropInstrument->surface(), QnWorkbenchDisplay::BackLayer);

    connect(m_itemLeftClickInstrument,  SIGNAL(pressed(QGraphicsView *, QGraphicsItem *, const ClickInfo &)),                       this,                           SLOT(at_item_leftPressed(QGraphicsView *, QGraphicsItem *, const ClickInfo &)));
    connect(m_itemLeftClickInstrument,  SIGNAL(clicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)),                       this,                           SLOT(at_item_leftClicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)));
    connect(m_itemLeftClickInstrument,  SIGNAL(doubleClicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)),                 this,                           SLOT(at_item_doubleClicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)));
    connect(ptzInstrument,              SIGNAL(doubleClicked(QnMediaResourceWidget *)),                                             this,                           SLOT(at_item_doubleClicked(QnMediaResourceWidget *)));
    connect(m_itemRightClickInstrument, SIGNAL(clicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)),                       this,                           SLOT(at_item_rightClicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)));
    connect(itemMiddleClickInstrument,  SIGNAL(clicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)),                       this,                           SLOT(at_item_middleClicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)));
    connect(m_sceneClickInstrument,     SIGNAL(clicked(QGraphicsView *, const ClickInfo &)),                                        this,                           SLOT(at_scene_clicked(QGraphicsView *, const ClickInfo &)));
    connect(m_sceneClickInstrument,     SIGNAL(doubleClicked(QGraphicsView *, const ClickInfo &)),                                  this,                           SLOT(at_scene_doubleClicked(QGraphicsView *, const ClickInfo &)));
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
    connect(sceneKeySignalingInstrument, SIGNAL(activated(QGraphicsScene *, QEvent *)),                                             this,                           SLOT(at_scene_keyPressed(QGraphicsScene *, QEvent *)));
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

    connect(m_rubberBandInstrument,     SIGNAL(rubberBandStarted(QGraphicsView *)),                                                 selectionOverlayHackInstrument, SLOT(recursiveDisable()));
    connect(m_rubberBandInstrument,     SIGNAL(rubberBandFinished(QGraphicsView *)),                                                selectionOverlayHackInstrument, SLOT(recursiveEnable()));

    connect(m_dragInstrument,           SIGNAL(dragProcessStarted(QGraphicsView *)),                                                m_moveInstrument,               SLOT(recursiveDisable()));
    connect(m_dragInstrument,           SIGNAL(dragProcessFinished(QGraphicsView *)),                                               m_moveInstrument,               SLOT(recursiveEnable()));

    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),         m_moveInstrument,               SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),        m_moveInstrument,               SLOT(recursiveEnable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),         m_dragInstrument,               SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),        m_dragInstrument,               SLOT(recursiveEnable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),         m_rubberBandInstrument,         SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),        m_rubberBandInstrument,         SLOT(recursiveEnable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),         ptzInstrument,                  SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),        ptzInstrument,                  SLOT(recursiveEnable()));
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
    connect(m_rotationInstrument,       SIGNAL(rotationProcessStarted(QGraphicsView *, QGraphicsWidget *)),                         ptzInstrument,                  SLOT(recursiveDisable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessFinished(QGraphicsView *, QGraphicsWidget *)),                        ptzInstrument,                  SLOT(recursiveEnable()));
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

    connect(ptzInstrument, &PtzInstrument::ptzProcessStarted, m_handScrollInstrument, &Instrument::recursiveDisable);
    connect(ptzInstrument, &PtzInstrument::ptzProcessFinished, m_handScrollInstrument, &Instrument::recursiveEnable);
    connect(ptzInstrument, &PtzInstrument::ptzProcessStarted, m_moveInstrument, &Instrument::recursiveDisable);
    connect(ptzInstrument, &PtzInstrument::ptzProcessFinished, m_moveInstrument, &Instrument::recursiveEnable);
    connect(ptzInstrument, &PtzInstrument::ptzProcessStarted, m_motionSelectionInstrument, &Instrument::recursiveDisable);
    connect(ptzInstrument, &PtzInstrument::ptzProcessFinished, m_motionSelectionInstrument, &Instrument::recursiveEnable);
    connect(ptzInstrument, &PtzInstrument::ptzProcessStarted, m_itemLeftClickInstrument, &Instrument::recursiveDisable);
    connect(ptzInstrument, &PtzInstrument::ptzProcessFinished, m_itemLeftClickInstrument, &Instrument::recursiveEnable);
    connect(ptzInstrument, &PtzInstrument::ptzProcessStarted, m_resizingInstrument, &Instrument::recursiveDisable);
    connect(ptzInstrument, &PtzInstrument::ptzProcessFinished, m_resizingInstrument, &Instrument::recursiveEnable);
    connect(ptzInstrument, &PtzInstrument::ptzProcessStarted, this, &QnWorkbenchController::at_ptzProcessStarted);

    connect(m_zoomWindowInstrument,       SIGNAL(zoomWindowProcessStarted(QnMediaResourceWidget *)),                                  m_handScrollInstrument,         SLOT(recursiveDisable()));
    connect(m_zoomWindowInstrument,       SIGNAL(zoomWindowProcessFinished(QnMediaResourceWidget *)),                                 m_handScrollInstrument,         SLOT(recursiveEnable()));
    connect(m_zoomWindowInstrument,       SIGNAL(zoomWindowProcessStarted(QnMediaResourceWidget *)),                                  m_moveInstrument,               SLOT(recursiveDisable()));
    connect(m_zoomWindowInstrument,       SIGNAL(zoomWindowProcessFinished(QnMediaResourceWidget *)),                                 m_moveInstrument,               SLOT(recursiveEnable()));
    connect(m_zoomWindowInstrument,       SIGNAL(zoomWindowProcessStarted(QnMediaResourceWidget *)),                                  m_motionSelectionInstrument,    SLOT(recursiveDisable()));
    connect(m_zoomWindowInstrument,       SIGNAL(zoomWindowProcessFinished(QnMediaResourceWidget *)),                                 m_motionSelectionInstrument,    SLOT(recursiveEnable()));
    connect(m_zoomWindowInstrument,       SIGNAL(zoomWindowProcessStarted(QnMediaResourceWidget *)),                                  m_itemLeftClickInstrument,      SLOT(recursiveDisable()));
    connect(m_zoomWindowInstrument,       SIGNAL(zoomWindowProcessFinished(QnMediaResourceWidget *)),                                 m_itemLeftClickInstrument,      SLOT(recursiveEnable()));
    connect(m_zoomWindowInstrument,       SIGNAL(zoomWindowProcessStarted(QnMediaResourceWidget *)),                                  ptzInstrument,                  SLOT(recursiveDisable()));
    connect(m_zoomWindowInstrument,       SIGNAL(zoomWindowProcessFinished(QnMediaResourceWidget *)),                                 ptzInstrument,                  SLOT(recursiveEnable()));
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
    m_zoomedToggle->setActive(display()->widget(Qn::ZoomedRole) != NULL);

    /* Set up context menu. */
    QWidget *window = display()->view()->window();
    window->addAction(action(action::ToggleSmartSearchAction));
    window->addAction(action(action::ToggleInfoAction));

    const auto screenRecordingAction = action(action::ToggleScreenRecordingAction);
    if (screenRecordingAction)
        window->addAction(screenRecordingAction);

    connect(action(action::SelectAllAction), SIGNAL(triggered()),                                                                       this,                           SLOT(at_selectAllAction_triggered()));
    connect(action(action::StartSmartSearchAction), SIGNAL(triggered()),                                                                this,                           SLOT(at_startSmartSearchAction_triggered()));
    connect(action(action::StopSmartSearchAction), SIGNAL(triggered()),                                                                 this,                           SLOT(at_stopSmartSearchAction_triggered()));
    connect(action(action::ToggleSmartSearchAction), SIGNAL(triggered()),                                                               this,                           SLOT(at_toggleSmartSearchAction_triggered()));
    connect(action(action::ClearMotionSelectionAction), SIGNAL(triggered()),                                                            this,                           SLOT(at_clearMotionSelectionAction_triggered()));
    connect(action(action::ShowInfoAction), SIGNAL(triggered()),                                                                        this,                           SLOT(at_showInfoAction_triggered()));
    connect(action(action::HideInfoAction), SIGNAL(triggered()),                                                                        this,                           SLOT(at_hideInfoAction_triggered()));
    connect(action(action::ToggleInfoAction), SIGNAL(triggered()),                                                                      this,                           SLOT(at_toggleInfoAction_triggered()));
    connect(action(action::CheckFileSignatureAction), SIGNAL(triggered()),                                                              this,                           SLOT(at_checkFileSignatureAction_triggered()));
    connect(action(action::MaximizeItemAction), SIGNAL(triggered()),                                                                    this,                           SLOT(at_maximizeItemAction_triggered()));
    connect(action(action::UnmaximizeItemAction), SIGNAL(triggered()),                                                                  this,                           SLOT(at_unmaximizeItemAction_triggered()));
    connect(action(action::FitInViewAction), SIGNAL(triggered()),                                                                       this,                           SLOT(at_fitInViewAction_triggered()));
    connect(action(action::ToggleLayoutTourModeAction), &QAction::toggled, this,
        [this](bool on)
        {
            m_motionSelectionInstrument->setEnabled(!on);
        });

    connect(accessController(), &QnWorkbenchAccessController::permissionsChanged, this,
        &QnWorkbenchController::at_accessController_permissionsChanged);

    connect(
        action(action::GoToNextItemAction), &QAction::triggered,
        this, &QnWorkbenchController::at_nextItemAction_triggered);

    connect(
        action(action::GoToPreviousItemAction), &QAction::triggered,
        this, &QnWorkbenchController::at_previousItemAction_triggered);

    connect(action(action::ToggleCurrentItemMaximizationStateAction), &QAction::triggered, this,
        &QnWorkbenchController::toggleCurrentItemMaximizationState);

    updateCurrentLayoutInstruments();
}

QnWorkbenchGridMapper *QnWorkbenchController::mapper() const {
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
                display()->scene()->clearSelection();

            menu()->trigger(action::RemoveLayoutItemFromSceneAction, widget);
            event->ignore();
            return true;
        }
    }

    return QObject::eventFilter(watched, event);
}

void QnWorkbenchController::updateGeometryDelta(QnResourceWidget *widget) {
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

void QnWorkbenchController::displayMotionGrid(const QList<QnResourceWidget *> &widgets, bool display) {
    foreach(QnResourceWidget *widget, widgets) {
        if(!widget->resource()->hasFlags(Qn::motion))
            continue;
        if (widget->isZoomWindow())
            continue;
        widget->setOption(QnResourceWidget::DisplayMotion, display);
    }
}

void QnWorkbenchController::displayWidgetInfo(const QList<QnResourceWidget *> &widgets, bool visible)
{
    const bool animate = display()->animationAllowed();
    foreach(QnResourceWidget *widget, widgets)
        widget->setInfoVisible(visible, animate);
}

void QnWorkbenchController::moveCursor(const QPoint &aAxis, const QPoint &bAxis) {
    QnWorkbenchItem *centerItem = m_cursorItem.data();
    if(!centerItem)
        centerItem = workbench()->currentLayout()->item(m_cursorPos);

    QPoint center = m_cursorPos;
    if(centerItem && !centerItem->geometry().contains(center))
        center = centerItem->geometry().topLeft();

    QRect boundingRect = workbench()->currentLayout()->boundingRect();
    if(boundingRect.isEmpty())
        return;

    QPoint aReturn = -aAxis * qAbs(dot(toPoint(boundingRect.size()), aAxis) / dot(aAxis, aAxis));
    QPoint bReturn = -bAxis * qAbs(dot(toPoint(boundingRect.size()), bAxis) / dot(bAxis, bAxis));

    QPoint pos = center;
    QnWorkbenchItem *item = NULL;
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

    display()->scene()->clearSelection();
    display()->widget(item)->setSelected(true);
    workbench()->setItem(role, item);
    m_cursorPos = pos;
    m_cursorItem = item;
}

void QnWorkbenchController::showContextMenuAt(const QPoint &pos)
{
    if (!m_menuEnabled)
        return;

    WeakGraphicsItemPointerList items(display()->scene()->selectedItems());
    executeDelayedParented([this, pos, items]()
        {
            QScopedPointer<QMenu> menu(this->menu()->newMenu(action::SceneScope, nullptr,
                items.materialized()));
            if (menu->isEmpty())
                return;

            QnHiDpiWorkarounds::showMenu(menu.data(), pos);
        }, this);
}

void QnWorkbenchController::updateDraggedItems()
{
    display()->setDraggedItems(m_draggedWorkbenchItems.toSet());
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //

void QnWorkbenchController::at_scene_keyPressed(QGraphicsScene *, QEvent *event)
{
    if (event->type() != QEvent::KeyPress)
        return;

    event->accept(); /* Accept by default. */

    QKeyEvent *e = static_cast<QKeyEvent *>(event);

    auto w = qobject_cast<MainWindow*>(mainWindow());
    NX_EXPECT(w);
    if (w && w->handleKeyPress(e->key()))
        return;

    switch (e->key())
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
            else
                moveCursor(QPoint(0, -1), QPoint(-1, 0));
            break;
        }
        case Qt::Key_Down:
        {
            if (e->modifiers() & Qt::AltModifier)
                m_handScrollInstrument->emulate(QPoint(0, 15));
            else
                moveCursor(QPoint(0, 1), QPoint(1, 0));
            break;
        }
        case Qt::Key_Left:
        {
            if (e->modifiers() & Qt::AltModifier)
                m_handScrollInstrument->emulate(QPoint(-15, 0));
            else
                moveCursor(QPoint(-1, 0), QPoint(0, -1));
            break;
        }
        case Qt::Key_Right:
        {
            if (e->modifiers() & Qt::AltModifier)
                m_handScrollInstrument->emulate(QPoint(15, 0));
            else
                moveCursor(QPoint(1, 0), QPoint(0, 1));
            break;
        }
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            m_wheelZoomInstrument->emulate(30);
            break;
        case Qt::Key_Minus:
            m_wheelZoomInstrument->emulate(-30);
            break;
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            break;
        case Qt::Key_Menu:
        {
            QGraphicsView *view = display()->view();
            QList<QGraphicsItem *> items = display()->scene()->selectedItems();
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

            int hotkey = e->key() - Qt::Key_0;

            nx::client::core::ptz::PtzHotkeysResourcePropertyAdaptor adaptor;
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

void QnWorkbenchController::at_scene_focusIn(QGraphicsScene *, QEvent *event) {
    // TODO: #Elric evil hack to prevent focus jumps when scene is focused.
    QFocusEvent *focusEvent = static_cast<QFocusEvent *>(event);
    *focusEvent = QFocusEvent(focusEvent->type(), Qt::OtherFocusReason);
}

void QnWorkbenchController::at_resizingStarted(QGraphicsView *, QGraphicsWidget *item, ResizingInfo *info) {
    TRACE("RESIZING STARTED");

    m_resizedWidget = qobject_cast<QnResourceWidget *>(item);
    if(m_resizedWidget == NULL)
        return;

    if(m_resizedWidget->item() == NULL)
        return;

    m_selectionOverlayHackInstrumentDisabled = true;
    display()->selectionOverlayHackInstrument()->recursiveDisable();

    workbench()->setItem(Qn::RaisedRole, NULL); /* Un-raise currently raised item so that it doesn't interfere with resizing. */

    display()->bringToFront(m_resizedWidget);
    opacityAnimator(display()->gridItem())->animateTo(1.0);
    opacityAnimator(m_resizedWidget)->animateTo(widgetManipulationOpacity);

    /* Calculate snap point */
    Qt::WindowFrameSection grabbedSection = Qn::rotateSection(info->frameSection(), item->rotation());
    QRect initialGeometry = m_resizedWidget->item()->geometry();
    QRect widgetInitialGeometry = mapper()->mapToGrid(rotated(m_resizedWidget->geometry(), m_resizedWidget->rotation()));
    m_resizingSnapPoint = Qn::calculatePinPoint(initialGeometry.intersected(widgetInitialGeometry), grabbedSection);
}

void QnWorkbenchController::at_resizing(QGraphicsView *, QGraphicsWidget *item, ResizingInfo *info) {
    if(m_resizedWidget != item || item == NULL)
        return;

    QRectF widgetGeometry = rotated(m_resizedWidget->geometry(), m_resizedWidget->rotation());

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

void QnWorkbenchController::at_resizingFinished(QGraphicsView *, QGraphicsWidget *item, ResizingInfo *) {
    TRACE("RESIZING FINISHED");

    opacityAnimator(display()->gridItem())->animateTo(0.0);
    if (m_resizedWidget)
        opacityAnimator(m_resizedWidget)->animateTo(1.0);

    if(m_resizedWidget == item && item != NULL) {
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
            workbench()->setItem(Qn::RaisedRole, NULL);
    }

    /* Clean up resizing state. */
    display()->gridItem()->setCellState(m_resizedWidgetRect, QnGridItem::Initial);
    m_resizedWidgetRect = QRect();
    m_resizedWidget = NULL;
    if(m_selectionOverlayHackInstrumentDisabled) {
        display()->selectionOverlayHackInstrument()->recursiveEnable();
        m_selectionOverlayHackInstrumentDisabled = false;
    }
}

void QnWorkbenchController::at_moveStarted(QGraphicsView *, const QList<QGraphicsItem *> &items)
{
    TRACE("MOVE STARTED");

    /* Build item lists. */
    for (QGraphicsItem *item: items)
    {
        QnResourceWidget *widget = item->isWidget() ? qobject_cast<QnResourceWidget *>(item->toGraphicsObject()) : NULL;
        if (!widget)
            continue;

        m_draggedWorkbenchItems.push_back(widget->item());
        opacityAnimator(widget)->animateTo(widgetManipulationOpacity);
        updateDraggedItems();
    }

    if (m_draggedWorkbenchItems.empty())
        return;

    m_selectionOverlayHackInstrumentDisabled2 = true;
    display()->selectionOverlayHackInstrument()->recursiveDisable();

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

        m_replacedWorkbenchItems = layout->items(replacedGeometries).subtract(m_draggedWorkbenchItems.toSet()).toList();

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

void QnWorkbenchController::at_moveFinished(QGraphicsView *, const QList<QGraphicsItem *> &) {
    TRACE("MOVE FINISHED");

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
        if(raisedItem != NULL && workbenchItems.contains(raisedItem))
            workbench()->setItem(Qn::RaisedRole, NULL);
    }

    /* Clean up dragging state. */
    display()->gridItem()->setCellState(m_dragGeometries, QnGridItem::Initial);
    m_dragDelta = invalidDragDelta();
    m_draggedWorkbenchItems.clear();
    m_replacedWorkbenchItems.clear();
    m_dragGeometries.clear();
    updateDraggedItems();

    if(m_selectionOverlayHackInstrumentDisabled2) {
        display()->selectionOverlayHackInstrument()->recursiveEnable();
        m_selectionOverlayHackInstrumentDisabled2 = false;
    }
}

void QnWorkbenchController::at_rotationStarted(QGraphicsView *, QGraphicsWidget *widget) {
    TRACE("ROTATION STARTED");

    display()->bringToFront(widget);
}

void QnWorkbenchController::at_rotationFinished(QGraphicsView *, QGraphicsWidget *widget) {
    TRACE("ROTATION FINISHED");

    const auto resourceWidget = dynamic_cast<QnResourceWidget*>(widget);
    if(!resourceWidget)
        return; /* We may also get NULL if the widget being rotated gets deleted. */

    resourceWidget->item()->setRotation(widget->rotation() - (resourceWidget->item()->data<bool>(Qn::ItemFlipRole, false) ? 180.0: 0.0));
}

void QnWorkbenchController::at_zoomRectChanged(QnMediaResourceWidget *widget, const QRectF &zoomRect) {
    widget->item()->setZoomRect(zoomRect);
}

void QnWorkbenchController::at_zoomRectCreated(QnMediaResourceWidget *widget, const QColor &color, const QRectF &zoomRect) {
    menu()->trigger(action::CreateZoomWindowAction, action::Parameters(widget)
        .withArgument(Qn::ItemZoomRectRole, zoomRect)
        .withArgument(Qn::ItemFrameDistinctionColorRole, color));
    widget->setCheckedButtons(widget->checkedButtons() & ~Qn::ZoomWindowButton);
}

void QnWorkbenchController::at_zoomTargetChanged(QnMediaResourceWidget *widget, const QRectF &zoomRect, QnMediaResourceWidget *zoomTargetWidget) {
    QnLayoutItemData data = widget->item()->data();
    delete widget;

    const auto resource = zoomTargetWidget->resource()->toResourcePtr();
    NX_EXPECT(resource);
    if (!resource)
        return;

    data.uuid = QnUuid::createUuid();
    data.resource.id = resource->getId();
    if (resource->hasFlags(Qn::local_media))
        data.resource.uniqueId = resource->getUniqueId();
    data.zoomTargetUuid = zoomTargetWidget->item()->uuid();
    data.rotation = zoomTargetWidget->item()->rotation();
    data.zoomRect = zoomRect;
    data.dewarpingParams = zoomTargetWidget->item()->dewarpingParams();
    data.dewarpingParams.panoFactor = 1; // zoom target must always be dewarped by 90 degrees
    data.dewarpingParams.enabled = zoomTargetWidget->resource()->getDewarpingParams().enabled;  // zoom items on fisheye cameras must always be dewarped

    QnLayoutResourcePtr layout = workbench()->currentLayout()->resource();
    if (!layout)
        return;
    if (layout->getItems().size() >= qnRuntime->maxSceneItems())
        return;
    layout->addItem(data);
}

void QnWorkbenchController::at_motionSelectionProcessStarted(QGraphicsView* /*view*/,
    QnMediaResourceWidget* widget)
{
    if (!menu()->canTrigger(action::StartSmartSearchAction, widget))
        return;

    widget->setOption(QnResourceWidget::DisplayMotion, true);
}

void QnWorkbenchController::at_motionRegionCleared(QGraphicsView *, QnMediaResourceWidget *widget) {
    widget->clearMotionSelection();
}

void QnWorkbenchController::at_motionRegionSelected(QGraphicsView *, QnMediaResourceWidget *widget, const QRect &region)
{
    if (region.isEmpty())
        return;

    widget->addToMotionSelection(region);

    for (auto otherWidget: display()->widgets())
    {
        if (otherWidget != widget)
        {
            if (auto otherMediaWidget = dynamic_cast<QnMediaResourceWidget*>(otherWidget))
                otherMediaWidget->clearMotionSelection();
        }
    }
}

void QnWorkbenchController::at_item_leftPressed(QGraphicsView *view, QGraphicsItem *item, const ClickInfo &info)
{
    Q_UNUSED(view)

    TRACE("ITEM LPRESSED");

    if (info.modifiers() != 0)
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

void QnWorkbenchController::at_item_leftClicked(QGraphicsView *, QGraphicsItem *item, const ClickInfo &info)
{
    TRACE("ITEM LCLICKED");

    if (info.modifiers() != 0)
        return;

    if (workbench()->item(Qn::ZoomedRole))
        return; /* Don't change currently raised item if we're zoomed. It is surprising for the user. */

    QnResourceWidget *widget = item->isWidget() ? qobject_cast<QnResourceWidget *>(item->toGraphicsObject()) : nullptr;
    if (!widget)
        return;

    const auto workbenchItem = widget->item();

    const bool canRaise = !workbench()->currentLayout()->flags().testFlag(QnLayoutFlag::NoResize);
    if (canRaise && workbench()->item(Qn::RaisedRole) != workbenchItem)
    {
        /* Don't raise if there's only one item in the layout. */
        QRectF occupiedGeometry = widget->geometry();
        occupiedGeometry = dilated(occupiedGeometry, occupiedGeometry.size() * raisedGeometryThreshold);

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

void QnWorkbenchController::at_item_rightClicked(
    QGraphicsView* /*view*/,
    QGraphicsItem* item,
    const ClickInfo& /*info*/)
{
    TRACE("ITEM RCLICKED");

    QnResourceWidget *widget = item->isWidget() ? qobject_cast<QnResourceWidget *>(item->toGraphicsObject()) : NULL;
    if(widget == NULL)
        return;

    /* Right click does not select items.
     * However, we need to select the item under mouse for the menu to work as expected. */
    if(!widget->isSelected()) {
        widget->scene()->clearSelection();
        widget->setSelected(true);
    }
    showContextMenuAt(QCursor::pos());
}

void QnWorkbenchController::at_item_middleClicked(QGraphicsView *, QGraphicsItem *item, const ClickInfo &) {
    TRACE("ITEM MCLICKED");

    if (tourIsRunning(context()))
        return;

    QnResourceWidget *widget = item->isWidget() ? qobject_cast<QnResourceWidget *>(item->toGraphicsObject()) : NULL;
    if(widget == NULL)
        return;

    int rotation = 0;
    if (widget->resource() && widget->resource()->hasProperty(QnMediaResource::rotationKey())) {
        rotation = widget->resource()->getProperty(QnMediaResource::rotationKey()).toInt();
    }

    widget->item()->setRotation(rotation);
}

void QnWorkbenchController::at_item_doubleClicked(QGraphicsView *, QGraphicsItem *item, const ClickInfo &) {
    TRACE("ITEM DOUBLECLICKED");

    QnResourceWidget *widget = item->isWidget() ? qobject_cast<QnResourceWidget *>(item->toGraphicsObject()) : NULL;
    if(widget == NULL)
        return;

    at_item_doubleClicked(widget);
}

void QnWorkbenchController::at_item_doubleClicked(QnMediaResourceWidget *widget) {
    at_item_doubleClicked(static_cast<QnResourceWidget *>(widget));
}

void QnWorkbenchController::at_item_doubleClicked(QnResourceWidget *widget)
{
    display()->scene()->clearSelection();
    widget->setSelected(true);

    if (workbench()->currentLayout()->isLayoutTourReview())
    {
        if (auto resource = widget->resource())
            menu()->trigger(action::OpenInNewTabAction, resource);
        return;
    }

    QnWorkbenchItem *workbenchItem = widget->item();
    QnWorkbenchItem *zoomedItem = workbench()->item(Qn::ZoomedRole);
    if (zoomedItem == workbenchItem)
    {
        // Stop single layout tour if it is running.
        if (tourIsRunning(context()))
        {
            menu()->trigger(action::ToggleLayoutTourModeAction);
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

void QnWorkbenchController::at_scene_clicked(QGraphicsView *view, const ClickInfo &info) {
    if(info.button() == Qt::LeftButton) {
        at_scene_leftClicked(view, info);
    } else {
        at_scene_rightClicked(view, info);
    }
}

void QnWorkbenchController::at_scene_leftClicked(QGraphicsView *, const ClickInfo &) {
    TRACE("SCENE LCLICKED");

    if(workbench() == NULL)
        return;

    workbench()->setItem(Qn::RaisedRole, NULL);
}

void QnWorkbenchController::at_scene_rightClicked(QGraphicsView* view, const ClickInfo& /*info*/)
{
    TRACE("SCENE RCLICKED");

    view->scene()->clearSelection(); /* Just to feel safe. */

    showContextMenuAt(QCursor::pos());
}

void QnWorkbenchController::at_scene_doubleClicked(QGraphicsView *, const ClickInfo &) {
    TRACE("SCENE DOUBLECLICKED");

    if(workbench() == NULL)
        return;

    workbench()->setItem(Qn::ZoomedRole, NULL);
    display()->fitInView(display()->animationAllowed());
}

void QnWorkbenchController::at_display_widgetChanged(Qn::ItemRole role) {
    QnResourceWidget *newWidget = display()->widget(role);
    QnResourceWidget *oldWidget = m_widgetByRole[role];
    if(newWidget == oldWidget)
        return;

    m_widgetByRole[role] = newWidget;

    QGraphicsItem *focusItem = display()->scene()->focusItem();
    bool canMoveFocus = !focusItem
        || dynamic_cast<QnResourceWidget*>(focusItem)
        || dynamic_cast<QnGraphicsWebView*>(focusItem);

    if (newWidget && canMoveFocus)
        newWidget->setFocus(); /* Move focus only if it's not already grabbed by some control element. */

    switch(role) {
    case Qn::ZoomedRole:
        m_zoomedToggle->setActive(newWidget != NULL);
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

void QnWorkbenchController::at_display_widgetAdded(QnResourceWidget *widget) {
    widget->installEventFilter(this);

    connect(widget, SIGNAL(rotationStartRequested()),   this,   SLOT(at_widget_rotationStartRequested()));
    connect(widget, SIGNAL(rotationStopRequested()),    this,   SLOT(at_widget_rotationStopRequested()));
}

void QnWorkbenchController::at_display_widgetAboutToBeRemoved(QnResourceWidget *widget)
{
    if(widget == m_resizedWidget)
        m_resizedWidget = NULL;

    QnWorkbenchItem *item = widget->item();
    if (m_draggedWorkbenchItems.contains(item))
    {
        m_draggedWorkbenchItems.removeOne(item);
        updateDraggedItems();
        if (m_draggedWorkbenchItems.empty())
            m_moveInstrument->resetLater();
    }
    if (m_replacedWorkbenchItems.contains(item))
        m_replacedWorkbenchItems.removeOne(item);

    widget->removeEventFilter(this);

    disconnect(widget, NULL, this, NULL);
}

void QnWorkbenchController::at_widget_rotationStartRequested(QnResourceWidget *widget) {
    m_rotationInstrument->start(display()->view(), widget);
}

void QnWorkbenchController::at_widget_rotationStartRequested() {
    at_widget_rotationStartRequested(checked_cast<QnResourceWidget *>(sender()));
}

void QnWorkbenchController::at_widget_rotationStopRequested() {
    m_rotationInstrument->reset();
}

void QnWorkbenchController::at_selectAllAction_triggered() {
    foreach(QnResourceWidget *widget, display()->widgets())
        widget->setSelected(true);

    /* Move focus to scene if it's not there. */
    if (menu()->targetProvider() && menu()->targetProvider()->currentScope() != action::SceneScope)
        display()->scene()->setFocusItem(NULL);
}

void QnWorkbenchController::at_stopSmartSearchAction_triggered() {
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
    QScopedPointer<SignDialog> dialog(new SignDialog(widget->resource(), mainWindow()));
    dialog->exec();
}

void QnWorkbenchController::at_nextItemAction_triggered()
{
    moveCursor(QPoint(1, 0), QPoint(0, 1));
}

void QnWorkbenchController::at_previousItemAction_triggered()
{
    moveCursor(QPoint(-1, 0), QPoint(0, -1));
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

        if(!(widget->options() & QnResourceWidget::DisplayMotion)) {
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


void QnWorkbenchController::at_showInfoAction_triggered() {
    displayWidgetInfo(menu()->currentParameters(sender()).widgets(), true);
}

void QnWorkbenchController::at_hideInfoAction_triggered() {
    displayWidgetInfo(menu()->currentParameters(sender()).widgets(), false);
}

void QnWorkbenchController::at_toggleInfoAction_triggered() {
    QnResourceWidgetList widgets = menu()->currentParameters(sender()).widgets();

    bool hidden = false;
    foreach(QnResourceWidget *widget, widgets)
        if(!(widget->options() & QnResourceWidget::DisplayInfo)){
            hidden = true;
            break;
        }

    if(hidden) {
        at_showInfoAction_triggered();
    } else {
        at_hideInfoAction_triggered();
    }
}


void QnWorkbenchController::at_maximizeItemAction_triggered() {
    QnResourceWidgetList widgets = menu()->currentParameters(sender()).widgets();
    if(widgets.empty())
        return;

    workbench()->setItem(Qn::ZoomedRole, widgets[0]->item());
}

void QnWorkbenchController::at_unmaximizeItemAction_triggered() {
    workbench()->setItem(Qn::ZoomedRole, NULL);
}

void QnWorkbenchController::at_fitInViewAction_triggered() {
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
        connect(layout, &QnLayoutResource::lockedChanged, this,
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

    const auto isMotionWidget = layout->flags().testFlag(QnLayoutFlag::MotionWidget);
    if (isMotionWidget)
    {
        m_moveInstrument->setEnabled(false);
        m_resizingInstrument->setEnabled(false);
        m_wheelZoomInstrument->setEnabled(false);
        m_handScrollInstrument->setEnabled(false);
        m_itemLeftClickInstrument->setEnabled(false);
        m_itemRightClickInstrument->setEnabled(false);
        m_rubberBandInstrument->setEnabled(false);
        m_sceneClickInstrument->setEnabled(false);
        m_gridAdjustmentInstrument->setEnabled(false);
        return;
    }


    const auto resource = layout->resource();
    if (!resource)
    {
        m_moveInstrument->setEnabled(false);
        m_resizingInstrument->setEnabled(false);
        m_wheelZoomInstrument->setEnabled(false);
        return;
    }

    const auto permissions = accessController()->permissions(resource);
    const auto writable = permissions.testFlag(Qn::WritePermission);

    m_moveInstrument->setEnabled(writable && !resource->locked());
    m_resizingInstrument->setEnabled(writable && !resource->locked()
        && !layout->flags().testFlag(QnLayoutFlag::NoResize));

    const auto fixedViewport = layout->flags().testFlag(QnLayoutFlag::FixedViewport);
    m_wheelZoomInstrument->setEnabled(!fixedViewport);
    m_handScrollInstrument->setEnabled(!fixedViewport);
    m_gridAdjustmentInstrument->setEnabled(!fixedViewport);
}

void QnWorkbenchController::at_ptzProcessStarted(QnMediaResourceWidget *widget) {
    if(widget == NULL)
        return;

    // skip method if current widget is already raised or zoomed
    if (display()->widget(Qn::RaisedRole) == widget
        || display()->widget(Qn::ZoomedRole) == widget)
        return;

    workbench()->setItem(Qn::RaisedRole, NULL); /* Un-raise currently raised item so that it doesn't interfere with ptz. */
    display()->scene()->clearSelection();
    widget->setSelected(true);
    display()->bringToFront(widget);
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
