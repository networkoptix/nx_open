// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_resource_widget.h"

#include <chrono>
#include <iterator>

#include <QtCore/QSignalBlocker>
#include <QtCore/QTimer>
#include <QtCore/QVarLengthArray>
#include <QtGui/QAction>
#include <QtGui/QPainter>
#include <QtOpenGLWidgets/QOpenGLWidget>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsLinearLayout>

#include <qt_graphics_items/graphics_stacked_widget.h>

#include <api/common_message_processor.h>
#include <api/server_rest_connection.h>
#include <camera/cam_display.h>
#include <camera/camera_data_manager.h>
#include <camera/resource_display.h>
#include <client/client_globals.h>
#include <client/client_module.h>
#include <client/client_runtime_settings.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/ptz/activity_ptz_controller.h>
#include <core/ptz/fallback_ptz_controller.h>
#include <core/ptz/fisheye_home_ptz_controller.h>
#include <core/ptz/preset_ptz_controller.h>
#include <core/ptz/ptz_controller_pool.h>
#include <core/ptz/tour_ptz_controller.h>
#include <core/ptz/viewport_ptz_controller.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <core/resource/client_camera.h>
#include <core/resource/file_layout_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/analytics/metadata_log_parser.h>
#include <nx/media/media_data_packet.h>
#include <nx/media/sse_helper.h>
#include <nx/network/cloud/protocol_type.h>
#include <nx/streaming/abstract_archive_stream_reader.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/utils/trace/trace.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/cross_system/cross_system_ptz_controller_pool.h>
#include <nx/vms/client/core/media/consuming_motion_metadata_provider.h>
#include <nx/vms/client/core/motion/motion_grid.h>
#include <nx/vms/client/core/resource/data_loaders/caching_camera_data_loader.h> //< TODO: #sivanov Remove this dependency.
#include <nx/vms/client/core/resource/screen_recording/desktop_resource.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/software_trigger/software_triggers_controller.h>
#include <nx/vms/client/core/software_trigger/software_triggers_watcher.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/utils/painter_transform_scale_stripper.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/integrations/integrations.h>
#include <nx/vms/client/desktop/license/videowall_license_validator.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/resource/resource_descriptor.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/resource_properties/camera/camera_settings_tab.h>
#include <nx/vms/client/desktop/scene/resource_widget/dialogs/encrypted_archive_password_dialog.h>
#include <nx/vms/client/desktop/scene/resource_widget/overlays/playback_position_item.h>
#include <nx/vms/client/desktop/scene/resource_widget/overlays/rewind_overlay.h>
#include <nx/vms/client/desktop/scene/resource_widget/private/camera_button_controller.h>
#include <nx/vms/client/desktop/scene/resource_widget/private/media_resource_widget_p.h>
#include <nx/vms/client/desktop/scene/resource_widget/private/object_tracking_button_controller.h>
#include <nx/vms/client/desktop/skin/font_config.h>
#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <nx/vms/client/desktop/style/style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_health/default_password_cameras_watcher.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/common/recording_status_helper.h>
#include <nx/vms/client/desktop/ui/graphics/items/overlays/analytics_overlay_widget.h>
#include <nx/vms/client/desktop/ui/graphics/items/overlays/area_select_overlay_widget.h>
#include <nx/vms/client/desktop/ui/graphics/items/overlays/camera_hotspots_overlay_widget.h>
#include <nx/vms/client/desktop/ui/graphics/items/overlays/figure/figures_watcher.h>
#include <nx/vms/client/desktop/ui/graphics/items/overlays/roi_figures_overlay_widget.h>
#include <nx/vms/client/desktop/ui/graphics/items/resource/widget_analytics_controller.h>
#include <nx/vms/client/desktop/utils/timezone_helper.h>
#include <nx/vms/client/desktop/watermark/watermark_painter.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/intercom/utils.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/saas/saas_utils.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/crypt/crypt.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/rule_manager.h>
#include <nx/vms/event/strings_helper.h>
#include <nx/vms/license/usage_helper.h>
#include <nx/vms/time/formatter.h>
#include <ui/fisheye/fisheye_ptz_controller.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/items/generic/image_button_bar.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/overlays/hud_overlay_widget.h>
#include <ui/graphics/items/overlays/io_module_overlay_widget.h>
#include <ui/graphics/items/overlays/resource_status_overlay_widget.h>
#include <ui/graphics/items/overlays/resource_title_item.h>
#include <ui/graphics/items/overlays/scrollable_text_items_widget.h>
#include <ui/graphics/items/overlays/status_overlay_controller.h>
#include <ui/graphics/items/resource/button_ids.h>
#include <ui/graphics/items/resource/resource_widget_renderer.h>
#include <ui/graphics/items/resource/software_trigger_button.h>
#include <ui/graphics/items/resource/two_way_audio_widget.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/widgets/main_window.h>
#include <ui/workaround/gl_native_painting.h>
#include <ui/workbench/watchers/workbench_render_watcher.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/delayed.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/synctime.h>
#include <utils/math/color_transformations.h>

using namespace std::chrono;

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::analytics;
using namespace nx::vms::api;
using namespace ui;

namespace ptz = nx::vms::common::ptz;
namespace html = nx::vms::common::html;

using nx::vms::client::core::Geometry;
using nx::vms::client::core::MotionGrid;
using nx::vms::client::core::MotionSelection;
using nx::vms::client::core::SoftwareTriggersWatcher;

namespace {

static constexpr int kMicroInMilliSeconds = 1000;

// TODO: #rvasilenko Change to other constant - 0 is 1/1/1970
// Note: -1 is used for invalid time
// Now it is returned when there is no archive data and archive is played backwards.
static constexpr int kNoTimeValue = 0;

static constexpr qreal kMaxForwardSpeed = 16.0;
static constexpr qreal kMaxBackwardSpeed = 16.0;

static constexpr int kTriggersSpacing = 4;
static constexpr int kTriggerButtonSize = 40;
static constexpr int kTriggersMargin = 8; // overlaps HUD margin, i.e. does not sum up with it

static const auto kUpdateDetailsInterval = 1s;

static constexpr auto k360VRAspectRatio = 16.0 / 9.0;

template<class Cont, class Item>
bool contains(const Cont& cont, const Item& item)
{
    return std::find(cont.cbegin(), cont.cend(), item) != cont.cend();
}

bool isSpecialDateTimeValueUsec(qint64 dateTimeUsec)
{
    return ((dateTimeUsec == DATETIME_NOW)
        || (dateTimeUsec == AV_NOPTS_VALUE)
        || (dateTimeUsec == kNoTimeValue));
}

bool getPtzObjectName(const QnPtzControllerPtr &controller, const QnPtzObject &object, QString *name)
{
    switch (object.type)
    {
        case Qn::PresetPtzObject:
        {
            QnPtzPresetList presets;
            if (!controller->getPresets(&presets))
                return false;

            foreach(const QnPtzPreset &preset, presets)
            {
                if (preset.id == object.id)
                {
                    *name = preset.name;
                    return true;
                }
            }

            return false;
        }
        case Qn::TourPtzObject:
        {
            QnPtzTourList tours;
            if (!controller->getTours(&tours))
                return false;

            foreach(const QnPtzTour &tour, tours)
            {
                if (tour.id == object.id)
                {
                    *name = tour.name;
                    return true;
                }
            }

            return false;
        }
        default:
            return false;
    }
}

bool tourIsRunning(QnWorkbenchContext* context)
{
    return context->action(action::ToggleShowreelModeAction)->isChecked();
}

void drawCrosshair(QPainter* painter, const QRectF& rect)
{
    const PainterTransformScaleStripper scaleStripper(painter);
    const QRectF fullRect = scaleStripper.mapRect(rect);
    const qreal crosshairRadius = std::min(fullRect.width(), fullRect.height()) / 10;
    const QPointF center = fullRect.center();
    const QBrush color = Qt::red; //< Predefined color for debug purposes.

    {
        QnScopedPainterPenRollback penRollback(painter, QPen(color, 2));
        painter->drawEllipse(center, crosshairRadius, crosshairRadius);
    }

    {
        QnScopedPainterPenRollback penRollback(painter, QPen(color, 1));
        painter->drawLine(
            center.x(), center.y() - crosshairRadius,
            center.x(), center.y() + crosshairRadius);
        painter->drawLine(
            center.x() - crosshairRadius, center.y(),
            center.x() + crosshairRadius, center.y());
    }
}

// Change button checked state without activating button action.
void setButtonCheckedSilent(QnImageButtonWidget* button, bool checked)
{
    if (!NX_ASSERT(button && button->isCheckable()))
        return;

    if (button->isChecked() == checked)
        return;

    QSignalBlocker signalBlocker(button->defaultAction());
    button->setChecked(checked);
}

template<class T>
T* findPtzController(QnPtzControllerPtr controller)
{
    if (auto instance = qobject_cast<T*>(controller.data()))
        return instance;

    if (auto proxy = qobject_cast<QnProxyPtzController*>(controller.data()))
        return findPtzController<T>(proxy->baseController());

    if (auto fallback = qobject_cast<QnFallbackPtzController*>(controller.data()))
    {
        if (auto main = findPtzController<T>(fallback->mainController()))
            return main;

        return findPtzController<T>(fallback->fallbackController());
    }
    return nullptr;
}

} // namespace

QnMediaResourceWidget::QnMediaResourceWidget(
    nx::vms::client::desktop::SystemContext* systemContext,
    WindowContext* windowContext,
    QnWorkbenchItem* item,
    QGraphicsItem* parent)
    :
    base_type(systemContext, windowContext, item, parent),
    d(new MediaResourceWidgetPrivate(base_type::resource())),
    m_recordingStatusHelper(new RecordingStatusHelper(systemContext, this)),
    m_posUtcMs(DATETIME_INVALID),
    m_watermarkPainter(new WatermarkPainter),
    m_itemId(item->uuid()),
    m_triggerWatcher(new SoftwareTriggersWatcher(systemContext, this)),
    m_triggersController(
        new nx::vms::client::core::SoftwareTriggersController(systemContext, this)),
    m_buttonController(new CameraButtonController(this)),
    m_objectTrackingButtonController(new ObjectTrackingButtonController(this)),
    m_toggleImageEnhancementAction(new QAction(this))
{
    NX_ASSERT(d->mediaResource, "Media resource widget was created with a non-media resource.");
    d->isExportedLayout = layoutResource()->isFile();
    d->isPreviewSearchLayout = layoutResource()->isPreviewSearchLayout();

    if (d->isExportedLayout)
    {
        const auto layout = layoutResource().dynamicCast<QnFileLayoutResource>();
        if (NX_ASSERT(layout))
        {
            d->noExportPermission = layout->metadata().itemProperties[item->uuid()].flags.testFlag(
                NovItemProperties::Flag::noExportPermission);
        }
    }

    initRenderer();
    initDisplay();

    setupHud();

    connect(base_type::resource().get(), &QnResource::propertyChanged, this,
        &QnMediaResourceWidget::at_resource_propertyChanged);

    connect(base_type::resource().get(), &QnResource::mediaDewarpingParamsChanged, this,
        &QnMediaResourceWidget::updateDewarpingParams);

    connect(item, &QnWorkbenchItem::dewarpingParamsChanged, this,
        &QnMediaResourceWidget::handleDewarpingParamsChanged);

    connect(this, &QnResourceWidget::zoomTargetWidgetChanged, this,
        &QnMediaResourceWidget::updateDisplay);

    connect(item, &QnWorkbenchItem::imageEnhancementChanged, this,
        &QnMediaResourceWidget::at_item_imageEnhancementChanged);

    connect(this, &QnResourceWidget::zoomRectChanged, this,
        &QnMediaResourceWidget::updateFisheye);

    connect(d.get(), &MediaResourceWidgetPrivate::stateChanged, this,
        [this]
        {
            const bool animate = animationAllowed();
            updateIoModuleVisibility(animate);
            updateStatusOverlay(animate);
            updateOverlayButton();
            updateButtonsVisibility();
        });

    connect(d.get(), &MediaResourceWidgetPrivate::analyticsSupportChanged, this,
        &QnMediaResourceWidget::updateButtonsVisibility);

    connect(d.get(), &MediaResourceWidgetPrivate::analyticsSupportChanged, this,
        &QnMediaResourceWidget::analyticsSupportChanged);

    if (d->camera)
    {
        auto updateStatus = [this]
            {
                const bool animate = animationAllowed();
                updateIoModuleVisibility(animate);
                updateStatusOverlay(animate);
                updateOverlayButton();
            };

        connect(d.get(), &MediaResourceWidgetPrivate::licenseStatusChanged, this,
            [this, updateStatus]
            {
                updateStatus();
                emit licenseStatusChanged();
            });

        auto ptzPool = systemContext->ptzControllerPool();
        if (d->camera->hasFlags(Qn::cross_system))
        {
            auto crossSystemControllerPool =
                dynamic_cast<nx::vms::client::core::CrossSystemPtzControllerPool*>(ptzPool);
            // For the cross-system cameras PTZ controller is initialized on demand.
            if (NX_ASSERT(crossSystemControllerPool))
                crossSystemControllerPool->createControllerIfNeeded(d->camera);
        }
        connect(ptzPool, &QnPtzControllerPool::controllerChanged, this,
            [this](const QnResourcePtr& resource)
            {
                // Make sure we will not handle resource removing.
                if (resource == d->camera && resource->resourcePool())
                    updatePtzController();
            });
    }

    connect(navigator(), &QnWorkbenchNavigator::bookmarksModeEnabledChanged, this,
        &QnMediaResourceWidget::updateCompositeOverlayMode);

    // Cross-system contexts have no message processor.
    if (auto messageProcessor = systemContext->messageProcessor())
    {
        connect(
            messageProcessor,
            &QnCommonMessageProcessor::businessActionReceived,
            this,
            [this](const nx::vms::event::AbstractActionPtr &businessAction)
            {
                if (businessAction->actionType() != ActionType::executePtzPresetAction)
                    return;

                const auto &actionParams = businessAction->getParams();
                if (actionParams.actionResourceId != d->resource->getId())
                    return;

                if (m_ptzController)
                {
                    m_ptzController->activatePreset(
                        actionParams.presetId,
                        QnAbstractPtzController::MaxPtzSpeed);
                }
            });
    }

    connect(action(action::ToggleSyncAction), &QAction::triggered, this,
        &QnMediaResourceWidget::handleSyncStateChanged);

    updateDewarpingParams();
    updateAspectRatio();
    updatePtzController();

    /* Set up info updates. */
    connect(this, &QnMediaResourceWidget::updateInfoTextLater, this,
        &QnMediaResourceWidget::updateInfoText, Qt::QueuedConnection);

    /* Set up overlays */
    initIoModuleOverlay();
    updateCameraButtons();
    initAnalyticsOverlays();
    initAreaSelectOverlay();
    initCameraHotspotsOverlay();

    /* Set up buttons. */
    createButtons();
    initStatusOverlayController();


    // TODO: #sivanov Get rid of resourceChanged.
    connect(base_type::resource().get(), &QnResource::resourceChanged, this,
        &QnMediaResourceWidget::updateButtonsVisibility);

    connect(this, &QnResourceWidget::aspectRatioChanged,
        this, &QnMediaResourceWidget::updateZoomWindowDewarping);

    connect(this, &QnResourceWidget::zoomRectChanged,
        this, &QnMediaResourceWidget::at_zoomRectChanged);

    connect(windowContext->resourceWidgetRenderWatcher(),
        &QnWorkbenchRenderWatcher::widgetChanged,
        this,
        &QnMediaResourceWidget::at_renderWatcher_widgetChanged);

    // Update buttons for single Showreel start/stop.
    connect(action(action::ToggleShowreelModeAction), &QAction::toggled, this,
        &QnMediaResourceWidget::updateButtonsVisibility);

    m_recordingStatusHelper->setCamera(d->camera);

    connect(m_recordingStatusHelper, &RecordingStatusHelper::recordingModeChanged,
        this, &QnMediaResourceWidget::updateIconButton);

    d->analyticsController.reset(new WidgetAnalyticsController(this));
    d->analyticsController->setAnalyticsOverlayWidget(m_analyticsOverlayWidget);
    d->analyticsController->setAnalyticsMetadataProvider(d->analyticsMetadataProvider);

    at_camDisplay_liveChanged();
    setPtzMode(item->controlPtz());
    at_imageEnhancementButton_toggled(item->imageEnhancement().enabled);
    updateIconButton();

    updateTitleText();
    updateInfoText();
    updateDetailsText();
    updatePositionText();
    updateFisheye();
    updateStatusOverlay(false);
    updateOverlayButton();
    setImageEnhancement(item->imageEnhancement());

    initSoftwareTriggers();

    updateWatermark();
    connect(systemContext->globalSettings(),
        &nx::vms::common::SystemSettings::watermarkChanged,
        this,
        &QnMediaResourceWidget::updateWatermark);
    connect(systemContext->accessController(),
        &nx::vms::client::core::AccessController::userChanged,
        this,
        &QnMediaResourceWidget::updateWatermark);

    connect(this, &QnMediaResourceWidget::updateInfoTextLater, this,
        &QnMediaResourceWidget::updateCurrentUtcPosMs, Qt::QueuedConnection);

    connect(layoutResource().get(),
        &LayoutResource::itemDataChanged,
        this,
        &QnMediaResourceWidget::handleItemDataChanged);

    // Update paused state after all initialization for widget is done.
    executeDelayedParented(
        [this]()
        {
            const QVariant isPaused = layoutResource()->itemData(m_itemId, Qn::ItemPausedRole);
            if (isPaused.isValid())
            {
                handleItemDataChanged(
                    m_itemId,
                    Qn::ItemPausedRole,
                    isPaused);
            }
        }, this);

    const bool canRotate = ResourceAccessManager::hasPermissions(layoutResource(),
        Qn::WritePermission);
    setOption(WindowRotationForbidden, !hasVideo() || !canRotate);
    setAnalyticsObjectsVisible(item->displayAnalyticsObjects(), false);
    setRoiVisible(item->displayRoi(), false);
    updateButtonsVisibility();
    updateHotspotsState();

    const auto triggerActionHandler =
        [this](const QnUuid& ruleId, bool success)
        {
            const auto button = static_cast<SoftwareTriggerButton*>(
                m_triggersContainer->item(ruleId));
            if (!button)
            {
                NX_ASSERT(false, "Can't find button for specified trigger rule id!");
                return;
            }

            button->setEnabled(true);
            button->setState(success
                ? SoftwareTriggerButton::State::Success
                : SoftwareTriggerButton::State::Failure);
        };

    // Local files dropped onto layout are not virtual cameras.
    if (d->camera)
        m_triggersController->setResourceId(d->camera->getId());

    using Controller = nx::vms::client::core::SoftwareTriggersController;
    connect(m_triggersController, &Controller::triggerActivated, this, triggerActionHandler);
    connect(m_triggersController, &Controller::triggerDeactivated, this, triggerActionHandler);
}

QnMediaResourceWidget::~QnMediaResourceWidget()
{
    if (d->display())
        d->display()->removeRenderer(m_renderer);

    m_renderer->notInUse();
    m_renderer->destroyAsync();
}

void QnMediaResourceWidget::beforeDestroy()
{
    layoutResource()->disconnect(this);
}

void QnMediaResourceWidget::handleItemDataChanged(
    const QnUuid& id,
    Qn::ItemDataRole role,
    const QVariant& data)
{
    if (id != m_itemId)
        return;

    switch (role)
    {
        case Qn::ItemPausedRole:
        {
            const bool shouldPause = data.toBool();
            if (shouldPause == display()->isPaused())
                return;

            if (shouldPause)
                display()->pause();
            else
                display()->play();
            break;
        }
        case Qn::ItemTimeRole:
        {
            setPosition(data.value<qint64>());
            break;
        }
        case Qn::ItemSpeedRole:
            if (const auto reader = display()->archiveReader())
                reader->setSpeed(data.toDouble());
            break;
        default:
            break;
    }
}

void QnMediaResourceWidget::handleDewarpingParamsChanged()
{
    updateFisheye();
    updateButtonsVisibility();
    emit dewarpingParamsChanged();
}

void QnMediaResourceWidget::initRenderer()
{
    // TODO: We shouldn't be using OpenGL context in class constructor.
    QGraphicsView *view = mainWindow()->view();
    m_renderer = new QnResourceWidgetRenderer(nullptr, view->viewport());
    connect(m_renderer, &QnResourceWidgetRenderer::sourceSizeChanged, this,
        &QnMediaResourceWidget::updateAspectRatio);
    m_renderer->inUse();
}

void QnMediaResourceWidget::initDisplay()
{
    const auto zoomTargetWidget = dynamic_cast<QnMediaResourceWidget *>(this->zoomTargetWidget());
    setDisplay(zoomTargetWidget
        ? zoomTargetWidget->display()
        : QnResourceDisplayPtr(new QnResourceDisplay(d->resource)));
}

void QnMediaResourceWidget::initSoftwareTriggers()
{
    if (!display()->camDisplay()->isRealTimeSource())
        return;

    if (d->isPreviewSearchLayout)
        return;

    static const auto kUpdateTriggersInterval = 1000;
    const auto updateTriggersAvailabilityTimer = new QTimer(this);
    updateTriggersAvailabilityTimer->setInterval(kUpdateTriggersInterval);

    connect(updateTriggersAvailabilityTimer, &QTimer::timeout,
        m_triggerWatcher, &SoftwareTriggersWatcher::updateTriggersAvailability);

    connect(m_triggerWatcher, &SoftwareTriggersWatcher::triggerAdded,
        this, &QnMediaResourceWidget::at_triggerAdded);
    connect(m_triggerWatcher, &SoftwareTriggersWatcher::triggerRemoved,
        this, &QnMediaResourceWidget::at_triggerRemoved);
    connect(m_triggerWatcher, &SoftwareTriggersWatcher::triggerFieldsChanged,
        this, &QnMediaResourceWidget::at_triggerFieldsChanged);

    m_triggerWatcher->setResourceId(d->resource->getId());
    updateTriggersAvailabilityTimer->start();
}

void QnMediaResourceWidget::initIoModuleOverlay()
{
    if (d->isIoModule)
    {
        // TODO: #vkutin Make a style metric that holds this value.
        const auto topMargin = titleBar()
            ? titleBar()->leftButtonsBar()->uniformButtonSize().height()
            : 0.0;

        m_ioModuleOverlayWidget = new QnIoModuleOverlayWidget(d->camera, d->ioModuleMonitor);
        m_ioModuleOverlayWidget->setAcceptedMouseButtons(Qt::NoButton);
        m_ioModuleOverlayWidget->setUserInputEnabled(
            ResourceAccessManager::hasPermissions(d->camera, Qn::DeviceInputPermission));
        m_ioModuleOverlayWidget->setContentsMargins(0.0, topMargin, 0.0, 0.0);
        addOverlayWidget(m_ioModuleOverlayWidget,
            {Visible, OverlayFlag::autoRotate | OverlayFlag::bindToViewport});

        updateButtonsVisibility();
        updateIoModuleVisibility(false);

        connect(d->camera.get(), &QnResource::statusChanged, this,
            [this]
            {
                updateIoModuleVisibility(animationAllowed());
            });
    }
}

void QnMediaResourceWidget::initAreaSelectOverlay()
{
    if (!hasVideo())
        return;

    m_areaSelectOverlayWidget = new AreaSelectOverlayWidget(m_compositeOverlay, this);
    addOverlayWidget(m_areaSelectOverlayWidget, {Visible, OverlayFlag::bindToViewport});

    connect(m_areaSelectOverlayWidget, &AreaSelectOverlayWidget::selectedAreaChanged,
        this, &QnMediaResourceWidget::handleSelectedAreaChanged);

    connect(m_areaSelectOverlayWidget, &AreaSelectOverlayWidget::selectionStarted, this,
        [this]() { selectThisWidget(true); });

    connect(m_areaSelectOverlayWidget, &AreaSelectOverlayWidget::selectionFinished,
        this, &QnMediaResourceWidget::areaSelectionFinished);
}

void QnMediaResourceWidget::initAnalyticsOverlays()
{
    if (!hasVideo())
        return;

    m_roiFiguresOverlayWidget = new RoiFiguresOverlayWidget(d->camera, m_compositeOverlay);
    addOverlayWidget(m_roiFiguresOverlayWidget, {Visible, OverlayFlag::bindToViewport});

    m_analyticsOverlayWidget = new AnalyticsOverlayWidget(m_compositeOverlay);
    addOverlayWidget(m_analyticsOverlayWidget,
        {Visible, OverlayFlag::autoRotate | OverlayFlag::bindToViewport});

    connect(m_statusOverlay, &QnStatusOverlayWidget::opacityChanged, this,
        [this]()
        {
            updateAnalyticsVisibility();
            updateHud(false);
        });
}

void QnMediaResourceWidget::initStatusOverlayController()
{
    if (!d->camera)
         return;

     const auto changeCameraPassword =
         [this](const QnVirtualCameraResourceList& cameras, bool forceShowCamerasList)
         {
             auto parameters = action::Parameters(cameras);
             parameters.setArgument(Qn::ForceShowCamerasList, forceShowCamerasList);
             menu()->trigger(action::ChangeDefaultCameraPasswordAction, parameters);
         };

     const auto controller = statusOverlayController();
     connect(controller, &QnStatusOverlayController::buttonClicked, this,
         [this, changeCameraPassword](Qn::ResourceOverlayButton button)
         {
             switch (button)
             {
                 case Qn::ResourceOverlayButton::Diagnostics:
                     processDiagnosticsRequest();
                     break;
                 case Qn::ResourceOverlayButton::EnableLicense:
                     processEnableLicenseRequest();
                     break;
                 case Qn::ResourceOverlayButton::Settings:
                     processSettingsRequest();
                     break;
                 case Qn::ResourceOverlayButton::MoreLicenses:
                     processMoreLicensesRequest();
                     break;
                 case Qn::ResourceOverlayButton::SetPassword:
                     changeCameraPassword(QnVirtualCameraResourceList() << d->camera, false);
                     break;
                 case Qn::ResourceOverlayButton::UnlockEncryptedArchive:
                     processEncryptedArchiveUnlockRequst();
                     break;
                 default:
                     break;
             }
         });

     connect(controller, &QnStatusOverlayController::customButtonClicked, this,
         [this, changeCameraPassword]()
         {
             const auto passwordWatcher = windowContext()->workbenchContext()
                 ->findInstance<DefaultPasswordCamerasWatcher>();
             changeCameraPassword(passwordWatcher->camerasWithDefaultPassword().values(), true);
         });
}

void QnMediaResourceWidget::initCameraHotspotsOverlay()
{
    if (!canDisplayHotspots())
        return;

    connect(d->camera.get(), &QnClientCameraResource::cameraHotspotsEnabledChanged,
        this, &QnMediaResourceWidget::updateHotspotsState);

    m_cameraHotspotsOverlayWidget = new CameraHotspotsOverlayWidget(this);
    m_cameraHotspotsOverlayWidget->setContentsMargins(0.0, 0.0, 0.0, 0.0);

    addOverlayWidget(m_cameraHotspotsOverlayWidget, {UserVisible, OverlayFlag::none, InfoLayer});
    m_cameraHotspotsOverlayWidget->stackBefore(m_hudOverlay);
}

QnMediaResourceWidget::AreaType QnMediaResourceWidget::areaSelectionType() const
{
    return m_areaSelectionType;
}

void QnMediaResourceWidget::setAreaSelectionType(AreaType value)
{
    if (m_areaSelectionType == value)
        return;

    const auto oldType = m_areaSelectionType;
    m_areaSelectionType = value;

    // Reset old selection dependencies.
    if (oldType == AreaType::motion)
        setMotionSearchModeEnabled(false);

    if (m_areaSelectOverlayWidget)
        m_areaSelectOverlayWidget->setActive(false);

    if (m_areaSelectionType == AreaType::motion)
        setMotionSearchModeEnabled(true);

    updateSelectedArea();
    emit areaSelectionTypeChanged();
}

void QnMediaResourceWidget::unsetAreaSelectionType(AreaType value)
{
    if (m_areaSelectionType == value)
        setAreaSelectionType(AreaType::none);
}

bool QnMediaResourceWidget::areaSelectionEnabled() const
{
    return m_areaSelectOverlayWidget && m_areaSelectionType != AreaType::none
        ? m_areaSelectOverlayWidget->active()
        : false;
}

void QnMediaResourceWidget::setAreaSelectionEnabled(bool value)
{
    if (!m_areaSelectOverlayWidget)
        return;

    if (m_areaSelectionType == AreaType::none || m_areaSelectionType == AreaType::motion)
        return;

    if (value == m_areaSelectOverlayWidget->active())
        return;

    m_areaSelectOverlayWidget->setActive(value);
    emit areaSelectionEnabledChanged();
}

void QnMediaResourceWidget::setAreaSelectionEnabled(AreaType areaType, bool value)
{
    if (value)
    {
        setAreaSelectionType(areaType);
        setAreaSelectionEnabled(true);
    }
    else
    {
        if (m_areaSelectionType == areaType)
            setAreaSelectionEnabled(false);
    }
}

QRectF QnMediaResourceWidget::analyticsFilterRect() const
{
    return m_analyticsFilterRect;
}

void QnMediaResourceWidget::setAnalyticsFilterRect(const QRectF& value)
{
    if (qFuzzyEquals(m_analyticsFilterRect, value))
        return;

    m_analyticsFilterRect = value;
    updateSelectedArea();

    emit analyticsFilterRectChanged();
}

void QnMediaResourceWidget::updateSelectedArea()
{
    switch (m_areaSelectionType)
    {
        case AreaType::motion:
            // TODO: #vkutin Ensure stored motion regions are used by motion selection instrument.
            [[fallthrough]];
        case AreaType::none:
            if (m_areaSelectOverlayWidget)
                m_areaSelectOverlayWidget->setSelectedArea({});
            break;

        case AreaType::analytics:
            if (m_areaSelectOverlayWidget)
                m_areaSelectOverlayWidget->setSelectedArea(m_analyticsFilterRect);
            break;
    }
}

void QnMediaResourceWidget::handleSelectedAreaChanged()
{
    switch (m_areaSelectionType)
    {
        case AreaType::none:
        case AreaType::motion:
            NX_ASSERT(m_areaSelectOverlayWidget->selectedArea().isEmpty());
            break;

        case AreaType::analytics:
            NX_ASSERT(m_areaSelectOverlayWidget);
            if (!m_areaSelectOverlayWidget)
                break;

            setAnalyticsFilterRect(m_areaSelectOverlayWidget->selectedArea());
            break;
    }
}

QString QnMediaResourceWidget::overlayCustomButtonText(
    Qn::ResourceStatusOverlay statusOverlay) const
{
    if (statusOverlay != Qn::PasswordRequiredOverlay)
        return QString();

    if (!accessController()->hasPowerUserPermissions())
        return QString();

    const auto watcher = windowContext()->workbenchContext()
        ->findInstance<DefaultPasswordCamerasWatcher>();
    const auto camerasCount = watcher ? watcher->camerasWithDefaultPassword().size() : 0;
    return camerasCount > 1
        ? tr("Set for all %n Cameras", nullptr, camerasCount)
        : QString();
}

void QnMediaResourceWidget::createButtons()
{
    if (d->camera && d->camera->isPtzRedirected())
    {
        createActionAndButton(
            /* icon*/ "item/area_zoom.svg",
            /* checked*/ false,
            /* shortcut*/ {},
            /* tooltip */ tr("Area Zoom"),
            /* help id */ HelpTopic::Id::MainWindow_MediaItem_Ptz,
            /* internal id */ Qn::PtzButton,
            /* statistics key */ "media_widget_area_zoom",
            /* handler */ &QnMediaResourceWidget::setPtzMode);
    }
    else
    {
        createActionAndButton(
            /* icon*/ "item/ptz.svg",
            /* checked*/ false,
            /* shortcut*/ QKeySequence::fromString("P"),
            /* tooltip */ tr("PTZ"),
            /* help id */ HelpTopic::Id::MainWindow_MediaItem_Ptz,
            /* internal id */ Qn::PtzButton,
            /* statistics key */ "media_widget_ptz",
            /* handler */ &QnMediaResourceWidget::setPtzMode);
    }

    createActionAndButton(
        "item/fisheye.svg",
        item()->dewarpingParams().enabled,
        QKeySequence::fromString("D"),
        tr("Dewarping"),
        HelpTopic::Id::MainWindow_MediaItem_Dewarping,
        Qn::FishEyeButton, "media_widget_fisheye",
        &QnMediaResourceWidget::at_fishEyeButton_toggled);

    createActionAndButton(
        "item/zoom_window.svg",
        false,
        QKeySequence::fromString("W"),
        tr("Create Zoom Window"),
        HelpTopic::Id::MainWindow_MediaItem_ZoomWindows,
        Qn::ZoomWindowButton, "media_widget_zoom",
        &QnMediaResourceWidget::setZoomWindowCreationModeEnabled);

    if (canDisplayHotspots())
    {
        createActionAndButton(
            "item/hotspots.svg",
            item()->displayHotspots() && d->camera->cameraHotspotsEnabled(),
            QKeySequence::fromString("H"),
            tr("Hotspots"),
            HelpTopic::Id::Empty,
            Qn::HotspotsButton, "media_widget_hotspots",
            &QnMediaResourceWidget::setHotspotsVisible);
    }

    m_toggleImageEnhancementAction->setCheckable(true);
    m_toggleImageEnhancementAction->setChecked(item()->imageEnhancement().enabled);
    m_toggleImageEnhancementAction->setShortcut(QStringLiteral("J"));
    m_toggleImageEnhancementAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(m_toggleImageEnhancementAction);

    connect(m_toggleImageEnhancementAction, &QAction::toggled, this,
        &QnMediaResourceWidget::at_imageEnhancementButton_toggled);

    {
        auto ioModuleButton = createStatisticAwareButton("media_widget_io_module");
        ioModuleButton->setIcon(qnSkin->icon("item/io.svg"));
        ioModuleButton->setCheckable(true);
        ioModuleButton->setChecked(false);
        ioModuleButton->setToolTip(tr("I/O Module"));
        connect(ioModuleButton, &QnImageButtonWidget::toggled, this,
            &QnMediaResourceWidget::at_ioModuleButton_toggled);
        titleBar()->rightButtonsBar()->addButton(Qn::IoModuleButton, ioModuleButton);
    }

    auto screenshotButton = createStatisticAwareButton("media_widget_screenshot");
    screenshotButton->setIcon(loadSvgIcon("item/screenshot.svg"));
    screenshotButton->setCheckable(false);
    screenshotButton->setToolTip(tooltipText(tr("Screenshot"), QKeySequence{"Alt+S"}));
    setHelpTopic(screenshotButton, HelpTopic::Id::MainWindow_MediaItem_Screenshot);
    connect(screenshotButton, &QnImageButtonWidget::clicked, this,
        &QnMediaResourceWidget::at_screenshotButton_clicked);

    titleBar()->rightButtonsBar()->addButton(Qn::ScreenshotButton, screenshotButton);

    if (ini().developerMode)
    {
        auto debugScreenshotButton =
            createStatisticAwareButton("media_widget_debug_screenshot");
        debugScreenshotButton->setIcon(loadSvgIcon("item/screenshot.svg"));
        debugScreenshotButton->setCheckable(false);
        debugScreenshotButton->setToolTip("Debug set of screenshots");
        connect(debugScreenshotButton, &QnImageButtonWidget::clicked, this,
            [this]
            {
                menu()->trigger(action::TakeScreenshotAction, action::Parameters(this)
                    .withArgument<QString>(Qn::FileNameRole, "_DEBUG_SCREENSHOT_KEY_"));
            });
        titleBar()->rightButtonsBar()->addButton(Qn::DbgScreenshotButton, debugScreenshotButton);
    }

    const auto motionSearchLocalAction = createActionAndButton(
        "item/motion.svg",
        isMotionSearchModeEnabled(),
        {},
        tooltipText(tr("Motion Search"), "Alt+M"),
        HelpTopic::Id::MainWindow_MediaItem_SmartSearch,
        Qn::MotionSearchButton,
        "media_widget_msearch",
        /*executor*/ {});

    connect(motionSearchLocalAction, &QAction::toggled, this,
        [this](bool on)
        {
            if ((titleBar()->rightButtonsBar()->visibleButtons() & Qn::MotionSearchButton) == 0)
                return;

            const auto motionSearchButton = titleBar()->rightButtonsBar()->button(
                Qn::MotionSearchButton);

            if (motionSearchButton->isClicked() && on)
                selectThisWidget(true);

            setMotionSearchModeEnabled(on);
        });

    const auto objectSearchLocalAction = createActionAndButton(
        "item/object.svg",
        action(action::ObjectSearchModeAction)->isChecked(),
        {},
        tooltipText(tr("Object Search"), "Alt+O"),
        /*helpTopic*/ {},
        Qn::ObjectSearchButton,
        "media_widget_object_search",
        /*executor*/ {});

    // Here `action(action::ObjectSearchModeAction)` is a global internal action that doesn't
    // check any conditions and has semantics of switching the Right Panel in or out of the
    // Object Search mode: current tab is "Objects", camera selection is "Current camera".
    // And `objectSearchLocalAction` is a local action bound to this media widget, with a local
    // shortcut, and its handler checks if the Object Search mode is relevant for this widget.

    connect(objectSearchLocalAction, &QAction::toggled, this,
        [this](bool on)
        {
            if ((titleBar()->rightButtonsBar()->visibleButtons() & Qn::ObjectSearchButton) == 0)
                return;

            const auto objectSearchButton = titleBar()->rightButtonsBar()->button(
                Qn::ObjectSearchButton);

            if (objectSearchButton->isClicked() && on)
                selectThisWidget(true);

            setProperty(Qn::NoHandScrollOver, on);
            action(action::ObjectSearchModeAction)->setChecked(on);
        });

    connect(action(action::ObjectSearchModeAction),
        &QAction::toggled,
        titleBar()->rightButtonsBar()->button(Qn::ObjectSearchButton),
        &QnImageButtonWidget::setChecked);
}

void QnMediaResourceWidget::updatePtzController()
{
    if (!item())
        return;

    const auto threadPool = systemContext()->ptzControllerPool()->commandThreadPool();
    const auto executorThread = systemContext()->ptzControllerPool()->executorThread();

    /* Set up PTZ controller. */
    QnPtzControllerPtr fisheyeController;
    fisheyeController.reset(new QnFisheyePtzController(this), &QObject::deleteLater);
    connect(item(), &QnWorkbenchItem::dewarpingParamsChanged,
        static_cast<QnFisheyePtzController*>(fisheyeController.get()),
        &QnFisheyePtzController::updateLimits);

    fisheyeController.reset(new QnViewportPtzController(fisheyeController));
    fisheyeController.reset(new QnPresetPtzController(fisheyeController));
    fisheyeController.reset(new QnTourPtzController(
        fisheyeController,
        threadPool,
        executorThread));
    fisheyeController.reset(new QnActivityPtzController(
        QnActivityPtzController::Local,
        fisheyeController));

    // Widget's zoomRect property is set only in `synchronize()` method, not instantly.
    if (item() && item()->zoomRect().isNull())
    {
        // Zoom items are not allowed to return home.
        fisheyeController.reset(new QnFisheyeHomePtzController(fisheyeController));
    }

    if (d->camera)
    {
        if (auto serverController = systemContext()->ptzControllerPool()->controller(d->camera))
        {
            serverController.reset(new QnActivityPtzController(
                QnActivityPtzController::Client,
                serverController));
            m_ptzController.reset(new QnFallbackPtzController(fisheyeController, serverController));
        }
        else
        {
            m_ptzController = fisheyeController;
        }
    }
    else
    {
        m_ptzController = fisheyeController;
    }

    connect(m_ptzController.get(), &QnAbstractPtzController::changed, this,
        &QnMediaResourceWidget::at_ptzController_changed);

    emit ptzControllerChanged();
}

qreal QnMediaResourceWidget::calculateVideoAspectRatio() const
{
    if (!placeholderPixmap().isNull() && zoomTargetWidget() && !zoomRect().isValid())
        return Geometry::aspectRatio(placeholderPixmap().size());

    const auto aviResource = d->resource.dynamicCast<QnAviResource>();
    if (aviResource && aviResource->flags().testFlag(Qn::still_image))
    {
        const auto aspect = aviResource->imageAspectRatio();
        if (aspect.isValid())
            return aspect.toFloat();
    }

    auto result = resource()->customAspectRatio();
    if (result.isValid())
        return result.toFloat();

    if (const auto& camera = resource()->toResourcePtr().dynamicCast<QnVirtualCameraResource>())
    {
        const auto cameraAspectRatio = camera->aspectRatio();
        if (cameraAspectRatio.isValid())
            return cameraAspectRatio.toFloat();
    }

    if (m_renderer && !m_renderer->sourceSize().isEmpty())
    {
        auto sourceSize = m_renderer->sourceSize();
        return Geometry::aspectRatio(sourceSize);
    }
    return defaultAspectRatio(); /*< Here we can get -1.0 if there are no predefined AR set */
}

void QnMediaResourceWidget::updateDisplay()
{
    const auto zoomTargetWidget = dynamic_cast<QnMediaResourceWidget*>(this->zoomTargetWidget());

    const auto display = zoomTargetWidget
        ? zoomTargetWidget->display()
        : QnResourceDisplayPtr(new QnResourceDisplay(d->mediaResource->toResourcePtr()));

    setDisplay(display);
}

const QnMediaResourcePtr &QnMediaResourceWidget::resource() const
{
    return d->mediaResource;
}

QnResourceDisplayPtr QnMediaResourceWidget::display() const
{
    return d->display();
}

QnResourceWidgetRenderer* QnMediaResourceWidget::renderer() const
{
    return m_renderer;
}

int QnMediaResourceWidget::defaultRotation() const
{
    return resource()->forcedRotation().value_or(0);
}

int QnMediaResourceWidget::defaultFullRotation() const
{
    const bool fisheyeEnabled = (isZoomWindow() || (item() && item()->dewarpingParams().enabled))
        && dewarpingParams().enabled;
    const int fisheyeRotation = fisheyeEnabled
        && dewarpingParams().viewMode == dewarping::FisheyeCameraMount::ceiling ? 180 : 0;

    return (defaultRotation() + fisheyeRotation) % 360;
}

bool QnMediaResourceWidget::hasVideo() const
{
    return d->hasVideo;
}

QPoint QnMediaResourceWidget::mapToMotionGrid(const QPointF &itemPos)
{
    QPointF gridPosF(Geometry::cwiseDiv(itemPos, Geometry::cwiseDiv(size(), motionGridSize())));
    QPoint gridPos(qFuzzyFloor(gridPosF.x()), qFuzzyFloor(gridPosF.y()));

    return Geometry::bounded(gridPos, QRect(QPoint(0, 0), motionGridSize()));
}

QPointF QnMediaResourceWidget::mapFromMotionGrid(const QPoint &gridPos)
{
    return Geometry::cwiseMul(gridPos, Geometry::cwiseDiv(size(), motionGridSize()));
}

QSize QnMediaResourceWidget::motionGridSize() const
{
    return Geometry::cwiseMul(
        channelLayout()->size(), QSize(MotionGrid::kWidth, MotionGrid::kHeight));
}

QPoint QnMediaResourceWidget::channelGridOffset(int channel) const
{
    return Geometry::cwiseMul(
        channelLayout()->position(channel), QSize(MotionGrid::kWidth, MotionGrid::kHeight));
}

void QnMediaResourceWidget::suspendHomePtzController()
{
    if (auto homePtzController = findPtzController<QnFisheyeHomePtzController>(m_ptzController))
        homePtzController->suspend();
}

void QnMediaResourceWidget::hideTextOverlay(const QnUuid& id)
{
    setTextOverlayParameters(id, false, QString(), QnHtmlTextItemOptions());
}

void QnMediaResourceWidget::showTextOverlay(const QnUuid& id, const QString& text,
    const QnHtmlTextItemOptions& options)
{
    setTextOverlayParameters(id, true, text, options);
}

void QnMediaResourceWidget::setTextOverlayParameters(const QnUuid& id, bool visible,
    const QString& text, const QnHtmlTextItemOptions& options)
{
    if (!m_textOverlayWidget)
        return;

    m_textOverlayWidget->deleteItem(id);

    if (!visible)
        return;

    /* Do not add empty text items: */
    if (text.trimmed().isEmpty())
        return;

    m_textOverlayWidget->addItem(text, options, id);
}

Qn::RenderStatus QnMediaResourceWidget::paintVideoTexture(
    QPainter* painter,
    int channel,
    const QRectF& sourceSubRect,
    const QRectF& targetRect)
{
    QOpenGLFunctions* functions = nullptr;
    const auto glWidget = m_renderer->openGLWidget();
    if (glWidget)
    {
        QnGlNativePainting::begin(glWidget, painter);
        functions = glWidget->context()->functions();
    }

    const qreal opacity = effectiveOpacity();
    bool opaque = qFuzzyCompare(opacity, 1.0);
    // Always use blending for images.
    if (functions)
    {
        if (!opaque || (base_type::resource()->flags() & Qn::still_image))
        {
            functions->glEnable(GL_BLEND);
            functions->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
    }

    m_renderer->setBlurFactor(m_statusOverlay->opacity());
    Qn::RenderStatus result = Qn::RenderStatus::NothingRendered;

    result = m_renderer->paint(painter, channel, sourceSubRect, targetRect, opacity);

    m_paintedChannels[channel] = true;

    /* There is no need to restore blending state before invoking endNativePainting. */
    if (glWidget)
        QnGlNativePainting::end(painter);

    return result;
}

bool QnMediaResourceWidget::capabilityButtonsAreVisible() const
{
    static const Qn::Permissions kInputPermissions = Qn::Permission::SoftTriggerPermission
            | Qn::Permission::DeviceInputPermission
            | Qn::Permission::WritePtzPermission;

    return !d->isPreviewSearchLayout
        // Video wall has userInput permission but no actual user.
        && !qnRuntime->isVideoWallMode()
        && d->camera
        && (accessController()->permissions(d->camera) & kInputPermissions) != 0;
}

void QnMediaResourceWidget::updateCapabilityButtons() const
{
    if (capabilityButtonsAreVisible())
    {
        m_objectTrackingButtonController->createButtons();
        m_buttonController->createButtons();
    }
    else
    {
        m_objectTrackingButtonController->removeButtons();
        m_buttonController->removeButtons();
    }
}

void QnMediaResourceWidget::updateTwoWayAudioButton() const
{
    const bool twoWayAudioButtonVisible =
        capabilityButtonsAreVisible()
        && !d->camera->hasFlags(Qn::cross_system)
        && d->camera->audioOutputDevice()->hasTwoWayAudio()
        && d->camera->isTwoWayAudioEnabled()
        && !d->camera->isIntercom();

    if (twoWayAudioButtonVisible)
        m_buttonController->createTwoWayAudioButton();
    else
        m_buttonController->removeTwoWayAudioButton();
}

void QnMediaResourceWidget::updateIntercomButtons()
{
    const bool intercomButtonVisible =
        capabilityButtonsAreVisible()
        && !d->camera->hasFlags(Qn::cross_system)
        && d->camera->isIntercom();

    if (intercomButtonVisible)
        m_buttonController->createIntercomButtons();
    else
        m_buttonController->removeIntercomButtons();
}

void QnMediaResourceWidget::setupHud()
{
    static const int kScrollLineHeight = 8;

    m_objectTrackingButtonContainer = new QnScrollableItemsWidget(m_hudOverlay->left());
    m_objectTrackingButtonContainer->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
    m_objectTrackingButtonContainer->setLineHeight(kScrollLineHeight);
    m_objectTrackingButtonContainer->setContentsMargins(8, 0, 0, 0);
    setOverlayWidgetVisible(m_objectTrackingButtonContainer, false, /*animate=*/false);

    m_objectTrackingButtonController->setButtonContainer(m_objectTrackingButtonContainer);
    connect(
        m_objectTrackingButtonController, &ObjectTrackingButtonController::requestButtonCreation,
        this, &QnMediaResourceWidget::updateCameraButtons);

    auto leftLayout = new QGraphicsLinearLayout(Qt::Horizontal, m_hudOverlay->left());
    leftLayout->addItem(m_objectTrackingButtonContainer);

    m_triggersContainer = new QnScrollableItemsWidget(m_hudOverlay->right());
    m_triggersContainer->setAlignment(Qt::AlignRight | Qt::AlignBottom);
    m_triggersContainer->setLineHeight(kScrollLineHeight);
    m_buttonController->setButtonContainer(m_triggersContainer);

    qreal right = 0.0;
    m_hudOverlay->content()->getContentsMargins(nullptr, nullptr, &right, nullptr);
    const auto triggersMargin = kTriggersMargin - right;

    m_triggersContainer->setSpacing(kTriggersSpacing);
    m_triggersContainer->setMaximumWidth(kTriggerButtonSize + triggersMargin);
    m_triggersContainer->setContentsMargins(0, 0, triggersMargin, 0);
    setOverlayWidgetVisible(m_triggersContainer, false, /*animate=*/false);

    m_compositeOverlay = new QnGraphicsStackedWidget(m_hudOverlay->right());

    m_textOverlayWidget = new QnScrollableTextItemsWidget(m_compositeOverlay);
    m_textOverlayWidget->setAlignment(Qt::AlignRight | Qt::AlignBottom);
    m_textOverlayWidget->setLineHeight(kScrollLineHeight);
    m_textOverlayWidget->setIgnoreMinimumHeight(false);

    m_bookmarksContainer = new QnScrollableTextItemsWidget(m_compositeOverlay);
    m_bookmarksContainer->setAlignment(Qt::AlignRight | Qt::AlignBottom);
    m_bookmarksContainer->setLineHeight(kScrollLineHeight);
    m_bookmarksContainer->hide();

    m_compositeOverlay->addWidget(m_textOverlayWidget);
    m_compositeOverlay->addWidget(m_bookmarksContainer);

    auto rightLayout = new QGraphicsLinearLayout(Qt::Horizontal, m_hudOverlay->right());
    rightLayout->addItem(m_compositeOverlay);
    rightLayout->addItem(m_triggersContainer);

    m_compositeOverlay->stackBefore(m_triggersContainer);

    setOverlayWidgetVisible(m_hudOverlay->right(), true, /*animate=*/false);

    // During the widget destruction the following scenario occurs: m_hudOverlay is destroyed, then
    // triggers container resizes, and we've got a crash in updateTriggersMinHeight().
    QPointer<QnViewportBoundWidget> content(m_hudOverlay->content());
    QPointer<QnViewportBoundWidget> rewindContent(m_rewindOverlay->content());
    QPointer<QnScrollableItemsWidget> triggersContainer(m_triggersContainer);
    const auto updateTriggersMinHeight =
        [content, triggersContainer, rewindContent]()
        {
            if (!content || !triggersContainer || !rewindContent)
                return;

            // Calculate minimum height for downscale no more than kMaxDownscaleFactor times.
            static const qreal kMaxDownscaleFactor = 2.0;
            const qreal available = content->fixedSize().height() / content->sceneScale();
            const qreal desired = triggersContainer->effectiveSizeHint(Qt::PreferredSize).height();
            const qreal extra = content->size().height() - triggersContainer->size().height();
            const qreal min = qMin(desired, available * kMaxDownscaleFactor - extra);
            triggersContainer->setMinimumHeight(min);

            rewindContent->setScale(content->sceneScale());
        };

    connect(m_hudOverlay->content(), &QnViewportBoundWidget::scaleChanged,
        this, updateTriggersMinHeight);

    connect(m_triggersContainer, &QnScrollableItemsWidget::contentHeightChanged,
        this, updateTriggersMinHeight);

    connect(m_rewindOverlay->content(), &QnViewportBoundWidget::scaleChanged,
        this, updateTriggersMinHeight);
}

void QnMediaResourceWidget::updateHud(bool animate)
{
    base_type::updateHud(animate);
    setOverlayWidgetVisible(
        m_objectTrackingButtonContainer,
        isOverlayWidgetVisible(titleBar()),
        animate);
    setOverlayWidgetVisible(m_triggersContainer, isOverlayWidgetVisible(titleBar()), animate);
    if (m_roiFiguresOverlayWidget)
    {
        setOverlayWidgetVisible(m_roiFiguresOverlayWidget,
            qFuzzyIsNull(m_statusOverlay->opacity()) && isRoiVisible(),
            animate);
    }
    updateCompositeOverlayMode();
}

void QnMediaResourceWidget::updateCameraButtons()
{
    updateCapabilityButtons();
    updateTwoWayAudioButton();
    updateIntercomButtons();
}

bool QnMediaResourceWidget::animationAllowed() const
{
    return WindowContextAware::display()->animationAllowed();
}

void QnMediaResourceWidget::resumeHomePtzController()
{
    if (auto homePtzController = findPtzController<QnFisheyeHomePtzController>(m_ptzController))
    {
        if (options().testFlag(DisplayDewarped) && display()->camDisplay()->isRealTimeSource())
            homePtzController->resume();
    }
}

bool QnMediaResourceWidget::forceShowPosition() const
{
    // In videowall mode position item info must always be visible if item is not in live.
    if (!qnRuntime->isVideoWallMode())
        return false;

    const bool isPlayingArchive = d->display()
        && d->display()->camDisplay()
        && !d->display()->camDisplay()->isRealTimeSource();

    return isPlayingArchive;
}

const MotionSelection& QnMediaResourceWidget::motionSelection() const
{
    return m_motionSelection;
}

bool QnMediaResourceWidget::isMotionSelectionEmpty() const
{
    return std::all_of(
        m_motionSelection.cbegin(),
        m_motionSelection.cend(),
         [](const QRegion& r) { return r.isEmpty(); });
}

void QnMediaResourceWidget::addToMotionSelection(const QRect &gridRect)
{
    // Just send changed() if gridRect is empty.
    if (gridRect.isEmpty())
    {
        emit motionSelectionChanged();
        return;
    }

    bool changed = false;

    for (int i = 0; i < channelCount(); ++i)
    {
        QRect rect = gridRect
            .translated(-channelGridOffset(i))
            .intersected(QRect(0, 0, MotionGrid::kWidth, MotionGrid::kHeight));
        if (rect.isEmpty())
            continue;

        QRegion selection;
        selection += rect;

        if (!selection.isEmpty())
        {
            if (changed)
            {
                /* In this case we don't need to bother comparing old & new selection regions. */
                m_motionSelection[i] += selection;
            }
            else
            {
                QRegion oldSelection = m_motionSelection[i];
                m_motionSelection[i] += selection;
                changed |= (oldSelection != m_motionSelection[i]);
            }
        }
    }

    if (changed)
    {
        invalidateMotionSelectionCache();
        emit motionSelectionChanged();
    }
}

void QnMediaResourceWidget::clearMotionSelection(bool sendMotionChanged)
{
    if (isMotionSelectionEmpty())
        return;

    for (int i = 0; i < m_motionSelection.size(); ++i)
        m_motionSelection[i] = QRegion();

    invalidateMotionSelectionCache();

    if (sendMotionChanged)
        emit motionSelectionChanged();
}

void QnMediaResourceWidget::setMotionSelection(const MotionSelection& motionSelection)
{
    if (motionSelection.empty())
    {
        clearMotionSelection();
        return;
    }

    if (motionSelection.size() != m_motionSelection.size())
    {
        NX_ASSERT(false, "invalid motion selection channel count");
        return;
    }

    if (m_motionSelection == motionSelection)
        return;

    m_motionSelection = motionSelection;
    invalidateMotionSelectionCache();
    emit motionSelectionChanged();
}

void QnMediaResourceWidget::ensureMotionSelectionCache()
{
    if (m_motionSelectionCacheValid)
        return;

    for (int i = 0; i < channelCount(); ++i)
    {
        QPainterPath path;
        path.addRegion(m_motionSelection[i]);
        m_motionSelectionPathCache[i] = path.simplified();
    }
}

void QnMediaResourceWidget::invalidateMotionSelectionCache()
{
    m_motionSelectionCacheValid = false;
}

void QnMediaResourceWidget::setDisplay(const QnResourceDisplayPtr& display)
{
    if (d->display())
        d->display()->removeRenderer(m_renderer);

    d->setDisplay(display);

    if (display)
    {
        connect(display->camDisplay(), &QnCamDisplay::stillImageChanged, this,
            &QnMediaResourceWidget::updateButtonsVisibility);

        connect(display->camDisplay(), &QnCamDisplay::liveMode, this,
            &QnMediaResourceWidget::at_camDisplay_liveChanged);
        connect(d->resource.get(), &QnResource::videoLayoutChanged, this,
            &QnMediaResourceWidget::at_videoLayoutChanged);

        if (const auto archiveReader = display->archiveReader())
        {
            connect(archiveReader, &QnAbstractArchiveStreamReader::streamPaused,
                this, &QnMediaResourceWidget::updateButtonsVisibility);
            connect(archiveReader, &QnAbstractArchiveStreamReader::streamResumed,
                this, &QnMediaResourceWidget::updateButtonsVisibility);
        }

        setChannelLayout(display->videoLayout());
        m_renderer->setChannelCount(display->videoLayout()->channelCount());
        display->addRenderer(m_renderer);
        updateCustomAspectRatio();
    }
    else
    {
        setChannelLayout(QnConstResourceVideoLayoutPtr(new QnDefaultResourceVideoLayout()));
        m_renderer->setChannelCount(0);
    }

    if (const auto consumer = d->motionMetadataConsumer())
        display->addMetadataConsumer(consumer);
    if (const auto consumer = d->analyticsMetadataConsumer())
        display->addMetadataConsumer(consumer);

    emit displayChanged();
}

void QnMediaResourceWidget::at_videoLayoutChanged()
{
    setChannelLayout(d->display()->videoLayout());
}

void QnMediaResourceWidget::updateIconButton()
{
    if (!d->camera || d->camera->hasFlags(Qn::virtual_camera))
        return;

    const auto icon = m_recordingStatusHelper->icon();
    m_hudOverlay->playbackPositionItem()->setRecordingIcon(icon.pixmap(QSize(12, 12)));
}

void QnMediaResourceWidget::updateRendererEnabled()
{
    if (d->resource->hasFlags(Qn::still_image))
        return;

    for (int channel = 0; channel < channelCount(); channel++)
        m_renderer->setEnabled(channel, !exposedRect(channel, true, true, false).isEmpty());
}

nx::vms::api::ImageCorrectionData QnMediaResourceWidget::imageEnhancement() const
{
    return item()->imageEnhancement();
}

void QnMediaResourceWidget::setImageEnhancement(
    const nx::vms::api::ImageCorrectionData& imageEnhancement)
{
    m_toggleImageEnhancementAction->setChecked(imageEnhancement.enabled);
    item()->setImageEnhancement(imageEnhancement);
    m_renderer->setImageCorrection(imageEnhancement);
}

// -------------------------------------------------------------------------- //
// Painting
// -------------------------------------------------------------------------- //
void QnMediaResourceWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    base_type::paint(painter, option, widget);

    updateRendererEnabled();

    for (int channel = 0; channel < channelCount(); channel++)
        m_renderer->setDisplayedRect(channel, exposedRect(channel, true, true, true));

    updateInfoTextLater();
    traceFps();
}

void QnMediaResourceWidget::traceFps() const
{
    if (!d->traceFpsTimer.hasExpired(1s))
        return;
    d->traceFpsTimer.restart();

    NX_TRACE_COUNTER_ID("Item FPS", (int64_t) this).args(
        {{ "FPS", d->getStatisticsFps(channelCount()) }});
}

Qn::RenderStatus QnMediaResourceWidget::paintChannelBackground(
    QPainter* painter,
    int channel,
    const QRectF& channelRect,
    const QRectF& paintRect)
{
    Qn::RenderStatus result = Qn::NothingRendered;

    if (!placeholderPixmap().isNull() && zoomTargetWidget() && !zoomRect().isValid())
    {
        const PainterTransformScaleStripper scaleStripper(painter);

        result = m_renderer->discardFrame(channel);
        m_paintedChannels[channel] = true;

        painter->drawPixmap(
            scaleStripper.mapRect(paintRect),
            placeholderPixmap(),
            placeholderPixmap().rect());

        return result;
    }
    else
    {
        const QRectF sourceSubRect = Geometry::toSubRect(channelRect, paintRect);
        const PainterTransformScaleStripper scaleStripper(painter);
        result = paintVideoTexture(painter,
            channel,
            sourceSubRect,
            scaleStripper.mapRect(paintRect));
    }

    const bool videoFramePresent = result == Qn::NewFrameRendered || result == Qn::OldFrameRendered;
    if (videoFramePresent)
    {
        integrations::paintVideoOverlays(this, painter);
    }
    else
    {
        base_type::paintChannelBackground(painter, channel, channelRect, paintRect);
    }

    return result;
}

void QnMediaResourceWidget::paintChannelForeground(QPainter *painter, int channel, const QRectF &rect)
{
    const auto timestamp = m_renderer->lastDisplayedTimestamp(channel);

    if (options().testFlag(DisplayMotion))
    {
        ensureMotionSelectionCache();

        paintMotionGrid(painter, channel, rect,
            d->motionMetadataProvider->metadata(timestamp, channel));

        // Motion search region.
        const bool isActiveWidget = navigator()->currentMediaWidget() == this;
        if (isActiveWidget && !m_motionSelection[channel].isEmpty())
        {
            QColor color = nx::vms::client::core::colorTheme()->color("camera.motion");
            paintFilledRegionPath(painter, rect, m_motionSelectionPathCache[channel], color, color);
        }
    }

    if (isAnalyticsModeEnabled())
    {
        // Avoid adding/removing scene items inside scene painting functions.
        executeDelayedParented(
            [this, timestamp, channel]()
            {
                d->analyticsController->updateAreas(timestamp, channel);
            },
            this);
        paintAnalyticsObjectsDebugOverlay(duration_cast<milliseconds>(timestamp), painter, rect);
    }

    if (!isZoomWindow() && channelCount() > 1)
        paintWatermark(painter, rect);

    if (ini().showVideoQualityOverlay
        && hasVideo()
        && !d->resource->hasFlags(Qn::local))
    {
        // Predefined colors for debug purposes under the ini flag.
        QColor overlayColor = m_renderer->isLowQualityImage(0)
            ? Qt::red
            : Qt::green;
        overlayColor = toTransparent(overlayColor, 0.5);
        const PainterTransformScaleStripper scaleStripper(painter);
        painter->fillRect(scaleStripper.mapRect(rect), overlayColor);
    }

    if (ini().showCameraCrosshair && hasVideo())
        drawCrosshair(painter, rect);
}

void QnMediaResourceWidget::paintEffects(QPainter* painter)
{
    base_type::paintEffects(painter);

    if (isZoomWindow() || channelCount() == 1)
        paintWatermark(painter, rect());
}

void QnMediaResourceWidget::paintMotionGrid(QPainter *painter, int channel, const QRectF &rect, const QnMetaDataV1Ptr &motion)
{
    // 5-7 fps

    qreal xStep = rect.width() / MotionGrid::kWidth;
    qreal yStep = rect.height() / MotionGrid::kHeight;

    QVector<QPointF> gridLines[2];

    if (motion && motion->channelNumber == (quint32)channel)
    {
        // 2-3 fps

        motion->removeMotion(reinterpret_cast<const simd128i*>(d->motionSkipMask(channel)));

        /* Horizontal lines. */
        for (int y = 1; y < MotionGrid::kHeight; ++y)
        {
            bool isMotion = motion->isMotionAt(0, y - 1) || motion->isMotionAt(0, y);
            gridLines[isMotion] << QPointF(0, y * yStep);
            int x = 1;
            while (x < MotionGrid::kWidth)
            {
                while (x < MotionGrid::kWidth && isMotion == (motion->isMotionAt(x, y - 1) || motion->isMotionAt(x, y)))
                    x++;
                gridLines[isMotion] << QPointF(x * xStep, y * yStep);
                if (x < MotionGrid::kWidth)
                {
                    isMotion = !isMotion;
                    gridLines[isMotion] << QPointF(x * xStep, y * yStep);
                }
            }
        }

        /* Vertical lines. */
        for (int x = 1; x < MotionGrid::kWidth; ++x)
        {
            bool isMotion = motion->isMotionAt(x - 1, 0) || motion->isMotionAt(x, 0);
            gridLines[isMotion] << QPointF(x * xStep, 0);
            int y = 1;
            while (y < MotionGrid::kHeight)
            {
                while (y < MotionGrid::kHeight && isMotion == (motion->isMotionAt(x - 1, y) || motion->isMotionAt(x, y)))
                    y++;
                gridLines[isMotion] << QPointF(x * xStep, y * yStep);
                if (y < MotionGrid::kHeight)
                {
                    isMotion = !isMotion;
                    gridLines[isMotion] << QPointF(x * xStep, y * yStep);
                }
            }
        }
    }
    else
    {
        for (int x = 1; x < MotionGrid::kWidth; ++x)
            gridLines[0] << QPointF(x * xStep, 0.0) << QPointF(x * xStep, rect.height());
        for (int y = 1; y < MotionGrid::kHeight; ++y)
            gridLines[0] << QPointF(0.0, y * yStep) << QPointF(rect.width(), y * yStep);
    }

    QnScopedPainterTransformRollback transformRollback(painter);
    painter->translate(rect.topLeft());

    QnScopedPainterPenRollback penRollback(painter);
    painter->setPen(QPen(
        nx::vms::client::core::colorTheme()->color("camera.motionGrid.background"), 0.0));
    painter->drawLines(gridLines[0]);

    painter->setPen(QPen(
        nx::vms::client::core::colorTheme()->color("camera.motionGrid.foreground"), 0.0));
    painter->drawLines(gridLines[1]);
}

void QnMediaResourceWidget::paintFilledRegionPath(QPainter *painter, const QRectF &rect, const QPainterPath &path, const QColor &color, const QColor &penColor)
{
    // 4-6 fps

    QnScopedPainterTransformRollback transformRollback(painter); Q_UNUSED(transformRollback);
    QnScopedPainterBrushRollback brushRollback(painter, color); Q_UNUSED(brushRollback);
    QnScopedPainterPenRollback penRollback(painter); Q_UNUSED(penRollback);

    painter->translate(rect.topLeft());
    painter->scale(rect.width() / MotionGrid::kWidth, rect.height() / MotionGrid::kHeight);
    painter->setPen(QPen(penColor, 0.0));
    painter->drawPath(path);
}

void QnMediaResourceWidget::paintProgress(QPainter* painter, const QRectF& rect, int progress)
{
    QnScopedPainterBrushRollback brushRollback(painter, Qt::transparent);
    QnScopedPainterPenRollback penRollback(painter, QPen(palette().color(QPalette::Light)));

    const PainterTransformScaleStripper scaleStripper(painter);

    const auto& widgetRect = scaleStripper.mapRect(rect);

    constexpr int kProgressBarPadding = 32;
    constexpr int kProgressBarHeight = 32;

    QRectF progressBarRect(
        widgetRect.x() + kProgressBarPadding, widgetRect.center().y() - kProgressBarHeight / 2,
        widgetRect.width() - kProgressBarPadding * 2, kProgressBarHeight);

    painter->drawRect(progressBarRect);
    painter->fillRect(
        Geometry::subRect(progressBarRect, QRectF(0, 0, progress / 100.0, 1)),
        palette().highlight());
}

void QnMediaResourceWidget::paintAnalyticsObjectsDebugOverlay(
    std::chrono::milliseconds timestamp,
    QPainter* painter,
    const QRectF& rect)
{
    if (!d->analyticsMetadataLogParser)
        return;

    static milliseconds prevRenderedFrameTimestamp = milliseconds::zero();
    static milliseconds currentRenderedFrameTimestamp = milliseconds::zero();

    if (currentRenderedFrameTimestamp != timestamp)
    {
        prevRenderedFrameTimestamp = currentRenderedFrameTimestamp;
        currentRenderedFrameTimestamp = timestamp;
    }

    if (prevRenderedFrameTimestamp != milliseconds::zero()
        && prevRenderedFrameTimestamp < currentRenderedFrameTimestamp)
    {
        const auto packets = d->analyticsMetadataLogParser->packetsBetweenTimestamps(
            prevRenderedFrameTimestamp.count(),
            currentRenderedFrameTimestamp.count());

        const PainterTransformScaleStripper scaleStripper(painter);
        const auto widgetRect = scaleStripper.mapRect(rect);

        for (const auto& packet: packets)
        {
            for (const auto& rect: packet->rects)
            {
                const auto absoluteObjectRect = Geometry::subRect(widgetRect, rect);
                // Predefined color used for debug purposes in the debug method.
                const QColor overlayColor = toTransparent(Qt::green, 0.3);
                painter->fillRect(absoluteObjectRect, overlayColor);
            }
        }
    }
}

void QnMediaResourceWidget::paintWatermark(QPainter* painter, const QRectF& rect)
{
    if (!m_watermarkPainter->watermark().visible())
        return;

    const auto imageRotation = defaultFullRotation();
    if (imageRotation == 0)
    {
        m_watermarkPainter->drawWatermark(painter, rect);
    }
    else
    {
        // We have implicit camera rotation due to default rotation and/or dewarping procedure.
        // We should rotate watermark to make it appear in appropriate orientation.
        const auto oldTransform = painter->transform();
        const auto transform =
            QTransform().translate(rect.center().x(), rect.center().y()).rotate(-imageRotation);
        painter->setTransform(transform, true);
        m_watermarkPainter->drawWatermark(painter, transform.inverted().mapRect(rect));
        painter->setTransform(oldTransform);
    }
}

QnPtzControllerPtr QnMediaResourceWidget::ptzController() const
{
    return m_ptzController;
}

bool QnMediaResourceWidget::supportsBasicPtz() const
{
    return d->supportsBasicPtz();
}

bool QnMediaResourceWidget::canControlPtzMove() const
{
    return d->supportsPtzCapabilities(
        Ptz::ContinuousPanCapability
        | Ptz::ContinuousTiltCapability
        | Ptz::ContinuousRotationCapability);
}

bool QnMediaResourceWidget::canControlPtzFocus() const
{
    return d->supportsPtzCapabilities(Ptz::ContinuousFocusCapability);
}

bool QnMediaResourceWidget::canControlPtzZoom() const
{
    return d->supportsPtzCapabilities(Ptz::ContinuousZoomCapability);
}

nx::vms::api::dewarping::MediaData QnMediaResourceWidget::dewarpingParams() const
{
    return m_dewarpingParams;
}

void QnMediaResourceWidget::setDewarpingParams(const nx::vms::api::dewarping::MediaData& params)
{
    if (m_dewarpingParams == params)
        return;

    m_dewarpingParams = params;
    handleDewarpingParamsChanged();

    emit dewarpingParamsChanged();
}

bool QnMediaResourceWidget::dewarpingApplied() const
{
    return m_dewarpingParams.enabled && (item()->dewarpingParams().enabled || isZoomWindow());
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
int QnMediaResourceWidget::helpTopicAt(const QPointF &) const
{

    auto isIoModule = [this]()
        {
            return (d->isIoModule
                && m_ioModuleOverlayWidget
                && overlayWidgetVisibility(m_ioModuleOverlayWidget) == OverlayVisibility::Visible);
        };

    if (action(action::ToggleShowreelModeAction)->isChecked())
        return HelpTopic::Id::MainWindow_Scene_TourInProgress;

    const Qn::ResourceStatusOverlay statusOverlay = statusOverlayController()->statusOverlay();

    if (statusOverlay == Qn::AnalogWithoutLicenseOverlay)
        return HelpTopic::Id::Licenses;

    if (statusOverlay == Qn::OfflineOverlay || statusOverlay == Qn::OldFirmwareOverlay)
        return HelpTopic::Id::MainWindow_MediaItem_Diagnostics;

    if (statusOverlay == Qn::UnauthorizedOverlay)
        return HelpTopic::Id::MainWindow_MediaItem_Unauthorized;

    if (statusOverlay == Qn::AccessDeniedOverlay)
        return HelpTopic::Id::MainWindow_MediaItem_AccessDenied;

    if (statusOverlay == Qn::IoModuleDisabledOverlay)
        return HelpTopic::Id::IOModules;

    if (options().testFlag(ControlPtz))
    {
        if (m_dewarpingParams.enabled)
            return HelpTopic::Id::MainWindow_MediaItem_Dewarping;

        return HelpTopic::Id::MainWindow_MediaItem_Ptz;
    }

    if (isZoomWindow())
        return HelpTopic::Id::MainWindow_MediaItem_ZoomWindows;

    if (options().testFlag(DisplayMotion))
        return HelpTopic::Id::MainWindow_MediaItem_SmartSearch;

    if (isIoModule())
        return HelpTopic::Id::IOModules;

    if (d->resource->hasFlags(Qn::local))
        return HelpTopic::Id::MainWindow_MediaItem_Local;

    if (d->camera && d->camera->isDtsBased())
        return HelpTopic::Id::MainWindow_MediaItem_AnalogCamera;

    return HelpTopic::Id::MainWindow_MediaItem;
}

void QnMediaResourceWidget::channelLayoutChangedNotify()
{
    base_type::channelLayoutChangedNotify();

    m_motionSelection.resize(channelCount());
    m_motionSelectionPathCache.resize(channelCount());
    m_paintedChannels.resize(channelCount());
    updateAspectRatio();
}

void QnMediaResourceWidget::channelScreenSizeChangedNotify()
{
    base_type::channelScreenSizeChangedNotify();

    m_renderer->setChannelScreenSize(channelScreenSize());
}

void QnMediaResourceWidget::optionsChangedNotify(Options changedFlags)
{
    const auto saveItemDataState =
        [this](Qn::ItemDataRole role)
        {
            m_savedItemDataState.insert(role, item()->data(role));
        };

    const auto restoreItemDataState =
        [this]()
        {
            for (const auto role: m_savedItemDataState.keys())
                item()->setData(role, m_savedItemDataState.take(role));
        };

    const bool motionSearchChanged = changedFlags.testFlag(DisplayMotion);
    const bool motionSearchEnabled = options().testFlag(DisplayMotion);
    if (motionSearchChanged)
    {
        d->setMotionEnabled(motionSearchEnabled);

        titleBar()->rightButtonsBar()->setButtonsChecked(
            Qn::MotionSearchButton, motionSearchEnabled);

        if (motionSearchEnabled)
        {
            // Since source frame area is expected for motion search, dewarping will be
            // disabled, also Camera Hotspots will be hidden, since Hotspot items are mouse
            // grabbers and may interfere selection process. Dewarped view params and Camera
            // Hotspots enabled state are saved before engaging motion selection process.

            if (item()->dewarpingParams().enabled)
                saveItemDataState(Qn::ItemImageDewarpingRole);

            if (item()->displayHotspots())
                saveItemDataState(Qn::ItemDisplayHotspotsRole);

            titleBar()->rightButtonsBar()->setButtonsChecked(
                Qn::PtzButton | Qn::FishEyeButton | Qn::ZoomWindowButton | Qn::HotspotsButton,
                false);

            setAreaSelectionType(AreaType::motion);
        }
        else
        {
            unsetAreaSelectionType(AreaType::motion);
        }

        updateTimelineVisibility();

        setOption(WindowResizingForbidden, motionSearchEnabled);

        emit motionSearchModeEnabled(motionSearchEnabled);
    }

    const bool ptzChanged = changedFlags.testFlag(ControlPtz);
    const bool ptzEnabled = options().testFlag(ControlPtz);
    if (ptzChanged && ptzEnabled)
    {
        titleBar()->rightButtonsBar()->setButtonsChecked(
            Qn::MotionSearchButton | Qn::ZoomWindowButton, false);

        if (!options().testFlag(DisplayDewarped)) //< True PTZ.
        {
            if (item()->displayHotspots())
                saveItemDataState(Qn::ItemDisplayHotspotsRole);

            titleBar()->rightButtonsBar()->setButtonsChecked(Qn::HotspotsButton, false);
        }
    }

    if (motionSearchChanged || ptzChanged)
    {
        static constexpr int kInvalidKeyboardModifier = 1;

        if (!motionSearchEnabled && !ptzEnabled)
            restoreItemDataState();

        if (options().testFlag(ControlPtz))
            setProperty(Qn::MotionSelectionModifiers, kInvalidKeyboardModifier); //< Disable.
        else if (options().testFlag(DisplayMotion))
            setProperty(Qn::MotionSelectionModifiers, 0); //< Enable with no modifier.
        else
            setProperty(Qn::MotionSelectionModifiers, QVariant()); //< Enable with default modifier.
    }

    if (changedFlags.testFlag(DisplayAnalyticsObjects))
        setAnalyticsObjectsVisible(options().testFlag(DisplayAnalyticsObjects), false);

    if (changedFlags.testFlag(DisplayRoi))
        setRoiVisible(options().testFlag(DisplayRoi), false);

    if (changedFlags.testFlag(FullScreenMode))
        updateTimelineVisibility();

    if (changedFlags.testFlags(DisplayHotspots))
    {
        const auto hotspotsButton = titleBar()->rightButtonsBar()->button(Qn::HotspotsButton);
        setButtonCheckedSilent(hotspotsButton, options().testFlag(DisplayHotspots));
    }

    base_type::optionsChangedNotify(changedFlags);
}

QString QnMediaResourceWidget::calculateDetailsText() const
{
    if (!d->updateDetailsTimer.hasExpired(kUpdateDetailsInterval))
        return d->currentDetailsText;

    qreal fps = 0.0;
    qreal mbps = 0.0;

    for (int i = 0; i < channelCount(); i++)
    {
        const auto provider = d->display()->mediaProvider();
        const QnMediaStreamStatistics *statistics = provider->getStatistics(i);
        if (provider->isConnectionLost()) // TODO: #sivanov Check does not work.
            continue;
        fps = qMax(fps, static_cast<qreal>(statistics->getFrameRate()));
        mbps += statistics->bitrateBitsPerSecond() / 1024.0 / 1024.0;
    }

    QString codecString;
    if (CodecParametersConstPtr codecContext = d->display()->mediaProvider()->getCodecContext())
        codecString = codecContext->getCodecName();
    if (m_renderer->isHardwareDecoderUsed(0))
        codecString += " (HW)";

    QString hqLqString;
    QString protocolString;
    if (hasVideo() && !d->resource->hasFlags(Qn::local))
    {
        hqLqString = (m_renderer->isLowQualityImage(0)) ? tr("Lo-Res") : tr("Hi-Res");

        if (const auto archiveDelegate = d->display()->archiveReader()->getArchiveDelegate())
        {
            const auto protocol = archiveDelegate->protocol();
            switch (protocol)
            {
                case nx::network::Protocol::udt:
                    // aka UDP hole punching.
                    protocolString = " (N)";
                    break;

                case nx::network::cloud::Protocol::relay:
                    // relayed connection (aka proxy).
                    protocolString = " (P)";
                    break;

                default:
                    // Regular connection.
                    break;
            }
        }
    }

    QString result;
    const auto addDetailsString =
        [&result](const QString& value)
        {
            if (!value.isEmpty())
            {
                result.append(html::styledParagraph(
                    value,
                    fontConfig()->small().pixelSize(),
                    /*bold*/ true));
            }
        };

    if (hasVideo())
    {
        const QSize channelResolution = d->display()->camDisplay()->getRawDataSize();
        const QSize videoLayout = d->mediaResource->getVideoLayout()->size();
        const QSize actualResolution = Geometry::cwiseMul(channelResolution, videoLayout);

        addDetailsString(QString("%1x%2")
            .arg(actualResolution.width())
            .arg(actualResolution.height()));
        addDetailsString(QString("%1fps").arg(fps, 0, 'f', 2));
    }

    const QString bandwidthString = QString("%1Mbps").arg(mbps, 0, 'f', 2);
    addDetailsString(bandwidthString + protocolString);
    addDetailsString(codecString);
    addDetailsString(hqLqString);

    d->currentDetailsText = result;
    d->updateDetailsTimer.restart();

    return d->currentDetailsText;
}

void QnMediaResourceWidget::setPosition(qint64 timestampMs)
{
    const auto reader = display()->archiveReader();
    if (!reader)
        return;

    const auto timestampUs = timestampMs < 0 || timestampMs == DATETIME_NOW
        ? timestampMs //< Special time value.
        : timestampMs * 1000;

    if (reader->isPaused())
        reader->jumpTo(timestampUs, timestampUs); //< Precise jump to avoid timeline blinks.
    else
        reader->jumpTo(timestampUs, 0);
}

std::chrono::milliseconds QnMediaResourceWidget::position() const
{
    return milliseconds(m_posUtcMs);
}

void QnMediaResourceWidget::setSpeed(double value)
{
    if (qFuzzyEquals(speed(), value))
        return;

    const auto reader = display()->archiveReader();
    if (!reader)
        return;

    reader->setSpeed(value);
    emit speedChanged();
}

double QnMediaResourceWidget::speed() const
{
    const auto reader = d->display()->archiveReader();
    return !reader || reader->isMediaPaused()
        ? 0.0
        : reader->getSpeed();
}

QnIOModuleMonitorPtr QnMediaResourceWidget::getIOModuleMonitor()
{
    return d->ioModuleMonitor;
}

void QnMediaResourceWidget::updateCurrentUtcPosMs()
{
    const qint64 usec = getUtcCurrentTimeUsec();
    qint64 newPos = DATETIME_INVALID;
    if (!isSpecialDateTimeValueUsec(usec))
        newPos = usec / kMicroInMilliSeconds;

    if (newPos == m_posUtcMs)
        return;

    m_posUtcMs = newPos;
    emit positionChanged(m_posUtcMs);
}

QString QnMediaResourceWidget::calculatePositionText() const
{
    /* Do not show time for regular media files. */
    if (!d->resource->flags().testFlag(Qn::utc))
        return QString();

    const auto extractTime =
        [this](qint64 dateTimeUsec)
        {
            if (isSpecialDateTimeValueUsec(dateTimeUsec))
                return QString();

            const auto dateTimeMs = dateTimeUsec / kMicroInMilliSeconds;
            QDateTime dt = QDateTime::fromMSecsSinceEpoch(dateTimeMs, displayTimeZone(resource()));

            const QString result = nx::vms::time::toString(dt);

            return ini().showPreciseItemTimestamps
                ? nx::format("%1<br/>%2 us").args(result, dateTimeUsec).toQString() //< Dev option.
                : result;

            return result;
        };

    const QString timeString = (d->display()->camDisplay()->isRealTimeSource()
            ? tr("LIVE")
            : extractTime(getUtcCurrentTimeUsec()));

    static const int kPositionTextPixelSize = 12;

    return html::styledParagraph(
        timeString,
        kPositionTextPixelSize,
        /*bold*/ true);
}

QString QnMediaResourceWidget::calculateTitleText() const
{
    QString resourceName = base_type::calculateTitleText();

    QnPtzObject activeObject;
    QString activeObjectName;
    if (m_ptzController->getActiveObject(&activeObject)
        && activeObject.type == Qn::TourPtzObject
        && getPtzObjectName(m_ptzController, activeObject, &activeObjectName))
    {
        return tr("%1 (Tour \"%2\" is active)").arg(resourceName).arg(activeObjectName);
    }
    return resourceName;
}

int QnMediaResourceWidget::calculateButtonsVisibility() const
{
    int result = base_type::calculateButtonsVisibility();

    if (ini().developerMode)
        result |= Qn::DbgScreenshotButton;

    if (d->hasVideo && !base_type::resource()->hasFlags(Qn::still_image))
    {
        const auto requiredPermission = d->isPlayingLive()
            ? Qn::ViewLivePermission
            : Qn::ViewFootagePermission;

        if (ResourceAccessManager::hasPermissions(d->resource, requiredPermission))
            result |= Qn::ScreenshotButton;
    }

    if (canDisplayHotspots() && d->camera->cameraHotspotsEnabled())
        result |= Qn::HotspotsButton;

    if (isZoomWindow())
    {
        result |= Qn::ZoomStatusIconButton;
        return result;
    }

    if (d->hasVideo && base_type::resource()->hasFlags(Qn::motion))
        result |= Qn::MotionSearchButton;

    if (d->hasVideo
        && d->camera
        && !workbench()->currentLayout()->resource()->hasFlags(Qn::cross_system)
        && d->isAnalyticsSupported
        && ResourceAccessManager::hasPermissions(d->camera, Qn::ViewFootagePermission))
    {
        result |= Qn::ObjectSearchButton;
    }

    if (d->supportsBasicPtz())
        result |= Qn::PtzButton;

    if (m_dewarpingParams.enabled)
    {
        result |= Qn::FishEyeButton;
        result &= ~Qn::PtzButton;
    }

    if (d->hasVideo && d->isIoModule)
        result |= Qn::IoModuleButton;

    if (d->hasVideo && !qnRuntime->lightMode().testFlag(Qn::LightModeNoZoomWindows))
    {
        const Qn::Permissions permissions = ResourceAccessManager::permissions(layoutResource());
        if (permissions.testFlag(Qn::WritePermission)
            && permissions.testFlag(Qn::AddRemoveItemsPermission)
            && !tourIsRunning(windowContext()->workbenchContext()))
        {
            result |= Qn::ZoomWindowButton;
        }
    }

    if (d->camera && (!d->camera->hasFlags(Qn::virtual_camera)))
        result |= Qn::RecordingStatusIconButton;

    if (d->isPlayingLive())
        result |= Qn::RecordingStatusIconButton;

    const auto reader = d->display()->archiveReader();
    if (!reader || reader->isMediaPaused())
        result |= Qn::PauseButton;

    return result;
}

Qn::ResourceStatusOverlay QnMediaResourceWidget::calculateStatusOverlay() const
{
    m_encryptedArchiveData.clear();

    if (qnRuntime->isVideoWallMode())
    {
        if (nx::vms::common::saas::saasInitialized(systemContext()))
        {
            if (systemContext()->saasServiceManager()->saasShutDown())
                return Qn::SaasShutDown;
        }
        else if (!isVideoWallLicenseValid())
        {
            return Qn::VideowallWithoutLicenseOverlay;
        }
    }

    if (d->noExportPermission)
        return Qn::NoExportPermissionOverlay;

    if (!d->hasAccess())
        return Qn::AccessDeniedOverlay;

    if (d->camera)
    {
        if (d->isPlayingLive() && d->camera->needsToChangeDefaultPassword())
            return Qn::PasswordRequiredOverlay;

        const auto calculateRequiredPermissions =
            [this]() -> Qn::Permissions
            {
                if (d->isIoModule)
                    return Qn::ViewContentPermission;

                Qn::Permissions requiredPermissions = d->isPlayingLive()
                    ? Qn::ViewLivePermission
                    : Qn::ViewFootagePermission;

                const auto screenRecordingAction = action(ui::action::ToggleScreenRecordingAction);
                if (screenRecordingAction && screenRecordingAction->isChecked())
                    requiredPermissions |= Qn::ExportPermission;

                return requiredPermissions;
            };

        if (!d->accessController()->hasPermissions(d->camera, calculateRequiredPermissions()))
            return Qn::AccessDeniedOverlay;
    }

    // TODO: #sivanov This requires a lot of refactoring
    // for live video make a quick check: status has higher priority than EOF.
    if (d->isPlayingLive() && d->camera && d->camera->hasFlags(Qn::virtual_camera))
        return Qn::NoLiveStreamOverlay;

    if (d->camera
        && d->camera->hasCameraCapabilities(nx::vms::api::DeviceCapability::isOldFirmware))
    {
        return Qn::OldFirmwareOverlay;
    }

    if (d->isOffline())
        return Qn::OfflineOverlay;

    if (d->isUnauthorized())
        return Qn::UnauthorizedOverlay;

    if (d->isIoModule)
    {
        if (!NX_ASSERT(d->camera) || !d->camera->isScheduleEnabled())
            return Qn::IoModuleDisabledOverlay;

        const auto hasArchive = [](const QnSecurityCamResourcePtr& camera)
        {
            auto systemContext = SystemContext::fromResource(camera);
            if (!NX_ASSERT(systemContext))
                return false;

            const auto footageServers = systemContext->cameraHistoryPool()->
                getCameraFootageData(camera, true);
            return !footageServers.empty();
        };

        if (d->isPlayingLive())
        {
            return d->accessController()->
                hasAnyPermission(d->camera, Qn::ViewLivePermission | Qn::UserInputPermissions)
                ? Qn::EmptyOverlay
                : Qn::AccessDeniedOverlay;
        }
        else
        {
            if (!d->accessController()->hasPermissions(d->camera, Qn::ViewFootagePermission))
                return Qn::AccessDeniedOverlay;

            return hasArchive(d->camera)
                ? Qn::NoVideoDataOverlay
                : Qn::NoDataOverlay;
        }
    }

    if (d->camera)
    {
        if (d->isPlayingLive()
            && !accessController()->hasPermissions(d->camera, Qn::ViewLivePermission))
        {
            return Qn::NoLiveStreamOverlay;
        }

        if (d->camera->isDtsBased() && !d->camera->isScheduleEnabled())
        {
            const auto requiredPermission = d->isPlayingLive()
                ? Qn::ViewLivePermission
                : Qn::ViewFootagePermission;

            if (!ResourceAccessManager::hasPermissions(d->camera, requiredPermission))
                return Qn::AnalogWithoutLicenseOverlay;
        }
    }

    const auto mediaEvent = d->display()->camDisplay()->lastMediaEvent();
    if (mediaEvent.code == nx::media::StreamEvent::tooManyOpenedConnections)
        return Qn::TooManyOpenedConnectionsOverlay;

    if (d->display()->camDisplay()->isEOFReached())
    {
        // No need to check status: offline and unauthorized are checked first.
        return d->isPlayingLive()
            ? Qn::LoadingOverlay
            : Qn::NoDataOverlay;
    }

    if (d->resource->hasFlags(Qn::local_image))
    {
        if (d->resource->getStatus() == nx::vms::api::ResourceStatus::offline)
            return Qn::NoDataOverlay;
        return Qn::EmptyOverlay;
    }

    if (d->resource->hasFlags(Qn::local_video))
    {
        if (d->resource->getStatus() == nx::vms::api::ResourceStatus::offline)
            return Qn::NoDataOverlay;

        // Handle export from I/O modules.
        if (!d->hasVideo)
            return Qn::NoVideoDataOverlay;
    }

    if (d->display()->camDisplay()->isLongWaiting())
    {
        auto loader = systemContext()->cameraDataManager()->loader(d->mediaResource,
            /*createIfNotExists*/ false);
        if (loader && loader->periods(Qn::RecordingContent)
            .containTime(d->display()->camDisplay()->getExternalTime() / 1000))
        {
            return base_type::calculateStatusOverlay(nx::vms::api::ResourceStatus::online,
                d->hasVideo);
        }

        return Qn::NoDataOverlay;
    }

    if (mediaEvent.code == nx::media::StreamEvent::cannotDecryptMedia
        && !d->display()->camDisplay()->isBuffering())
    {
        m_encryptedArchiveData = mediaEvent.extraData;
        return Qn::CannotDecryptMediaOverlay;
    }

    if (d->display()->isPaused())
    {
        if (!d->hasVideo)
            return Qn::NoVideoDataOverlay;

        return Qn::EmptyOverlay;
    }

    return base_type::calculateStatusOverlay(nx::vms::api::ResourceStatus::online, d->hasVideo);
}

Qn::ResourceOverlayButton QnMediaResourceWidget::calculateOverlayButton(
    Qn::ResourceStatusOverlay statusOverlay) const
{
    if (!d->camera || !d->camera->resourcePool())
        return Qn::ResourceOverlayButton::Empty;

    const bool powerUserPermissions = accessController()->hasPowerUserPermissions();

    const bool canChangeSettings = qnRuntime->isDesktopMode()
        && ResourceAccessManager::hasPermissions(d->camera,
            Qn::SavePermission | Qn::WritePermission);

    switch (statusOverlay)
    {
        case Qn::IoModuleDisabledOverlay:
        case Qn::AnalogWithoutLicenseOverlay:
        {
            if (canChangeSettings)
            {
                switch (d->licenseStatus())
                {
                    case nx::vms::license::UsageStatus::notUsed:
                        return Qn::ResourceOverlayButton::EnableLicense;

                    case nx::vms::license::UsageStatus::overflow:
                        return Qn::ResourceOverlayButton::MoreLicenses;
                    default:
                        break;
                }
            }

            return Qn::ResourceOverlayButton::Empty;
        }

        case Qn::AccessDeniedOverlay:
        {
            return Qn::ResourceOverlayButton::Empty;
        }

        case Qn::OldFirmwareOverlay:
        case Qn::OfflineOverlay:
        {
            return menu()->canTrigger(action::CameraDiagnosticsAction, d->camera)
                ? Qn::ResourceOverlayButton::Diagnostics
                : Qn::ResourceOverlayButton::Empty;
        }

        case Qn::UnauthorizedOverlay:
        {
            return canChangeSettings
                ? Qn::ResourceOverlayButton::Settings
                : Qn::ResourceOverlayButton::Empty;
        }

        case Qn::PasswordRequiredOverlay:
        {
            if (qnRuntime->isDesktopMode() && powerUserPermissions)
                return Qn::ResourceOverlayButton::SetPassword;

            break;
        }

        case Qn::CannotDecryptMediaOverlay:
        {
            return powerUserPermissions
                ? Qn::ResourceOverlayButton::UnlockEncryptedArchive
                : Qn::ResourceOverlayButton::Empty;
        }

        default:
            break;
    }

    return base_type::calculateOverlayButton(statusOverlay);
}

void QnMediaResourceWidget::at_resource_propertyChanged(
    const QnResourcePtr& /*resource*/,
    const QString& key)
{
    if (key == QnMediaResource::customAspectRatioKey())
    {
        updateCustomAspectRatio();
    }
    else if (key == ResourcePropertyKey::kCameraCapabilities
        || key == ResourcePropertyKey::kTwoWayAudioEnabled
        || key == ResourcePropertyKey::kAudioOutputDeviceId
        || key == ResourcePropertyKey::kIoSettings)
    {
        updateCameraButtons();
    }
    else if (key == ResourcePropertyKey::kMediaStreams)
    {
        updateAspectRatio();
    }
    else if (key == ResourcePropertyKey::kCameraHotspotsEnabled)
    {
        updateButtonsVisibility();
    }
}

void QnMediaResourceWidget::updateAspectRatio()
{
    if (item() && item()->dewarpingParams().enabled && m_dewarpingParams.enabled)
    {
        const auto panoFactor = item()->dewarpingParams().panoFactor;
        if (panoFactor > 1)
        {
            setAspectRatio(panoFactor);
            return;
        }
        if (m_dewarpingParams.is360VR())
        {
            setAspectRatio(k360VRAspectRatio);
            return;
        }
    }

    qreal baseAspectRatio = calculateVideoAspectRatio();
    NX_ASSERT(!qFuzzyIsNull(baseAspectRatio));
    if (baseAspectRatio <= 0.0)
    {
        setAspectRatio(baseAspectRatio); /* No aspect ratio. */
        return;
    }

    qreal aspectRatio = baseAspectRatio *
        Geometry::aspectRatio(channelLayout()->size()) *
        (zoomRect().isNull() ? 1.0 : Geometry::aspectRatio(zoomRect()));

    setAspectRatio(aspectRatio);
}

bool QnMediaResourceWidget::isPlayingLive() const
{
    return d->isPlayingLive();
}

void QnMediaResourceWidget::at_camDisplay_liveChanged()
{
    const bool isPlayingLive = d->display()->camDisplay()->isRealTimeSource();

    if (isPlayingLive)
    {
        resumeHomePtzController();
    }
    else
    {
        titleBar()->rightButtonsBar()->setButtonsChecked(Qn::PtzButton, false);
        suspendHomePtzController();
    }
    updateCompositeOverlayMode();
    updateIconButton();

    if (qnRuntime->isVideoWallMode())
        updateHud(false);
}

void QnMediaResourceWidget::at_screenshotButton_clicked()
{
    menu()->trigger(action::TakeScreenshotAction, this);
}

void QnMediaResourceWidget::at_fishEyeButton_toggled(bool checked)
{
    auto params = item()->dewarpingParams();
    params.enabled = checked;
    item()->setDewarpingParams(params); //< TODO: #sivanov Move to instrument.

    setOption(DisplayDewarped, checked);
    if (checked)
    {
        setOption(DisplayMotion, false);
        resumeHomePtzController();
    }
    else
    {
        /* Stop all ptz activity. */
        ptzController()->continuousMove({});
        suspendHomePtzController();
    }

    updateButtonsVisibility();
}

void QnMediaResourceWidget::at_imageEnhancementButton_toggled(bool checked)
{
    auto params = item()->imageEnhancement();
    if (params.enabled == checked)
        return;

    params.enabled = checked;
    setImageEnhancement(params);
}

void QnMediaResourceWidget::at_ioModuleButton_toggled(bool /*checked*/)
{
    if (m_ioModuleOverlayWidget)
        updateIoModuleVisibility(animationAllowed());
}

void QnMediaResourceWidget::at_renderWatcher_widgetChanged(QnResourceWidget *widget)
{
    if (widget == this)
        updateRendererEnabled();
}

void QnMediaResourceWidget::at_zoomRectChanged()
{
    updateButtonsVisibility();
    updateAspectRatio();
    updateIconButton();
    updateZoomWindowDewarping();
    m_roiFiguresOverlayWidget->setZoomRect(zoomRect());
    m_analyticsOverlayWidget->setZoomRect(zoomRect());
}

void QnMediaResourceWidget::updateZoomWindowDewarping()
{
    if (!options().testFlag(DisplayDewarped) || !isZoomWindow() || !m_ptzController)
        return;

    m_ptzController->absoluteMove(
        ptz::CoordinateSpace::logical,
        QnFisheyePtzController::positionFromRect(m_dewarpingParams, zoomRect()),
        2.0);
}

void QnMediaResourceWidget::at_ptzController_changed(ptz::DataFields fields)
{
    if (fields.testFlag(ptz::DataField::capabilities))
        updateButtonsVisibility();

    if (fields & (ptz::DataField::activeObject | ptz::DataField::tours))
        updateTitleText();
}

void QnMediaResourceWidget::updateDewarpingParams()
{
    setDewarpingParams(d->mediaResource->getDewarpingParams());
}

void QnMediaResourceWidget::updateFisheye()
{
    if (!item())
        return;

    auto itemParams = item()->dewarpingParams();

    // Zoom windows have no own "dewarping" button, so counting it always pressed.
    const bool enabled = isZoomWindow() || itemParams.enabled;

    const bool fisheyeEnabled = enabled && m_dewarpingParams.enabled;

    const bool showFisheyeOverlay = fisheyeEnabled && !isZoomWindow();

    auto updatedOptions = options();
    updatedOptions.setFlag(ControlPtz, showFisheyeOverlay);
    updatedOptions.setFlag(DisplayDewarped, fisheyeEnabled);
    setOptions(updatedOptions);

    if (fisheyeEnabled && titleBar()->rightButtonsBar()->button(Qn::FishEyeButton))
        titleBar()->rightButtonsBar()->button(Qn::FishEyeButton)->setChecked(fisheyeEnabled);
    if (enabled)
        titleBar()->rightButtonsBar()->setButtonsChecked(Qn::MotionSearchButton | Qn::ZoomWindowButton, false);

    const bool flip = fisheyeEnabled && !m_dewarpingParams.is360VR()
        && m_dewarpingParams.viewMode == dewarping::FisheyeCameraMount::ceiling;

    const QList<int> allowedPanoFactorValues = m_dewarpingParams.allowedPanoFactorValues();
    if (!allowedPanoFactorValues.contains(itemParams.panoFactor))
    {
        itemParams.panoFactor = allowedPanoFactorValues.last();
        item()->setDewarpingParams(itemParams);
    }
    item()->setData(Qn::ItemFlipRole, flip);

    updateAspectRatio();
    if (display() && display()->camDisplay())
        display()->camDisplay()->setFisheyeEnabled(fisheyeEnabled);

    emit fisheyeChanged();

    // TODO: This check definitely doesn't belong here.
    const bool hasPtz = titleBar()->rightButtonsBar()->visibleButtons() & Qn::PtzButton;
    const bool ptzEnabled = titleBar()->rightButtonsBar()->checkedButtons() & Qn::PtzButton;
    if (hasPtz && ptzEnabled)
        setPtzMode(true);
}

void QnMediaResourceWidget::updateCustomAspectRatio()
{
    if (!d->display())
        return;

    d->display()->camDisplay()->setOverridenAspectRatio(d->mediaResource->customAspectRatio());
}

void QnMediaResourceWidget::updateIoModuleVisibility(bool animate)
{
    if (!d->isIoModule)
        return;

    const QnImageButtonWidget* const button = titleBar()->rightButtonsBar()->button(Qn::IoModuleButton);
    const bool ioBtnChecked = (button && button->isChecked());
    const bool onlyIoData = !d->hasVideo;
    const bool correctLicenseStatus = d->camera && d->camera->isScheduleEnabled();

    /// TODO: #ynikitenkov It needs to refactor error\status overlays totally!

    m_ioCouldBeShown = ((ioBtnChecked || onlyIoData) && correctLicenseStatus);
    const bool correctState = (!d->isOffline()
        && !d->isUnauthorized()
        && d->hasAccess()
        && d->isPlayingLive());
    const OverlayVisibility visibility = (m_ioCouldBeShown && correctState ? Visible : Invisible);
    setOverlayWidgetVisibility(m_ioModuleOverlayWidget, visibility, animate);
    updateOverlayWidgetsVisibility(animate);

    updateStatusOverlay(animate);
    updateOverlayButton();
}

void QnMediaResourceWidget::updateAnalyticsVisibility(bool animate)
{
    if (!m_analyticsOverlayWidget)
        return;

    setOverlayWidgetVisible(m_analyticsOverlayWidget,
        m_statusOverlay->opacity() == 0 && d->analyticsEnabled,
        animate);
}

void QnMediaResourceWidget::updateHotspotsState()
{
    setOption(DisplayHotspots, canDisplayHotspots()
        && item()->displayHotspots()
        && d->camera->cameraHotspotsEnabled());
}

void QnMediaResourceWidget::updateTimelineVisibility()
{
    if (options().testFlag(FullScreenMode))
    {
        action(action::ToggleTimelineAction)->setChecked(
            isAnalyticsObjectsVisibleForcefully() || isMotionSearchModeEnabled());
    }
}

void QnMediaResourceWidget::processDiagnosticsRequest()
{
    statisticsModule()->controls()->registerClick(
        "resource_status_overlay_diagnostics");

    if (d->camera)
        menu()->trigger(action::CameraDiagnosticsAction, d->camera);
}

void QnMediaResourceWidget::processEnableLicenseRequest()
{
    statisticsModule()->controls()->registerClick(
        "resource_status_overlay_enable_license");

    const auto licenseStatus = d->licenseStatus();
    if (licenseStatus != nx::vms::license::UsageStatus::notUsed)
        return;

    qnResourcesChangesManager->saveCamera(d->camera,
        [](const QnVirtualCameraResourcePtr &camera)
        {
            camera->setScheduleEnabled(true);
        });

    const bool animate = WindowContextAware::display()->animationAllowed();
    updateIoModuleVisibility(animate);
}

void QnMediaResourceWidget::processSettingsRequest()
{
    statisticsModule()->controls()->registerClick(
        "resource_status_overlay_settings");
    if (!d->camera)
        return;

    //TODO: #sivanov Check if may be provided without static cast.
    menu()->trigger(action::CameraSettingsAction, action::Parameters(d->camera)
        .withArgument(Qn::FocusTabRole, int(CameraSettingsTab::general)));
}

void QnMediaResourceWidget::processMoreLicensesRequest()
{
    statisticsModule()->controls()->registerClick(
        "resource_status_overlay_more_licenses");

    menu()->trigger(action::PreferencesLicensesTabAction);
}

void QnMediaResourceWidget::processEncryptedArchiveUnlockRequst()
{
    statisticsModule()->controls()->registerClick(
        "resource_status_overlay_unlock_encrypted_archive");
    if (!m_encryptedArchivePasswordDialog)
    {
        m_encryptedArchivePasswordDialog.reset(
            new EncryptedArchivePasswordDialog(resource(), mainWindow()));
    }
    m_encryptedArchivePasswordDialog->showForEncryptionData(m_encryptedArchiveData);
}

void QnMediaResourceWidget::at_item_imageEnhancementChanged()
{
    setImageEnhancement(item()->imageEnhancement());
}

void QnMediaResourceWidget::updateCompositeOverlayMode()
{
    if (!m_compositeOverlay || d->camera.isNull())
        return;

    const bool isLive = (d->display() && d->display()->camDisplay()
        ? d->display()->camDisplay()->isRealTimeSource() : false);

    const bool bookmarksEnabled = !isLive
        && navigator()->bookmarksModeEnabled();

    m_compositeOverlay->setCurrentWidget(bookmarksEnabled
        ? m_bookmarksContainer
        : m_textOverlayWidget);

    const bool visible = !options().testFlag(QnResourceWidget::InfoOverlaysForbidden)
        && (bookmarksEnabled || isLive);

    const bool animate = m_compositeOverlay->scene() != nullptr;
    setOverlayWidgetVisible(m_compositeOverlay, visible, animate);

    for (int i = 0; i < m_triggersContainer->count(); ++i)
    {
        if (auto button = qobject_cast<SoftwareTriggerButton*>(m_triggersContainer->item(i)))
            button->setLive(isLive);
    }
}

qint64 QnMediaResourceWidget::getUtcCurrentTimeUsec() const
{
    // get timestamp from the first channel that was painted
    int channel = std::distance(m_paintedChannels.cbegin(),
        std::find_if(m_paintedChannels.cbegin(), m_paintedChannels.cend(), [](bool value) { return value; }));
    if (channel >= channelCount())
        channel = 0;

    qint64 timestampUsec = hasVideo()
        ? m_renderer->getTimestampOfNextFrameToRender(channel).count()
        : display()->camDisplay()->getCurrentTime();

    return timestampUsec;
}

qint64 QnMediaResourceWidget::getUtcCurrentTimeMs() const
{
    qint64 datetimeUsec = getUtcCurrentTimeUsec();
    if (datetimeUsec == DATETIME_NOW)
        return qnSyncTime->currentMSecsSinceEpoch();

    if (datetimeUsec == AV_NOPTS_VALUE)
        return 0;

    return datetimeUsec / 1000;
}

QnScrollableTextItemsWidget* QnMediaResourceWidget::bookmarksContainer()
{
    return m_bookmarksContainer;
}

void QnMediaResourceWidget::setZoomWindowCreationModeEnabled(bool enabled)
{
    setOption(ControlZoomWindow, enabled);
    titleBar()->rightButtonsBar()->setButtonsChecked(Qn::ZoomWindowButton, enabled);
    if (enabled)
    {
        titleBar()->rightButtonsBar()->setButtonsChecked(
            Qn::PtzButton | Qn::FishEyeButton | Qn::MotionSearchButton, false);
    }

    setOption(WindowResizingForbidden, enabled);

    emit zoomWindowCreationModeEnabled(enabled);
}

void QnMediaResourceWidget::setMotionSearchModeEnabled(bool enabled)
{
    setOption(DisplayMotion, enabled);
}

bool QnMediaResourceWidget::isMotionSearchModeEnabled() const
{
    return (titleBar()->rightButtonsBar()->checkedButtons() & Qn::MotionSearchButton) != 0;
}

void QnMediaResourceWidget::setPtzMode(bool value)
{
    const bool ptzEnabled = value && d->supportsBasicPtz();

    titleBar()->rightButtonsBar()->setButtonsChecked(Qn::PtzButton, ptzEnabled);

    if (ptzEnabled)
        menu()->trigger(action::JumpToLiveAction, this);

    setOption(ControlPtz, ptzEnabled);
    item()->setControlPtz(ptzEnabled);
}

QnMediaResourceWidget::PtzEnabledBy QnMediaResourceWidget::ptzActivationReason()
{
    return m_ptzActivationReason;
}

void QnMediaResourceWidget::setPtzActivationReason(QnMediaResourceWidget::PtzEnabledBy reason)
{
    m_ptzActivationReason = reason;
}

QnSpeedRange QnMediaResourceWidget::speedRange() const
{
    static constexpr qreal kUnitSpeed = 1.0;
    static constexpr qreal kZeroSpeed = 0.0;

    if (!d->display() || !d->display()->archiveReader())
        return QnSpeedRange(kZeroSpeed, kZeroSpeed);

    if (!hasVideo())
        return QnSpeedRange(kUnitSpeed, kZeroSpeed);

    const qreal backward = d->display()->archiveReader()->isNegativeSpeedSupported()
        ? availableSpeedRange().backward
        : kZeroSpeed;

    return QnSpeedRange(availableSpeedRange().forward, backward);
}

const QnSpeedRange& QnMediaResourceWidget::availableSpeedRange()
{
    static const QnSpeedRange kAvailableSpeedRange(kMaxForwardSpeed, kMaxBackwardSpeed);
    return kAvailableSpeedRange;
}

bool QnMediaResourceWidget::isScheduleEnabled() const
{
    return d->licenseStatus() == nx::vms::license::UsageStatus::used;
}

bool QnMediaResourceWidget::isRoiVisible() const
{
    return options().testFlag(DisplayRoi);
}

void QnMediaResourceWidget::setRoiVisible(bool visible, bool animate)
{
    if (isRoiVisible() == visible)
        return;

    setOption(DisplayRoi, visible);
    item()->setDisplayRoi(visible);
    updateHud(animate);
}

bool QnMediaResourceWidget::isAnalyticsSupported() const
{
    return d->isAnalyticsSupported;
}

bool QnMediaResourceWidget::isAnalyticsObjectsVisible() const
{
    return options().testFlag(DisplayAnalyticsObjects);
}

void QnMediaResourceWidget::setAnalyticsObjectsVisible(bool visible, bool animate)
{
    if (isAnalyticsObjectsVisible() == visible)
        return;

    setOption(DisplayAnalyticsObjects, visible);
    item()->setDisplayAnalyticsObjects(visible);
    setAnalyticsModeEnabled(visible || d->analyticsObjectsVisibleForcefully, animate);
}

bool QnMediaResourceWidget::isAnalyticsObjectsVisibleForcefully() const
{
    return d->analyticsObjectsVisibleForcefully;
}

void QnMediaResourceWidget::setAnalyticsObjectsVisibleForcefully(bool visible, bool animate)
{
    d->analyticsObjectsVisibleForcefully = visible;
    setOption(DisplayAnalyticsObjects, visible);
    setAnalyticsModeEnabled(visible, animate);
    updateTimelineVisibility();
}

bool QnMediaResourceWidget::isAnalyticsModeEnabled() const
{
    return d->isAnalyticsEnabledInStream();
}

bool QnMediaResourceWidget::canDisplayHotspots() const
{
    const auto workbenchContext = windowContext()->workbenchContext();

    return ini().enableCameraHotspotsFeature
        && d->camera
        && !tourIsRunning(workbenchContext);
}

bool QnMediaResourceWidget::hotspotsVisible() const
{
    if (!(NX_ASSERT(canDisplayHotspots())))
        return false;

    return options().testFlag(DisplayHotspots);
}

void QnMediaResourceWidget::setHotspotsVisible(bool visible)
{
    if (!(NX_ASSERT(canDisplayHotspots())))
        return;

    if (hotspotsVisible() == visible)
        return;

    item()->setDisplayHotspots(visible);
}

void QnMediaResourceWidget::setAnalyticsModeEnabled(bool enabled, bool animate)
{
    if (d->analyticsEnabled == enabled)
        return;

    // Cleanup existing object frames in any case.
    if (!enabled)
        d->analyticsController->clearAreas();

    d->analyticsEnabled = enabled;

    // We should be able to disable analytics if it is not supported anymore.
    if (isAnalyticsSupported() || !enabled)
        d->setAnalyticsEnabledInStream(enabled);

    updateAnalyticsVisibility(animate);
}

void QnMediaResourceWidget::atItemDataChanged(Qn::ItemDataRole role)
{
    base_type::atItemDataChanged(role);

    switch (role)
    {
        case Qn::ItemDisplayAnalyticsObjectsRole:
            updateAnalyticsVisibility(/*animate*/ false);
            break;

        case Qn::ItemDisplayRoiRole:
            updateHud(/*animate*/ false);
            break;

        case Qn::ItemDisplayHotspotsRole:
            updateHotspotsState();
            break;

        default:
            break;
    }
}

QnClientCameraResourcePtr QnMediaResourceWidget::camera() const
{
    return d->camera;
}

nx::vms::client::core::AbstractAnalyticsMetadataProviderPtr
    QnMediaResourceWidget::analyticsMetadataProvider() const
{
    return d->analyticsMetadataProvider;
}

void QnMediaResourceWidget::setAnalyticsFilter(const nx::analytics::db::Filter& value)
{
    d->setAnalyticsFilter(value);
}

void QnMediaResourceWidget::updateWatermark()
{
    const auto context = systemContext();
    const auto accessController = context->accessController();
    const auto user = accessController->user();
    nx::core::Watermark watermark;

    // First create normal watermark according to current client state.
    if (context->globalSettings()->watermarkSettings().useWatermark
        && !accessController->hasGlobalPermissions(nx::vms::api::GlobalPermission::powerUser)
        && user
        && !user->getName().isEmpty())
    {
        watermark = {context->globalSettings()->watermarkSettings(), user->getName()};
    }

    // Do not show watermark for local AVI resources.
    if (resource().dynamicCast<QnAviResource>())
        watermark = {};

    // For exported layouts force using watermark if it exists.
    bool useLayoutWatermark = false;
    if (const auto exportedLayout = layoutResource().dynamicCast<QnFileLayoutResource>())
    {
        auto watermarkVariant = exportedLayout->data(Qn::LayoutWatermarkRole);
        if (watermarkVariant.isValid())
        {
            auto layoutWatermark = watermarkVariant.value<nx::core::Watermark>();
            if (layoutWatermark.visible())
            {
                watermark = layoutWatermark;
                useLayoutWatermark = true;
            }
        }
    }

    // Do not set watermark for admins but ONLY if it is not embedded in layout.
    if (accessController->hasPowerUserPermissions() && !useLayoutWatermark)
        return;

    m_watermarkPainter->setWatermark(watermark);
}

QAction* QnMediaResourceWidget::createActionAndButton(const QString& iconName,
    bool checked,
    const QKeySequence& shortcut,
    const QString& toolTip,
    HelpTopic::Id helpTopic,
    Qn::WidgetButtons buttonId,
    const QString& buttonName,
    ButtonHandler executor)
{
    auto action = new QAction(this);
    action->setIcon(iconName.endsWith("svg") ? loadSvgIcon(iconName) : qnSkin->icon(iconName));
    action->setCheckable(true);
    action->setChecked(checked);
    action->setShortcut(shortcut);
    // We will get scene-wide shortcut otherwise.
    action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    action->setToolTip(tooltipText(toolTip, shortcut));
    setHelpTopic(action, helpTopic);
    if (executor)
        connect(action, &QAction::toggled, this, executor);
    // We still want the shortcut to work inside the whole widget.
    addAction(action);

    auto button = createStatisticAwareButton(buttonName);
    button->setDefaultAction(action);
    titleBar()->rightButtonsBar()->addButton(buttonId, button);
    return action;
}

// ------------------------------------------------------------------------------------------------
// Soft Triggers

void QnMediaResourceWidget::createTrigger(const SoftwareTriggerInfo& info)
{
    const auto lowerBoundPredicate =
        [](const SoftwareTriggerInfo& left, const SoftwareTriggerInfo& right)
        {
            return left.name < right.name
                || (left.name == right.name
                    && left.icon < right.icon);
        };

    const int index = std::lower_bound(m_triggers.cbegin(), m_triggers.cend(), info,
        lowerBoundPredicate) - m_triggers.cbegin();

    const auto button = new SoftwareTriggerButton(this);
    configureTriggerButton(button, info);

    m_triggersContainer->insertItem(index, button, info.ruleId);
    m_triggers.insert(index, info);
}

void QnMediaResourceWidget::updateTriggerButtonTooltip(
    SoftwareTriggerButton* button,
    const SoftwareTriggerInfo& info)
{
    if (!button)
    {
        NX_ASSERT(false, "Trigger button is null");
        return;
    }

    if (info.enabled)
    {
        const auto name = nx::vms::event::StringsHelper::getSoftwareTriggerName(info.name);
        button->setToolTip(info.prolonged
            ? nx::format("%1 (%2)").args(name, tr("press and hold", "Soft Trigger")).toQString()
            : name);
        return;
    }
    else
    {
        button->setToolTip(tr("Disabled by schedule"));
    }
}

void QnMediaResourceWidget::configureTriggerButton(SoftwareTriggerButton* button,
    const SoftwareTriggerInfo& info)
{
    NX_ASSERT(button);

    button->setIcon(info.icon);
    button->setProlonged(info.prolonged);

    const bool buttonEnabled = !button->isLive() || info.enabled;
    if (button->isEnabled() != buttonEnabled)
    {
        if (!buttonEnabled)
        {
            const bool longPressed = info.prolonged &&
                button->state() == SoftwareTriggerButton::State::Waiting;
            if (longPressed)
                button->setState(SoftwareTriggerButton::State::Failure);
        }

        button->setEnabled(buttonEnabled);
    }

    updateTriggerButtonTooltip(button, info);

    button->disconnect(this);

    if (info.prolonged)
    {
        connect(button, &SoftwareTriggerButton::pressed, this,
            [this, button, handler = info.clientSideHandler, ruleId = info.ruleId]()
            {
                if (!button->isLive())
                    return;

                const bool success = m_triggersController->activateTrigger(ruleId);
                button->setState(success
                    ? SoftwareTriggerButton::State::Waiting
                    : SoftwareTriggerButton::State::Failure);

                if (success && handler)
                    handler();

                menu()->triggerIfPossible(action::SuspendCurrentShowreelAction);
            });

        connect(button, &SoftwareTriggerButton::released, this,
            [this, button]()
            {
                if (!button->isLive())
                    return;

                /* In case of activation error don't try to deactivate: */
                if (button->state() == SoftwareTriggerButton::State::Failure)
                    return;

                button->setState(m_triggersController->deactivateTrigger()
                    ? SoftwareTriggerButton::State::Default
                    : SoftwareTriggerButton::State::Failure);

                menu()->triggerIfPossible(action::ResumeCurrentShowreelAction);
            });
    }
    else
    {
        connect(button, &SoftwareTriggerButton::clicked, this,
            [this, button, handler = info.clientSideHandler, ruleId = info.ruleId]()
            {
                if (!button->isLive())
                    return;

                const bool success = m_triggersController->activateTrigger(ruleId);
                button->setEnabled(!success);
                button->setState(success
                    ? SoftwareTriggerButton::State::Waiting
                    : SoftwareTriggerButton::State::Failure);

                if (success && handler)
                    handler();
            });
    }

    // Go-to-live handler.
    connect(button, &SoftwareTriggerButton::clicked, this,
        [this, button, workbenchDisplay = WindowContextAware::display()]()
        {
            if (!button->isLive())
                menu()->trigger(action::JumpToLiveAction, this);
        });

    connect(button, &SoftwareTriggerButton::isLiveChanged, this,
        [this, ruleId = info.ruleId]()
        {
            m_triggerWatcher->updateTriggerAvailability(ruleId);
        });
}

int QnMediaResourceWidget::triggerIndex(const QnUuid& ruleId) const
{
    const auto it = std::find_if(m_triggers.cbegin(), m_triggers.cend(),
        [ruleId](const auto& val) { return val.ruleId == ruleId; });

    return (it != m_triggers.end()) ? (it - m_triggers.cbegin()) : -1;
}

void QnMediaResourceWidget::removeTrigger(int index)
{
    if (!NX_ASSERT(index >= 0 && index < m_triggers.size()))
        return;

    m_triggersContainer->deleteItem(m_triggers[index].ruleId);
    m_triggers.removeAt(index);
}

void QnMediaResourceWidget::at_triggerRemoved(QnUuid id)
{
    removeTrigger(triggerIndex(id));
}

void QnMediaResourceWidget::at_triggerAdded(
    QnUuid id,
    const QString& iconPath,
    const QString& name,
    bool prolonged,
    bool enabled)
{
    auto info = SoftwareTriggerInfo{
        .ruleId = id,
        .name = name,
        .icon = iconPath,
        .enabled = enabled,
        .prolonged = prolonged,
    };

    // TODO: #amalov Fill client side handler.
    createTrigger(info);
}

void QnMediaResourceWidget::at_triggerFieldsChanged(
    QnUuid id,
    SoftwareTriggersWatcher::TriggerFields fields)
{
    if (!fields)
        return;

    int index = triggerIndex(id);
    if (!NX_ASSERT(index >= 0 && index < m_triggers.size()))
        return;

    auto info = m_triggers[index];

    if (fields.testFlag(SoftwareTriggersWatcher::NameField))
        info.name = m_triggerWatcher->triggerName(id);
    if (fields.testFlag(SoftwareTriggersWatcher::IconField))
        info.icon = m_triggerWatcher->triggerIcon(id);
    if (fields.testFlag(SoftwareTriggersWatcher::ProlongedField))
        info.prolonged = m_triggerWatcher->prolongedTrigger(id);
    if (fields.testFlag(SoftwareTriggersWatcher::EnabledField))
        info.enabled = m_triggerWatcher->triggerEnabled(id);

    if (fields & SoftwareTriggersWatcher::TriggerFields{
        SoftwareTriggersWatcher::NameField,
        SoftwareTriggersWatcher::IconField})
    {
        // Recreate trigger to maintain alphabetic order.
        removeTrigger(index);
        createTrigger(info);
    }
    else
    {
        configureTriggerButton(
            static_cast<SoftwareTriggerButton*>(m_triggersContainer->item(index)),
            info);
    }
}

void QnMediaResourceWidget::handleSyncStateChanged(bool enabled)
{
    updateHud(enabled);
}

bool QnMediaResourceWidget::isTitleUnderMouse() const
{
    if (!m_hudOverlay || !m_hudOverlay->title())
        return false;

    return m_hudOverlay->title()->isUnderMouse();
}
