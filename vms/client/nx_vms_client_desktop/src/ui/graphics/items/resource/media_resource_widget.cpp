#include "media_resource_widget.h"

#include <chrono>

#include <QtCore/QTimer>
#include <QtCore/QVarLengthArray>
#include <QtGui/QPainter>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsLinearLayout>

#include <boost/algorithm/cxx11/all_of.hpp>

#include <api/app_server_connection.h>
#include <api/global_settings.h>
#include <api/server_rest_connection.h>

#include <nx/vms/event/rule.h>
#include <nx/vms/event/strings_helper.h>
#include <nx/vms/event/rule_manager.h>

#include <camera/resource_display.h>
#include <camera/cam_display.h>
#include <camera/camera_data_manager.h>
#include <camera/loaders/caching_camera_data_loader.h> // TODO: #GDM remove this dependency

#include <client/client_settings.h>
#include <client/client_globals.h>
#include <client/client_runtime_settings.h>
#include <client/client_module.h>
#include <nx/vms/client/desktop/analytics/camera_metadata_analytics_controller.h>
#include <nx/vms/client/desktop/ini.h>

#include <client_core/client_core_module.h>

#include <common/common_module.h>
#include <translation/datetime_formatter.h>

#include <core/resource/media_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/client_camera.h>
#include <core/resource/camera_history.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <plugins/resource/desktop_camera/desktop_resource_base.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/ptz/ptz_controller_pool.h>
#include <core/ptz/preset_ptz_controller.h>
#include <core/ptz/tour_ptz_controller.h>
#include <core/ptz/fallback_ptz_controller.h>
#include <core/ptz/activity_ptz_controller.h>
#include <core/ptz/viewport_ptz_controller.h>
#include <core/ptz/fisheye_home_ptz_controller.h>

#include <nx/streaming/media_data_packet.h>
#include <nx/streaming/abstract_archive_stream_reader.h>
#include <nx/utils/log/log.h>
#include <nx/utils/collection.h>

#include <nx/client/core/media/consuming_motion_metadata_provider.h>
#include <nx/client/core/motion/motion_grid.h>
#include <nx/client/core/utils/geometry.h>

#include <nx/vms/client/desktop/common/utils/painter_transform_scale_stripper.h>
#include <nx/vms/client/desktop/integrations/integrations.h>
#include <nx/vms/client/desktop/resource_properties/camera/camera_settings_tab.h>
#include <nx/vms/client/desktop/scene/resource_widget/private/media_resource_widget_p.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/common/recording_status_helper.h>
#include <nx/vms/client/desktop/ui/graphics/items/overlays/area_highlight_overlay_widget.h>
#include <nx/vms/client/desktop/ui/graphics/items/overlays/area_select_overlay_widget.h>
#include <nx/vms/client/desktop/ui/graphics/items/resource/widget_analytics_controller.h>
#include <nx/vms/client/desktop/utils/entropix_image_enhancer.h>
#include <nx/vms/client/desktop/watermark/watermark_painter.h>

#include <ui/fisheye/fisheye_ptz_controller.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/items/controls/html_text_item.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/image_button_bar.h>
#include <ui/graphics/items/resource/button_ids.h>
#include <ui/graphics/items/resource/resource_widget_renderer.h>
#include <ui/graphics/items/resource/software_trigger_button.h>
#include <ui/graphics/items/resource/two_way_audio_widget.h>
#include <ui/graphics/items/overlays/io_module_overlay_widget.h>
#include <ui/graphics/items/overlays/resource_status_overlay_widget.h>
#include <ui/graphics/items/overlays/hud_overlay_widget.h>
#include <ui/graphics/items/overlays/resource_title_item.h>
#include <ui/graphics/items/overlays/scrollable_text_items_widget.h>
#include <ui/graphics/items/overlays/status_overlay_controller.h>
#include <ui/graphics/items/standard/graphics_stacked_widget.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/style/nx_style.h>

#include <ui/workaround/gl_native_painting.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/watchers/workbench_render_watcher.h>

#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/synctime.h>
#include <utils/common/html.h>
#include <utils/license_usage_helper.h>
#include <utils/math/color_transformations.h>
#include <api/common_message_processor.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <utils/media/sse_helper.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource_management/resource_runtime_data.h>
#include <nx/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/desktop/system_health/default_password_cameras_watcher.h>
#include <nx/vms/client/desktop/ini.h>

#include <nx/network/cloud/protocol_type.h>

using namespace std::chrono;

using namespace nx;
using namespace vms::client::desktop;
using namespace ui;

using nx::vms::client::core::Geometry;
using nx::vms::client::core::MotionGrid;
using nx::vms::api::StreamDataFilter;
using nx::vms::api::StreamDataFilters;

namespace {

static constexpr int kMicroInMilliSeconds = 1000;

// TODO: #rvasilenko Change to other constant - 0 is 1/1/1970
// Note: -1 is used for invalid time
// Now it is returned when there is no archive data and archive is played backwards.
// Who returns it? --gdm?
static constexpr int kNoTimeValue = 0;

static constexpr qreal kMotionRegionAlpha = 0.4;

static constexpr qreal kMaxForwardSpeed = 16.0;
static constexpr qreal kMaxBackwardSpeed = 16.0;

static constexpr int kTriggersSpacing = 4;
static constexpr int kTriggerButtonSize = 40;
static constexpr int kTriggersMargin = 8; // overlaps HUD margin, i.e. does not sum up with it

static const char* kTriggerRequestIdProperty = "_qn_triggerRequestId";

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
    return context->action(action::ToggleLayoutTourModeAction)->isChecked();
}

} // namespace

QnMediaResourceWidget::QnMediaResourceWidget(QnWorkbenchContext* context, QnWorkbenchItem* item, QGraphicsItem* parent):
    base_type(context, item, parent),
    d(new MediaResourceWidgetPrivate(base_type::resource(), accessController())),
    m_recordingStatusHelper(new RecordingStatusHelper(this)),
    m_posUtcMs(DATETIME_INVALID),
    m_watermarkPainter(new WatermarkPainter),
    m_itemId(item->uuid())
{
    NX_ASSERT(d->resource, "Media resource widget was created with a non-media resource.");
    d->isExportedLayout = item
        && item->layout()
        && item->layout()->resource()
        && item->layout()->resource()->isFile();

    d->isPreviewSearchLayout = item
        && item->layout()
        && item->layout()->isSearchLayout();

    initRenderer();
    initDisplay();

    setupHud();

    connect(base_type::resource(), &QnResource::propertyChanged, this,
        &QnMediaResourceWidget::at_resource_propertyChanged);

    connect(base_type::resource(), &QnResource::mediaDewarpingParamsChanged, this,
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

    if (d->camera)
    {
        connect(d->camera, &QnVirtualCameraResource::motionRegionChanged, this,
            &QnMediaResourceWidget::invalidateMotionSensitivity);

        connect(d.get(), &MediaResourceWidgetPrivate::licenseStatusChanged,
            this,
            [this]
            {
                const bool animate = animationAllowed();
                updateIoModuleVisibility(animate);
                updateStatusOverlay(animate);
                updateOverlayButton();

                emit licenseStatusChanged();
            });

        auto ptzPool = qnClientCoreModule->ptzControllerPool();
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

    connect(commonModule()->messageProcessor(), &QnCommonMessageProcessor::businessActionReceived, this,
        [this](const vms::event::AbstractActionPtr &businessAction)
        {
            if (businessAction->actionType() != vms::api::ActionType::executePtzPresetAction)
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

    connect(context, &QnWorkbenchContext::userChanged,
        this, &QnMediaResourceWidget::resetTriggers);

    updateDisplay();
    updateDewarpingParams();
    updateAspectRatio();
    updatePtzController();

    /* Set up info updates. */
    connect(this, &QnMediaResourceWidget::updateInfoTextLater, this,
        &QnMediaResourceWidget::updateInfoText, Qt::QueuedConnection);

    /* Set up overlays */
    initIoModuleOverlay();
    updateTwoWayAudioWidget();
    initAreaHighlightOverlay();
    initAreaSelectOverlay();

    /* Set up buttons. */
    createButtons();
    initStatusOverlayController();

    connect(base_type::resource(), &QnResource::resourceChanged, this,
        &QnMediaResourceWidget::updateButtonsVisibility); // TODO: #GDM #Common get rid of resourceChanged

    connect(this, &QnResourceWidget::zoomRectChanged, this,
        &QnMediaResourceWidget::at_zoomRectChanged);
    connect(context->instance<QnWorkbenchRenderWatcher>(), &QnWorkbenchRenderWatcher::widgetChanged,
        this, &QnMediaResourceWidget::at_renderWatcher_widgetChanged);

    // Update buttons for single layout tour start/stop
    connect(action(action::ToggleLayoutTourModeAction), &QAction::toggled, this,
        &QnMediaResourceWidget::updateButtonsVisibility);

    m_recordingStatusHelper->setCamera(d->camera);

    connect(m_recordingStatusHelper, &RecordingStatusHelper::recordingModeChanged,
        this, &QnMediaResourceWidget::updateIconButton);

    d->analyticsController.reset(new WidgetAnalyticsController(this));
    d->analyticsController->setAreaHighlightOverlayWidget(m_areaHighlightOverlayWidget);
    d->analyticsController->setAnalyticsMetadataProvider(d->analyticsMetadataProvider);

    at_camDisplay_liveChanged();
    setPtzMode(false);
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
    connect(globalSettings(), &QnGlobalSettings::watermarkChanged,
        this, &QnMediaResourceWidget::updateWatermark);
    connect(context, &QnWorkbenchContext::userChanged,
        this, &QnMediaResourceWidget::updateWatermark);

    connect(this, &QnMediaResourceWidget::updateInfoTextLater, this,
        &QnMediaResourceWidget::updateCurrentUtcPosMs);

    connect(this, &QnMediaResourceWidget::positionChanged, this,
        &QnMediaResourceWidget::clearEntropixEnhancedImage);
    connect(this, &QnMediaResourceWidget::zoomRectChanged, this,
        &QnMediaResourceWidget::clearEntropixEnhancedImage);

    connect(qnResourceRuntimeDataManager, &QnResourceRuntimeDataManager::layoutItemDataChanged,
        this, &QnMediaResourceWidget::handleItemDataChanged);

    const bool canRotate = accessController()->hasPermissions(item->layout()->resource(),
        Qn::WritePermission);
    setOption(WindowRotationForbidden, !hasVideo() || !canRotate);
    updateButtonsVisibility();
}

QnMediaResourceWidget::~QnMediaResourceWidget()
{
    ensureAboutToBeDestroyedEmitted();

    if (d->display())
        d->display()->removeRenderer(m_renderer);

    m_renderer->notInUse();
    m_renderer->destroyAsync();

    for (auto* data : m_binaryMotionMask)
        qFreeAligned(data);
    m_binaryMotionMask.clear();
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
            if (const auto reader = display()->archiveReader())
            {
                const auto timestampUSec = data.toLongLong() * 1000;
                reader->jumpTo(timestampUSec, 0);
            }
            break;
        }
        case Qn::ItemSpeedRole:
            display()->archiveReader()->setSpeed(data.toDouble());
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
    // TODO: #Elric
    // Strictly speaking, this is a hack.
    // We shouldn't be using OpenGL context in class constructor.
    QGraphicsView *view = QnWorkbenchContextAware::display()->view();
    const auto viewport = qobject_cast<const QGLWidget*>(view ? view->viewport() : nullptr);
    m_renderer = new QnResourceWidgetRenderer(nullptr, viewport ? viewport->context() : nullptr);
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
        this, &QnMediaResourceWidget::updateTriggersAvailability);
    updateTriggersAvailabilityTimer->start();

    resetTriggers();

    auto eventRuleManager = commonModule()->eventRuleManager();

    connect(eventRuleManager, &vms::event::RuleManager::rulesReset,
        this, &QnMediaResourceWidget::resetTriggers);

    connect(eventRuleManager, &vms::event::RuleManager::ruleAddedOrUpdated,
        this, &QnMediaResourceWidget::at_eventRuleAddedOrUpdated);

    connect(eventRuleManager, &vms::event::RuleManager::ruleRemoved,
        this, &QnMediaResourceWidget::at_eventRuleRemoved);
}

void QnMediaResourceWidget::initIoModuleOverlay()
{
    if (d->isIoModule)
    {
        // TODO: #vkutin #gdm #common Make a style metric that holds this value.
        const auto topMargin = titleBar()
            ? titleBar()->leftButtonsBar()->uniformButtonSize().height()
            : 0.0;

        m_ioModuleOverlayWidget = new QnIoModuleOverlayWidget();
        m_ioModuleOverlayWidget->setIOModule(d->camera);
        m_ioModuleOverlayWidget->setAcceptedMouseButtons(Qt::NoButton);
        m_ioModuleOverlayWidget->setUserInputEnabled(
            accessController()->hasGlobalPermission(GlobalPermission::userInput));
        m_ioModuleOverlayWidget->setContentsMargins(0.0, topMargin, 0.0, 0.0);
        addOverlayWidget(m_ioModuleOverlayWidget, detail::OverlayParams(Visible, true, true));

        updateButtonsVisibility();
        updateIoModuleVisibility(false);

        connect(d->camera, &QnResource::statusChanged, this,
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

    m_areaSelectOverlayWidget = new AreaSelectOverlayWidget(m_compositeOverlay);
    addOverlayWidget(m_areaSelectOverlayWidget, detail::OverlayParams(Visible, true, true));

    connect(m_areaSelectOverlayWidget, &AreaSelectOverlayWidget::selectedAreaChanged,
        this, &QnMediaResourceWidget::handleSelectedAreaChanged);
}

void QnMediaResourceWidget::initAreaHighlightOverlay()
{
    if (!hasVideo())
        return;

    m_areaHighlightOverlayWidget = new AreaHighlightOverlayWidget(m_compositeOverlay);
    addOverlayWidget(m_areaHighlightOverlayWidget, detail::OverlayParams(Visible, true, true));

    connect(m_statusOverlay, &QnStatusOverlayWidget::opacityChanged,
        this, &QnMediaResourceWidget::updateAreaHighlightVisibility);
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
                 default:
                     break;
             }
         });

     connect(controller, &QnStatusOverlayController::customButtonClicked, this,
         [this, changeCameraPassword]()
         {
             const auto passwordWatcher = context()->instance<DefaultPasswordCamerasWatcher>();
             changeCameraPassword(passwordWatcher->camerasWithDefaultPassword().values(), true);
         });
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

    if (!accessController()->hasGlobalPermission(GlobalPermission::admin))
        return QString();

    const auto watcher = context()->instance<DefaultPasswordCamerasWatcher>();
    const auto camerasCount = watcher ? watcher->camerasWithDefaultPassword().size() : 0;
    return camerasCount > 1
        ? tr("Set for all %n Cameras", nullptr, camerasCount)
        : QString();
}

void QnMediaResourceWidget::updateTriggerAvailability(const vms::event::RulePtr& rule)
{
    const auto ruleId = rule->id();
    const auto triggerIt = lowerBoundbyTriggerName(rule);

    if (triggerIt == m_triggers.end())
        return;

    // Do not update data for the same rule, until we force it
    if (triggerIt->ruleId != ruleId)
        return;

    const auto button = qobject_cast<SoftwareTriggerButton*>(
        m_triggersContainer->item(triggerIt->overlayItemId));

    if (!button)
        return;

    const bool buttonEnabled = (rule && rule->isScheduleMatchTime(qnSyncTime->currentDateTime()))
        || !button->isLive();

    if (button->isEnabled() == buttonEnabled)
        return;

    const auto info = triggerIt->info;
    if (!buttonEnabled)
    {
        const bool longPressed = info.prolonged &&
            button->state() == SoftwareTriggerButton::State::Waiting;
        if (longPressed)
            button->setState(SoftwareTriggerButton::State::Failure);
    }

    button->setEnabled(buttonEnabled);
    updateTriggerButtonTooltip(button, info, buttonEnabled);
}

void QnMediaResourceWidget::updateTriggersAvailability()
{
    for (auto data: m_triggers)
        updateTriggerAvailability(commonModule()->eventRuleManager()->rule(data.ruleId));
}

void QnMediaResourceWidget::createButtons()
{
    auto screenshotButton = createStatisticAwareButton(lit("media_widget_screenshot"));
    screenshotButton->setIcon(qnSkin->icon("item/screenshot.png"));
    screenshotButton->setCheckable(false);
    screenshotButton->setToolTip(tr("Screenshot"));
    setHelpTopic(screenshotButton, Qn::MainWindow_MediaItem_Screenshot_Help);
    connect(screenshotButton, &QnImageButtonWidget::clicked, this,
        &QnMediaResourceWidget::at_screenshotButton_clicked);

    titleBar()->rightButtonsBar()->addButton(Qn::ScreenshotButton, screenshotButton);

    auto searchButton = createStatisticAwareButton(lit("media_widget_msearch"));
    searchButton->setIcon(qnSkin->icon("item/search.png"));
    searchButton->setCheckable(true);
    searchButton->setToolTip(tr("Smart Search"));
    setHelpTopic(searchButton, Qn::MainWindow_MediaItem_SmartSearch_Help);
    connect(searchButton, &QnImageButtonWidget::toggled, this,
        [this, searchButton](bool on)
        {
            if (searchButton->isClicked() && on)
                selectThisWidget(true);

            setMotionSearchModeEnabled(on);
        });

    titleBar()->rightButtonsBar()->addButton(Qn::MotionSearchButton, searchButton);

    if (d->camera && d->camera->isPtzRedirected())
    {
        createActionAndButton(
            /* icon*/ "item/area_zoom.png",
            /* checked*/ false,
            /* shortcut*/ {},
            /* tooltip */ tr("Area Zoom"),
            /* help id */ Qn::MainWindow_MediaItem_Ptz_Help,
            /* internal id */ Qn::PtzButton,
            /* statistics key */ "media_widget_area_zoom",
            /* handler */ &QnMediaResourceWidget::setPtzMode);
    }
    else
    {
        createActionAndButton(
            /* icon*/ "item/ptz.png",
            /* checked*/ false,
            /* shortcut*/ QKeySequence::fromString("P"),
            /* tooltip */ tr("PTZ"),
            /* help id */ Qn::MainWindow_MediaItem_Ptz_Help,
            /* internal id */ Qn::PtzButton,
            /* statistics key */ "media_widget_ptz",
            /* handler */ &QnMediaResourceWidget::setPtzMode);
    }

    createActionAndButton(
        "item/fisheye.png",
        item()->dewarpingParams().enabled,
        lit("D"),
        tr("Dewarping"),
        Qn::MainWindow_MediaItem_Dewarping_Help,
        Qn::FishEyeButton, lit("media_widget_fisheye"),
        &QnMediaResourceWidget::at_fishEyeButton_toggled);

    createActionAndButton(
        "item/zoom_window.png",
        false,
        lit("W"),
        tr("Create Zoom Window"),
        Qn::MainWindow_MediaItem_ZoomWindows_Help,
        Qn::ZoomWindowButton, lit("media_widget_zoom"),
        &QnMediaResourceWidget::setZoomWindowCreationModeEnabled);

    createActionAndButton(
        "item/image_enhancement.png",
        item()->imageEnhancement().enabled,
        lit("E"),
        tr("Image Enhancement"),
        Qn::MainWindow_MediaItem_ImageEnhancement_Help,
        Qn::EnhancementButton, lit("media_widget_enhancement"),
        &QnMediaResourceWidget::at_imageEnhancementButton_toggled);

    {
        QnImageButtonWidget *ioModuleButton = createStatisticAwareButton(lit("media_widget_io_module"));
        ioModuleButton->setIcon(qnSkin->icon("item/io.png"));
        ioModuleButton->setCheckable(true);
        ioModuleButton->setChecked(false);
        ioModuleButton->setToolTip(tr("I/O Module"));
        connect(ioModuleButton, &QnImageButtonWidget::toggled, this,
            &QnMediaResourceWidget::at_ioModuleButton_toggled);
        titleBar()->rightButtonsBar()->addButton(Qn::IoModuleButton, ioModuleButton);
    }

    if (qnRuntime->isDevMode())
    {
        QnImageButtonWidget *debugScreenshotButton = createStatisticAwareButton(lit("media_widget_debug_screenshot"));
        debugScreenshotButton->setIcon(qnSkin->icon("item/screenshot.png"));
        debugScreenshotButton->setCheckable(false);
        debugScreenshotButton->setToolTip(lit("Debug set of screenshots"));
        connect(debugScreenshotButton, &QnImageButtonWidget::clicked, this,
            [this]
            {
                menu()->trigger(action::TakeScreenshotAction, action::Parameters(this)
                    .withArgument<QString>(Qn::FileNameRole, lit("_DEBUG_SCREENSHOT_KEY_")));
            });
        titleBar()->rightButtonsBar()->addButton(Qn::DbgScreenshotButton, debugScreenshotButton);
    }

    auto entropixEnhancementButton =
        createStatisticAwareButton(lit("media_widget_entropix_enhancement"));
    entropixEnhancementButton->setIcon(qnSkin->icon("item/image_enhancement.png"));
    entropixEnhancementButton->setToolTip(lit("Entropix Image Enhancement"));
    connect(entropixEnhancementButton, &QnImageButtonWidget::clicked, this,
        &QnMediaResourceWidget::at_entropixEnhancementButton_clicked);
    titleBar()->rightButtonsBar()->addButton(
        Qn::EntropixEnhancementButton, entropixEnhancementButton);
}

void QnMediaResourceWidget::updatePtzController()
{
    if (!item())
        return;

    const auto threadPool = qnClientCoreModule->ptzControllerPool()->commandThreadPool();
    const auto executorThread = qnClientCoreModule->ptzControllerPool()->executorThread();

    /* Set up PTZ controller. */
    QnPtzControllerPtr fisheyeController;
    fisheyeController.reset(new QnFisheyePtzController(this), &QObject::deleteLater);
    fisheyeController.reset(new QnViewportPtzController(fisheyeController));
    fisheyeController.reset(new QnPresetPtzController(fisheyeController));
    fisheyeController.reset(new QnTourPtzController(
        fisheyeController,
        threadPool,
        executorThread));
    fisheyeController.reset(new QnActivityPtzController(
        commonModule(),
        QnActivityPtzController::Local,
        fisheyeController));

    // Small hack because widget's zoomRect is set only in Synchronize method, not instantly --gdm
    if (item() && item()->zoomRect().isNull())
    {
        // zoom items are not allowed to return home
        m_homePtzController = new QnFisheyeHomePtzController(fisheyeController);
        fisheyeController.reset(m_homePtzController);
    }

    if (d->camera)
    {
        auto ptzPool = qnClientCoreModule->ptzControllerPool();
        if (QnPtzControllerPtr serverController = ptzPool->controller(d->camera))
        {
            serverController.reset(new QnActivityPtzController(commonModule(),
                QnActivityPtzController::Client, serverController));
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

    connect(m_ptzController, &QnAbstractPtzController::changed, this,
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
        const auto cameraAr = camera->aspectRatio();
        if (cameraAr.isValid())
            return cameraAr.toFloat();
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
    return d->resource->getProperty(QnMediaResource::rotationKey()).toInt();
}

int QnMediaResourceWidget::defaultFullRotation() const
{
    const bool fisheyeEnabled = (isZoomWindow() || item() && item()->dewarpingParams().enabled)
        && dewarpingParams().enabled;
    const int fisheyeRotation = fisheyeEnabled
        && dewarpingParams().viewMode == QnMediaDewarpingParams::VerticalDown ? 180 : 0;

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
    if (m_homePtzController)
        m_homePtzController->suspend();
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
    QnGlNativePainting::begin(m_renderer->glContext(), painter);

    const qreal opacity = effectiveOpacity();
    bool opaque = qFuzzyCompare(opacity, 1.0);
    // always use blending for images --gdm
    if (!opaque || (base_type::resource()->flags() & Qn::still_image))
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    m_renderer->setBlurFactor(m_statusOverlay->opacity());
    const auto result = m_renderer->paint(channel, sourceSubRect, targetRect, opacity);
    m_paintedChannels[channel] = true;

    /* There is no need to restore blending state before invoking endNativePainting. */
    QnGlNativePainting::end(painter);

    return result;
}

void QnMediaResourceWidget::setupHud()
{
    static const int kScrollLineHeight = 8;

    m_triggersContainer = new QnScrollableItemsWidget(m_hudOverlay->right());
    m_triggersContainer->setAlignment(Qt::AlignRight | Qt::AlignBottom);
    m_triggersContainer->setLineHeight(kScrollLineHeight);

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
    QPointer<QnScrollableItemsWidget> triggersContainer(m_triggersContainer);
    const auto updateTriggersMinHeight =
        [content, triggersContainer]()
        {
            if (!content || !triggersContainer)
                return;

            // Calculate minimum height for downscale no more than kMaxDownscaleFactor times.
            static const qreal kMaxDownscaleFactor = 2.0;
            const qreal available = content->fixedSize().height() / content->sceneScale();
            const qreal desired = triggersContainer->effectiveSizeHint(Qt::PreferredSize).height();
            const qreal extra = content->size().height() - triggersContainer->size().height();
            const qreal min = qMin(desired, available * kMaxDownscaleFactor - extra);
            triggersContainer->setMinimumHeight(min);
        };

    connect(m_hudOverlay->content(), &QnViewportBoundWidget::scaleChanged,
        this, updateTriggersMinHeight);

    connect(m_triggersContainer, &QnScrollableItemsWidget::contentHeightChanged,
        this, updateTriggersMinHeight);
}

void QnMediaResourceWidget::updateHud(bool animate)
{
    base_type::updateHud(animate);
    setOverlayWidgetVisible(m_triggersContainer, isOverlayWidgetVisible(titleBar()), animate);
    updateCompositeOverlayMode();
}

void QnMediaResourceWidget::updateTwoWayAudioWidget()
{
    const auto user = context()->user();
    const bool twoWayAudioWidgetRequired = !d->isPreviewSearchLayout
        && d->camera
        && d->camera->hasTwoWayAudio()
        && accessController()->hasGlobalPermission(GlobalPermission::userInput)
        && NX_ASSERT(user);

    if (twoWayAudioWidgetRequired)
    {
        if (!d->twoWayAudioWidgetId.isNull())
            return; //< Already exists.

        const auto twoWayAudioWidget = new QnTwoWayAudioWidget(QnDesktopResource::calculateUniqueId(
            commonModule()->moduleGUID(), user->getId()));
        twoWayAudioWidget->setCamera(d->camera);
        twoWayAudioWidget->setFixedHeight(kTriggerButtonSize);
        context()->statisticsModule()->registerButton(lit("two_way_audio"), twoWayAudioWidget);

        // Items are ordered left-to-right and bottom-to-top,
        // so the two-way audio item is inserted at the bottom.
        d->twoWayAudioWidgetId = m_triggersContainer->insertItem(0, twoWayAudioWidget);
    }
    else
    {
        if (d->twoWayAudioWidgetId.isNull())
            return;

        m_triggersContainer->deleteItem(d->twoWayAudioWidgetId);
        d->twoWayAudioWidgetId = {};
    }
}

bool QnMediaResourceWidget::animationAllowed() const
{
    return QnWorkbenchContextAware::display()->animationAllowed();
}

void QnMediaResourceWidget::resumeHomePtzController()
{
    if (m_homePtzController && options().testFlag(DisplayDewarped)
        && display()->camDisplay()->isRealTimeSource())
            m_homePtzController->resume();
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

const QList<QRegion> &QnMediaResourceWidget::motionSelection() const
{
    return m_motionSelection;
}

bool QnMediaResourceWidget::isMotionSelectionEmpty() const
{
    using boost::algorithm::all_of;
    return all_of(m_motionSelection, [](const QRegion& r) { return r.isEmpty(); });
}

void QnMediaResourceWidget::addToMotionSelection(const QRect &gridRect)
{
    // Just send changed() if gridRect is empty.
    if (gridRect.isEmpty())
    {
        emit motionSelectionChanged();
        return;
    }

    ensureMotionSensitivity();

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
        selection -= m_motionSensitivity[i].getMotionMask();

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

void QnMediaResourceWidget::setMotionSelection(const QList<QRegion>& regions)
{
    if (regions.empty())
    {
        clearMotionSelection();
        return;
    }

    if (regions.size() != m_motionSelection.size())
    {
        NX_ASSERT(false, "invalid motion selection channel count");
        return;
    }

    if (m_motionSelection == regions)
        return;

    m_motionSelection = regions;
    invalidateMotionSelectionCache();
    emit motionSelectionChanged();
}

void QnMediaResourceWidget::invalidateMotionSensitivity()
{
    m_motionSensitivityValid = false;
}

void QnMediaResourceWidget::ensureMotionSensitivity() const
{
    if (m_motionSensitivityValid)
        return;

    if (d->camera)
    {
        m_motionSensitivity = d->camera->getMotionRegionList();

        if (m_motionSensitivity.size() != channelCount())
        {
            qnWarning("Camera '%1' returned a motion sensitivity list of invalid size.", d->camera->getName());
            qnResizeList(m_motionSensitivity, channelCount());
        }
    }
    else if (d->resource->hasFlags(Qn::motion))
    {
        for (int i = 0, count = channelCount(); i < count; i++)
            m_motionSensitivity.push_back(QnMotionRegion());
    }
    else
    {
        m_motionSensitivity.clear();
    }

    invalidateBinaryMotionMask();

    m_motionSensitivityValid = true;
}

void QnMediaResourceWidget::ensureBinaryMotionMask() const
{
    if (m_binaryMotionMaskValid)
        return;

    ensureMotionSensitivity();
    for (int i = 0; i < channelCount(); ++i)
	{
        QnMetaDataV1::createMask(m_motionSensitivity[i].getMotionMask(),
			reinterpret_cast<char*>(m_binaryMotionMask[i]));
	}
}

void QnMediaResourceWidget::invalidateBinaryMotionMask() const
{
    m_binaryMotionMaskValid = false;
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
        connect(d->resource, &QnResource::videoLayoutChanged, this,
            &QnMediaResourceWidget::at_videoLayoutChanged);

        if (const auto archiveReader = display->archiveReader())
        {
            connect(archiveReader, &QnAbstractArchiveStreamReader::streamPaused,
                this, &QnMediaResourceWidget::updateButtonsVisibility);
            connect(archiveReader, &QnAbstractArchiveStreamReader::streamResumed,
                this, &QnMediaResourceWidget::updateButtonsVisibility);
            connect(archiveReader, &QnAbstractArchiveStreamReader::streamResumed,
                this, &QnMediaResourceWidget::clearEntropixEnhancedImage);
        }

        setChannelLayout(display->videoLayout());
        display->addRenderer(m_renderer);
        m_renderer->setChannelCount(display->videoLayout()->channelCount());
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
    const auto buttonsBar = titleBar()->leftButtonsBar();

    if (isZoomWindow())
    {
        const auto iconButton = buttonsBar->button(Qn::RecordingStatusIconButton);
        iconButton->setIcon(qnSkin->icon("item/zoom_window_hovered.png"));
        iconButton->setToolTip(tr("Zoom Window"));
        return;
    }

    if (!d->camera || d->camera->hasFlags(Qn::wearable_camera))
        return;

    const auto icon = m_recordingStatusHelper->icon();
    const auto iconButton = buttonsBar->button(Qn::RecordingStatusIconButton);
    iconButton->setIcon(icon);
    iconButton->setToolTip(m_recordingStatusHelper->tooltip());
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
    titleBar()->rightButtonsBar()->button(Qn::EnhancementButton)->setChecked(imageEnhancement.enabled);
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
}

Qn::RenderStatus QnMediaResourceWidget::paintChannelBackground(
    QPainter* painter,
    int channel,
    const QRectF& channelRect,
    const QRectF& paintRect)
{
    QRectF sourceSubRect = Geometry::toSubRect(channelRect, paintRect);

    if (d->camera && d->camera->hasCombinedSensors())
    {
        const auto& sensor = d->camera->combinedSensorsDescription().mainSensor();
        if (sensor.isValid())
            sourceSubRect = Geometry::subRect(sensor.geometry, sourceSubRect);
    }

    Qn::RenderStatus result = Qn::NothingRendered;

    if (!m_entropixEnhancedImage.isNull())
    {
        const PainterTransformScaleStripper scaleStripper(painter);
        painter->drawImage(
            scaleStripper.mapRect(paintRect),
            m_entropixEnhancedImage,
            m_entropixEnhancedImage.rect());
        result = Qn::NewFrameRendered;
    }
    else if (!placeholderPixmap().isNull() && zoomTargetWidget() && !zoomRect().isValid())
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
            QColor color = toTransparent(qnGlobals->mrsColor(), 0.2);
            paintFilledRegionPath(painter, rect, m_motionSelectionPathCache[channel], color, color);
        }
    }

    if (isAnalyticsEnabled())
        d->analyticsController->updateAreas(timestamp, channel);
    else
        d->analyticsController->updateAreaForZoomWindow();

    if (ini().enableOldAnalyticsController && d->analyticsMetadataProvider)
    {
        // TODO: Rewrite old-style analytics visualization (via zoom windows) with metadata
        // providers.
        if (const auto metadata = d->analyticsMetadataProvider->metadata(timestamp, channel))
            qnMetadataAnalyticsController->gotMetadata(d->resource, metadata);
    }

    if (m_entropixProgress >= 0)
        paintProgress(painter, rect, m_entropixProgress);

    paintWatermark(painter, rect);

    if (ini().showVideoQualityOverlay
        && hasVideo()
        && !d->resource->hasFlags(Qn::local))
    {
        QColor overlayColor = m_renderer->isLowQualityImage(0)
            ? Qt::red
            : Qt::green;
        overlayColor = toTransparent(overlayColor, 0.5);
        const PainterTransformScaleStripper scaleStripper(painter);
        painter->fillRect(scaleStripper.mapRect(rect), overlayColor);
    }
}

void QnMediaResourceWidget::paintMotionGrid(QPainter *painter, int channel, const QRectF &rect, const QnMetaDataV1Ptr &motion)
{
    // 5-7 fps

    ensureMotionSensitivity();

    qreal xStep = rect.width() / MotionGrid::kWidth;
    qreal yStep = rect.height() / MotionGrid::kHeight;

    QVector<QPointF> gridLines[2];

    if (motion && motion->channelNumber == (quint32)channel)
    {
        // 2-3 fps

        ensureBinaryMotionMask();
        motion->removeMotion(m_binaryMotionMask[channel]);

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
    painter->setPen(QPen(QColor(255, 255, 255, 16), 0.0));
    painter->drawLines(gridLines[0]);

    painter->setPen(QPen(QColor(255, 0, 0, 128), 0.0));
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

QnMediaDewarpingParams QnMediaResourceWidget::dewarpingParams() const
{
    return m_dewarpingParams;
}

void QnMediaResourceWidget::setDewarpingParams(const QnMediaDewarpingParams &params)
{
    if (m_dewarpingParams == params)
        return;

    m_dewarpingParams = params;
    handleDewarpingParamsChanged();

    emit dewarpingParamsChanged();
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

    if (action(action::ToggleLayoutTourModeAction)->isChecked())
        return Qn::MainWindow_Scene_TourInProgress_Help;

    const Qn::ResourceStatusOverlay statusOverlay = statusOverlayController()->statusOverlay();

    if (statusOverlay == Qn::AnalogWithoutLicenseOverlay)
        return Qn::MainWindow_MediaItem_AnalogLicense_Help;

    if (statusOverlay == Qn::OfflineOverlay || statusOverlay == Qn::OldFirmwareOverlay)
        return Qn::MainWindow_MediaItem_Diagnostics_Help;

    if (statusOverlay == Qn::UnauthorizedOverlay)
        return Qn::MainWindow_MediaItem_Unauthorized_Help;

    if (statusOverlay == Qn::IoModuleDisabledOverlay)
        return Qn::IOModules_Help;

    if (options().testFlag(ControlPtz))
    {
        if (m_dewarpingParams.enabled)
            return Qn::MainWindow_MediaItem_Dewarping_Help;

        return Qn::MainWindow_MediaItem_Ptz_Help;
    }

    if (isZoomWindow())
        return Qn::MainWindow_MediaItem_ZoomWindows_Help;

    if (options().testFlag(DisplayMotion))
        return Qn::MainWindow_MediaItem_SmartSearch_Help;

    if (isIoModule())
        return Qn::IOModules_Help;

    if (d->resource->hasFlags(Qn::local))
        return Qn::MainWindow_MediaItem_Local_Help;

    if (d->camera && d->camera->isDtsBased())
        return Qn::MainWindow_MediaItem_AnalogCamera_Help;

    return Qn::MainWindow_MediaItem_Help;
}

void QnMediaResourceWidget::channelLayoutChangedNotify()
{
    base_type::channelLayoutChangedNotify();

    qnResizeList(m_motionSelection, channelCount());
    qnResizeList(m_motionSelectionPathCache, channelCount());
    qnResizeList(m_motionSensitivity, channelCount());
    qnResizeList(m_paintedChannels, channelCount());

    while (m_binaryMotionMask.size() > channelCount())
    {
        qFreeAligned(m_binaryMotionMask.back());
        m_binaryMotionMask.pop_back();
    }
    while (m_binaryMotionMask.size() < channelCount())
    {
        m_binaryMotionMask.push_back(static_cast<simd128i *>(qMallocAligned(MotionGrid::kWidth * MotionGrid::kHeight / 8, 32)));
        memset(m_binaryMotionMask.back(), 0, MotionGrid::kWidth * MotionGrid::kHeight / 8);
    }

    updateAspectRatio();
}

void QnMediaResourceWidget::channelScreenSizeChangedNotify()
{
    base_type::channelScreenSizeChangedNotify();

    m_renderer->setChannelScreenSize(channelScreenSize());
}

void QnMediaResourceWidget::optionsChangedNotify(Options changedFlags)
{
    if (changedFlags.testFlag(DisplayMotion))
    {
        const bool motionSearchEnabled = options().testFlag(DisplayMotion);

        if (auto reader = d->display()->archiveReader())
        {
            StreamDataFilters filter = reader->streamDataFilter();
            filter.setFlag(StreamDataFilter::motion, motionSearchEnabled);
            filter.setFlag(StreamDataFilter::media);
            reader->setStreamDataFilter(filter);
        }

        titleBar()->rightButtonsBar()->setButtonsChecked(
            Qn::MotionSearchButton, motionSearchEnabled);

        if (motionSearchEnabled)
        {
            titleBar()->rightButtonsBar()->setButtonsChecked(
                Qn::PtzButton | Qn::FishEyeButton | Qn::ZoomWindowButton, false);

            action(action::ToggleTimelineAction)->setChecked(true);
            setAreaSelectionType(AreaType::motion);
        }
        else
        {
            unsetAreaSelectionType(AreaType::motion);
        }

        setOption(WindowResizingForbidden, motionSearchEnabled);

        if (motionSearchEnabled)
            setProperty(Qn::MotionSelectionModifiers, 0);
        else
            setProperty(Qn::MotionSelectionModifiers, QVariant()); //< Use defaults.

        emit motionSearchModeEnabled(motionSearchEnabled);
    }

    if (changedFlags.testFlag(ControlPtz))
    {
        const bool switchedToPtzMode = options().testFlag(ControlPtz);
        if (switchedToPtzMode)
        {
            titleBar()->rightButtonsBar()->setButtonsChecked(
	        Qn::MotionSearchButton | Qn::ZoomWindowButton, false);
        }
    }

    base_type::optionsChangedNotify(changedFlags);
}

QString QnMediaResourceWidget::calculateDetailsText() const
{
    qreal fps = 0.0;
    qreal mbps = 0.0;

    for (int i = 0; i < channelCount(); i++)
    {
        const QnMediaStreamStatistics *statistics = d->display()->mediaProvider()->getStatistics(i);
        if (statistics->isConnectionLost()) // TODO: #GDM check does not work, case #3993
            continue;
        fps = qMax(fps, static_cast<qreal>(statistics->getFrameRate()));
        mbps += statistics->getBitrateMbps();
    }

    QString codecString;
    if (QnConstMediaContextPtr codecContext = d->display()->mediaProvider()->getCodecContext())
        codecString = codecContext->getCodecName();

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

    static const int kDetailsTextPixelSize = 11;

    QString result;
    const auto addDetailsString =
		[&result](const QString& value)
        {
            if (!value.isEmpty())
                result.append(htmlFormattedParagraph(value, kDetailsTextPixelSize, /*bold*/ true));
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

    return result;
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

    static const auto extractTime =
        [](qint64 dateTimeUsec)
        {
            if (isSpecialDateTimeValueUsec(dateTimeUsec))
                return QString();

            const auto dateTimeMs = dateTimeUsec / kMicroInMilliSeconds;
            const QString result = datetime::toString(dateTimeMs);

            return ini().showPreciseItemTimestamps
                ? lit("%1<br>%2 us").arg(result).arg(dateTimeUsec)
                : result;

            return result;
        };

    const QString timeString = (d->display()->camDisplay()->isRealTimeSource()
        ? tr("LIVE")
        : extractTime(getDisplayTimeUsec()));

    static const int kPositionTextPixelSize = 14;
    return htmlFormattedParagraph(timeString, kPositionTextPixelSize, true);
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

    if (qnRuntime->isDevMode())
        result |= Qn::DbgScreenshotButton;

    if (d->hasVideo && !base_type::resource()->hasFlags(Qn::still_image))
    {
        const auto requiredPermission = d->isPlayingLive()
            ? Qn::ViewLivePermission
            : Qn::ViewFootagePermission;

        if (accessController()->hasPermissions(d->resource, requiredPermission))
            result |= Qn::ScreenshotButton;
    }

    bool rgbImage = false;
    QString url = base_type::resource()->getUrl().toLower();

    // TODO: #rvasilenko totally evil. Button availability should be based on actual
    // colorspace value, better via some function in enhancement implementation,
    // and not on file extension checks!
    if (base_type::resource()->hasFlags(Qn::still_image)
        && !url.endsWith(lit(".jpg"))
        && !url.endsWith(lit(".jpeg"))
        )
        rgbImage = true;

    if (!rgbImage && d->hasVideo)
        result |= Qn::EnhancementButton;

    if (isZoomWindow())
    {
        if (d->display() && d->display()->isPaused()
            && d->camera && d->camera->hasCombinedSensors())
        {
            result |= Qn::EntropixEnhancementButton;
        }
        result |= Qn::RecordingStatusIconButton;

        return result;
    }

    if (d->hasVideo && base_type::resource()->hasFlags(Qn::motion))
    {
        result |= Qn::MotionSearchButton;
    }

    if (d->canControlPtz())
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
        if (item()
            && item()->layout()
            && accessController()->hasPermissions(item()->layout()->resource(),
                Qn::WritePermission | Qn::AddRemoveItemsPermission)
            && !tourIsRunning(context())
            )
            result |= Qn::ZoomWindowButton;
    }

    if (d->camera && (!d->camera->hasFlags(Qn::wearable_camera)))
        result |= Qn::RecordingStatusIconButton;

    return result;
}

Qn::ResourceStatusOverlay QnMediaResourceWidget::calculateStatusOverlay() const
{
    if (qnRuntime->isVideoWallMode() && !QnVideoWallLicenseUsageHelper(commonModule()).isValid())
        return Qn::VideowallWithoutLicenseOverlay;

    // TODO: #GDM #3.1 This really requires hell a lot of refactoring
    // for live video make a quick check: status has higher priority than EOF
    if (d->isPlayingLive() && d->camera && d->camera->hasFlags(Qn::wearable_camera))
        return Qn::NoLiveStreamOverlay;

    if (d->camera && d->camera->hasCameraCapabilities(Qn::IsOldFirmwareCapability))
        return Qn::OldFirmwareOverlay;

    if (d->isOffline())
        return Qn::OfflineOverlay;

    if (d->isUnauthorized())
        return Qn::UnauthorizedOverlay;

    if (d->camera)
    {
        if (d->isPlayingLive() && d->camera->needsToChangeDefaultPassword())
            return Qn::PasswordRequiredOverlay;

        const Qn::Permission requiredPermission = d->isPlayingLive()
            ? Qn::ViewLivePermission
            : Qn::ViewFootagePermission;

        if (!accessController()->hasPermissions(d->camera, requiredPermission))
            return Qn::AnalogWithoutLicenseOverlay;
    }

    if (d->isIoModule)
    {
        if (!d->isPlayingLive())
        {
            if (d->display()->camDisplay()->isLongWaiting())
                return Qn::NoDataOverlay;
            return Qn::NoVideoDataOverlay;
        }

        if (m_ioCouldBeShown) /// If widget could be shown then licenses Ok
            return Qn::EmptyOverlay;

        if (d->licenseStatus() != QnLicenseUsageStatus::used)
            return Qn::IoModuleDisabledOverlay;
    }

    if (d->display()->camDisplay()->lastMediaEvent() == Qn::MediaStreamEvent::TooManyOpenedConnections)
    {
        // Too many opened connections
        return Qn::TooManyOpenedConnectionsOverlay;
    }

    if (d->display()->camDisplay()->isEOFReached())
    {
        // No need to check status: offline and unauthorized are checked first.
        return d->isPlayingLive()
            ? Qn::LoadingOverlay
            : Qn::NoDataOverlay;
    }

    if (d->resource->hasFlags(Qn::local_image))
    {
        if (d->resource->getStatus() == Qn::Offline)
            return Qn::NoDataOverlay;
        return Qn::EmptyOverlay;
    }

    if (d->resource->hasFlags(Qn::local_video))
    {
        if (d->resource->getStatus() == Qn::Offline)
            return Qn::NoDataOverlay;

        // Handle export from I/O modules.
        if (!d->hasVideo)
            return Qn::NoVideoDataOverlay;
    }

    if (options().testFlag(DisplayActivity) && d->display()->isPaused())
    {
        if (!qnRuntime->isVideoWallMode())
            return Qn::PausedOverlay;

        return Qn::EmptyOverlay;
    }

    if (d->display()->camDisplay()->isLongWaiting())
    {
        auto loader = qnClientModule->cameraDataManager()->loader(d->mediaResource, false);
        if (loader && loader->periods(Qn::RecordingContent).containTime(d->display()->camDisplay()->getExternalTime() / 1000))
            return base_type::calculateStatusOverlay(Qn::Online, d->hasVideo);

        return Qn::NoDataOverlay;
    }

    if (d->display()->isPaused())
    {
        if (!d->hasVideo)
            return Qn::NoVideoDataOverlay;

        return Qn::EmptyOverlay;
    }

    return base_type::calculateStatusOverlay(Qn::Online, d->hasVideo);
}

Qn::ResourceOverlayButton QnMediaResourceWidget::calculateOverlayButton(
    Qn::ResourceStatusOverlay statusOverlay) const
{
    if (!d->camera || !d->camera->resourcePool())
        return Qn::ResourceOverlayButton::Empty;

    if (statusOverlay == Qn::PasswordRequiredOverlay
        && context()->accessController()->hasGlobalPermission(GlobalPermission::admin))
    {
        return Qn::ResourceOverlayButton::SetPassword;
    }

    const bool canChangeSettings = accessController()->hasPermissions(d->camera,
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
                    case QnLicenseUsageStatus::notUsed:
                        return Qn::ResourceOverlayButton::EnableLicense;

                    case QnLicenseUsageStatus::overflow:
                        return Qn::ResourceOverlayButton::MoreLicenses;
                    default:
                        break;
                }
            }

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
        updateCustomAspectRatio();
    else if (key == ResourcePropertyKey::kCameraCapabilities)
        updateTwoWayAudioWidget();
    else if (key == ResourcePropertyKey::kCombinedSensorsDescription)
        updateAspectRatio();
    else if (key == ResourcePropertyKey::kMediaStreams)
        updateAspectRatio();
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
    item()->setDewarpingParams(params); // TODO: #Elric #PTZ move to instrument

    setOption(DisplayDewarped, checked);
    if (checked)
    {
        setOption(DisplayMotion, false);
        resumeHomePtzController();
    }
    else
    {
        /* Stop all ptz activity. */
        ptzController()->continuousMove(nx::core::ptz::Vector());
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

    // TODO: #PTZ probably belongs to instrument.
    if (options() & DisplayDewarped)
    {
        m_ptzController->absoluteMove(
            Qn::LogicalPtzCoordinateSpace,
            QnFisheyePtzController::positionFromRect(m_dewarpingParams, zoomRect()),
            2.0);
    }
}

void QnMediaResourceWidget::at_ptzController_changed(Qn::PtzDataFields fields)
{
    if (fields & Qn::CapabilitiesPtzField)
        updateButtonsVisibility();

    if (fields & (Qn::ActiveObjectPtzField | Qn::ToursPtzField))
        updateTitleText();
}

void QnMediaResourceWidget::at_entropixEnhancementButton_clicked()
{
    using nx::vms::client::desktop::EntropixImageEnhancer;

    m_entropixEnhancer.reset(new EntropixImageEnhancer(d->camera));
    connect(m_entropixEnhancer, &EntropixImageEnhancer::cameraScreenshotReady,
        this, &QnMediaResourceWidget::at_entropixImageLoaded);
    connect(m_entropixEnhancer, &EntropixImageEnhancer::progressChanged, this,
        [this](int progress)
        {
            m_entropixProgress = progress < 100 ? progress : -1;
        });

    m_entropixEnhancer->requestScreenshot(d->display()->currentTimeUSec() / 1000, zoomRect());
}

void QnMediaResourceWidget::at_entropixImageLoaded(const QImage& image)
{
    m_entropixEnhancedImage = image;
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
    bool enabled = isZoomWindow() || itemParams.enabled;

    bool fisheyeEnabled = enabled && m_dewarpingParams.enabled;

    const bool showFisheyeOverlay = fisheyeEnabled && !isZoomWindow();
    setOption(ControlPtz, showFisheyeOverlay);
    setOption(DisplayDewarped, fisheyeEnabled);

    if (fisheyeEnabled && titleBar()->rightButtonsBar()->button(Qn::FishEyeButton))
        titleBar()->rightButtonsBar()->button(Qn::FishEyeButton)->setChecked(fisheyeEnabled);
    if (enabled)
        titleBar()->rightButtonsBar()->setButtonsChecked(Qn::MotionSearchButton | Qn::ZoomWindowButton, false);

    bool flip = fisheyeEnabled
        && m_dewarpingParams.viewMode == QnMediaDewarpingParams::VerticalDown;

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

    // TODO: #Elric doesn't belong here, hack.
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
    const bool correctLicenseStatus = isLicenseUsed();

    /// TODO: #ynikitenkov It needs to refactor error\status overlays totally!

    m_ioCouldBeShown = ((ioBtnChecked || onlyIoData) && correctLicenseStatus);
    const bool correctState = (!d->isOffline()
        && !d->isUnauthorized()
        && d->isPlayingLive());
    const OverlayVisibility visibility = (m_ioCouldBeShown && correctState ? Visible : Invisible);
    setOverlayWidgetVisibility(m_ioModuleOverlayWidget, visibility, animate);
    updateOverlayWidgetsVisibility(animate);

    updateStatusOverlay(animate);
    updateOverlayButton();
}

void QnMediaResourceWidget::updateAreaHighlightVisibility()
{
    if (!m_areaHighlightOverlayWidget)
        return;

    const auto visibility = (m_statusOverlay->opacity() > 0) ? Invisible : Visible;
    setOverlayWidgetVisibility(m_areaHighlightOverlayWidget, visibility, false);
}

void QnMediaResourceWidget::processDiagnosticsRequest()
{
    context()->statisticsModule()->registerClick(lit("resource_status_overlay_diagnostics"));

    if (d->camera)
        menu()->trigger(action::CameraDiagnosticsAction, d->camera);
}

void QnMediaResourceWidget::processEnableLicenseRequest()
{
    context()->statisticsModule()->registerClick(lit("resource_status_overlay_enable_license"));

    const auto licenseStatus = d->licenseStatus();
    if (licenseStatus != QnLicenseUsageStatus::notUsed)
        return;

    qnResourcesChangesManager->saveCamera(d->camera,
        [](const QnVirtualCameraResourcePtr &camera)
        {
            camera->setLicenseUsed(true);
        });

    const bool animate = QnWorkbenchContextAware::display()->animationAllowed();
    updateIoModuleVisibility(animate);
}

void QnMediaResourceWidget::processSettingsRequest()
{
    context()->statisticsModule()->registerClick(lit("resource_status_overlay_settings"));
    if (!d->camera)
        return;

    //TODO: #GDM Check if may be provided without static cast.
    menu()->trigger(action::CameraSettingsAction, action::Parameters(d->camera)
        .withArgument(Qn::FocusTabRole, int(CameraSettingsTab::general)));
}

void QnMediaResourceWidget::processMoreLicensesRequest()
{
    context()->statisticsModule()->registerClick(lit("resource_status_overlay_more_licenses"));

    menu()->trigger(action::PreferencesLicensesTabAction);
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

qint64 QnMediaResourceWidget::getDisplayTimeUsec() const
{
    qint64 result = getUtcCurrentTimeUsec();
    if (!isSpecialDateTimeValueUsec(result))
    {
        const auto timeWatcher = context()->instance<nx::vms::client::core::ServerTimeWatcher>();
        result += timeWatcher->displayOffset(d->mediaResource) * 1000ll;
    }
    return result;
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
    const bool ptzEnabled = value && d->canControlPtz();
    if (ptzEnabled)
    {
        titleBar()->rightButtonsBar()->setButtonsChecked(Qn::PtzButton, true);

        // TODO: #gdm evil hack! Won't work if SYNC is off and this item is not selected.
        action(action::JumpToLiveAction)->trigger();
    }

    setOption(ControlPtz, ptzEnabled);
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

bool QnMediaResourceWidget::isLicenseUsed() const
{
    return d->licenseStatus() == QnLicenseUsageStatus::used;
}

bool QnMediaResourceWidget::isAnalyticsSupported() const
{
    return d->isAnalyticsSupported;
}

bool QnMediaResourceWidget::isAnalyticsEnabled() const
{
    if (!isAnalyticsSupported())
        return false;

    if (const auto reader = display()->archiveReader())
        return reader->streamDataFilter().testFlag(StreamDataFilter::objectDetection);

    return false;
}

void QnMediaResourceWidget::setAnalyticsEnabled(bool analyticsEnabled)
{
    if (!isAnalyticsSupported())
        return;

    if (auto reader = display()->archiveReader())
    {
        StreamDataFilters filter = reader->streamDataFilter();
        filter.setFlag(StreamDataFilter::objectDetection, analyticsEnabled);
        filter.setFlag(StreamDataFilter::media);
        reader->setStreamDataFilter(filter);
    }

    if (!analyticsEnabled)
    {
        d->analyticsController->clearAreas();

        // Earlier we didn't unset forced video buffer length to avoid micro-freezes required to
        // fill in the video buffer on succeeding analytics mode activations. But it looks like this
        // mode causes a lot of glitches when enabled, so we'd prefer to leave on a safe side.
        display()->camDisplay()->setForcedVideoBufferLength(milliseconds::zero());
    }
    else
    {
        display()->camDisplay()->setForcedVideoBufferLength(
            milliseconds(ini().analyticsVideoBufferLengthMs));
    }
}

nx::vms::client::core::AbstractAnalyticsMetadataProviderPtr
    QnMediaResourceWidget::analyticsMetadataProvider() const
{
    return d->analyticsMetadataProvider;
}

/*
* Soft Triggers
*/

QnMediaResourceWidget::SoftwareTriggerInfo QnMediaResourceWidget::makeTriggerInfo(
    const nx::vms::event::RulePtr& rule) const
{
    return SoftwareTriggerInfo({
        rule->eventParams().inputPortId,
        rule->eventParams().caption,
        rule->eventParams().description,
        rule->isActionProlonged() });
}

void QnMediaResourceWidget::createTriggerIfRelevant(
    const vms::event::RulePtr& rule)
{
    const auto ruleId = rule->id();
    const auto it = lowerBoundbyTriggerName(rule);

    NX_ASSERT(it == m_triggers.end() || it->ruleId != ruleId);

    if (!isRelevantTriggerRule(rule))
        return;

    const SoftwareTriggerInfo info = makeTriggerInfo(rule);

    std::function<void()> clientSideHandler;

    if (rule->actionType() == nx::vms::api::ActionType::bookmarkAction)
    {
        clientSideHandler =
            [this] { action(action::BookmarksModeAction)->setChecked(true); };
    }

    const auto button = new SoftwareTriggerButton(this);
    configureTriggerButton(button, info, clientSideHandler);

    connect(button, &SoftwareTriggerButton::isLiveChanged, this,
        [this, rule]()
        {
            updateTriggerAvailability(rule);
        });

    const int index = std::distance(m_triggers.begin(), it);
    const auto overlayItemId = m_triggersContainer->insertItem(index, button);
    m_triggers.insert(it, SoftwareTrigger{ruleId, info, overlayItemId});
}

QnMediaResourceWidget::TriggerDataList::iterator
    QnMediaResourceWidget::lowerBoundbyTriggerName(const nx::vms::event::RulePtr& rule)
{
    static const auto compareFunction =
        [](const SoftwareTrigger& left, const SoftwareTrigger& right)
        {
            return left.info.name < right.info.name
                || (left.info.name == right.info.name
                    && left.info.icon < right.info.icon);
        };

    const auto idValue = SoftwareTrigger{rule->id(), makeTriggerInfo(rule), QnUuid()};
    return std::lower_bound(m_triggers.begin(), m_triggers.end(), idValue, compareFunction);
}

bool QnMediaResourceWidget::isRelevantTriggerRule(const vms::event::RulePtr& rule) const
{
    if (rule->isDisabled() || rule->eventType() != vms::api::EventType::softwareTriggerEvent)
        return false;

    const auto resourceId = d->resource->getId();
    if (!rule->eventResources().empty() && !rule->eventResources().contains(resourceId))
        return false;

    if (rule->eventParams().metadata.allUsers)
        return true;

    const auto currentUser = accessController()->user();
    if (!currentUser)
        return false; //< All triggers will be added when user is set up.

    const auto subjects = rule->eventParams().metadata.instigators;
    if (::contains(subjects, currentUser->getId()))
        return true;

    return ::contains(subjects, QnUserRolesManager::unifiedUserRoleId(currentUser));
}

void QnMediaResourceWidget::updateTriggerButtonTooltip(
    SoftwareTriggerButton* button,
    const SoftwareTriggerInfo& info,
    bool enabledBySchedule)
{
    if (!button)
    {
        NX_ASSERT(false, "Trigger button is null");
        return;
    }

    if (enabledBySchedule)
    {
        const auto name = vms::event::StringsHelper::getSoftwareTriggerName(info.name);
        button->setToolTip(info.prolonged
            ? lit("%1 (%2)").arg(name).arg(tr("press and hold", "Soft Trigger"))
            : name);
        return;
    }
    else
    {
        button->setToolTip(tr("Disabled by schedule"));
    }

}

void QnMediaResourceWidget::updateWatermark()
{
    // Ini guard; remove on release. Default watermark is invisible.
    auto settings = globalSettings()->watermarkSettings();
    if (!ini().enableWatermark)
        return;

    // First create normal watermark according to current client state.
    auto watermark = context()->watermark();

    // Do not show watermark for local AVI resources.
    if (resource().dynamicCast<QnAviResource>())
        watermark = {};

    // Force using layout watermark if it exists and is visible.
    bool useLayoutWatermark = false;
    if (item() && item()->layout())
    {
        auto watermarkVariant = item()->layout()->data(Qn::LayoutWatermarkRole);
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
    if (accessController()->hasGlobalPermission(nx::vms::api::GlobalPermission::admin)
        && !useLayoutWatermark)
    {
        return;
    }

    m_watermarkPainter->setWatermark(watermark);
}

void QnMediaResourceWidget::createActionAndButton(const char* iconName,
    bool checked,
    const QKeySequence& shortcut,
    const QString& toolTip,
    Qn::HelpTopic helpTopic,
    Qn::WidgetButtons buttonId, const QString& buttonName,
    ButtonHandler executor)
{
    auto action = new QAction(this);
    action->setIcon(qnSkin->icon(iconName));
    action->setCheckable(true);
    action->setChecked(checked);
    action->setShortcut(shortcut);
    // We will get scene-wide shortcut otherwise.
    action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    action->setToolTip(toolTip);
    setHelpTopic(action, helpTopic);
    connect(action, &QAction::toggled, this, executor);
    // We still want the shortcut to work inside the whole widget.
    addAction(action);

    auto button = createStatisticAwareButton(buttonName);
    button->setDefaultAction(action);
    titleBar()->rightButtonsBar()->addButton(buttonId, button);
}

void QnMediaResourceWidget::configureTriggerButton(SoftwareTriggerButton* button,
    const SoftwareTriggerInfo& info, std::function<void()> clientSideHandler)
{
    NX_ASSERT(button);

    button->setIcon(info.icon);
    button->setProlonged(info.prolonged);
    updateTriggerButtonTooltip(button, info, true);

    const auto resultHandler =
        [button = QPointer<SoftwareTriggerButton>(button)](bool success, qint64 requestId)
        {
            if (!button || button->property(
                kTriggerRequestIdProperty).value<rest::Handle>() != requestId)
            {
                return;
            }

            button->setEnabled(true);

            button->setState(success
                ? SoftwareTriggerButton::State::Success
                : SoftwareTriggerButton::State::Failure);
        };

    if (info.prolonged)
    {
        connect(button, &SoftwareTriggerButton::pressed, this,
            [this, button, resultHandler, clientSideHandler, id = info.triggerId]()
            {
                if (!button->isLive())
                    return;

                const auto requestId = invokeTrigger(id, resultHandler, vms::api::EventState::active);
                const bool success = requestId != rest::Handle();
                button->setProperty(kTriggerRequestIdProperty, requestId);
                button->setState(success
                    ? SoftwareTriggerButton::State::Waiting
                    : SoftwareTriggerButton::State::Failure);

                if (success && clientSideHandler)
                    clientSideHandler();
            });

        connect(button, &SoftwareTriggerButton::released, this,
            [this, button, resultHandler, id = info.triggerId]()
            {
                if (!button->isLive())
                    return;

                /* In case of activation error don't try to deactivate: */
                if (button->state() == SoftwareTriggerButton::State::Failure)
                    return;

                const auto requestId = invokeTrigger(id, resultHandler, vms::api::EventState::inactive);
                const bool success = requestId != rest::Handle();
                button->setProperty(kTriggerRequestIdProperty, requestId);
                button->setState(success
                    ? SoftwareTriggerButton::State::Default
                    : SoftwareTriggerButton::State::Failure);
            });
    }
    else
    {
        connect(button, &SoftwareTriggerButton::clicked, this,
            [this, button, resultHandler, clientSideHandler, id = info.triggerId]()
            {
                if (!button->isLive())
                    return;

                const auto requestId = invokeTrigger(id, resultHandler);
                const bool success = requestId != rest::Handle();
                button->setProperty(kTriggerRequestIdProperty, requestId);
                button->setEnabled(!success);
                button->setState(success
                    ? SoftwareTriggerButton::State::Waiting
                    : SoftwareTriggerButton::State::Failure);

                if (success && clientSideHandler)
                    clientSideHandler();
            });
    }

    // Go-to-live handler.
    connect(button, &SoftwareTriggerButton::clicked, this,
        [this, button, workbenchDisplay = QnWorkbenchContextAware::display()]()
        {
            if (button->isLive())
                return;

            const auto syncAction = action(action::ToggleSyncAction);
            const bool sync = syncAction->isEnabled() && syncAction->isChecked();

            if (sync || workbenchDisplay->widget(Qn::CentralRole) == this)
            {
                action(action::JumpToLiveAction)->trigger();
            }
            else if (auto reader = display()->archiveReader())
            {
                // TODO: Refactor workbench navigator to avoid switching to live in different places.
                reader->jumpTo(DATETIME_NOW, 0);
                reader->setSpeed(1.0);
                reader->resumeMedia();
            }
        });
}

void QnMediaResourceWidget::resetTriggers()
{
    /* Delete all buttons: */
    for (const auto& data: m_triggers)
        m_triggersContainer->deleteItem(data.overlayItemId);

    /* Clear triggers information: */
    m_triggers.clear();

    if (!accessController()->hasGlobalPermission(GlobalPermission::userInput))
        return;

    /* Create new relevant triggers: */
    for (const auto& rule: commonModule()->eventRuleManager()->rules())
        createTriggerIfRelevant(rule); //< creates a trigger only if the rule is relevant

    updateTriggersAvailability();
}

void QnMediaResourceWidget::at_eventRuleRemoved(const QnUuid& id)
{
    const auto it = std::find_if(m_triggers.begin(), m_triggers.end(),
        [id](const auto& val) { return val.ruleId == id; });
    if (it == m_triggers.end() || it->ruleId != id)
        return;

    m_triggersContainer->deleteItem(it->overlayItemId);
    m_triggers.erase(it);
}

void QnMediaResourceWidget::clearEntropixEnhancedImage()
{
    if (m_entropixEnhancer)
        m_entropixEnhancer->cancelRequest();
    if (!m_entropixEnhancedImage.isNull())
        m_entropixEnhancedImage = QImage();
};

void QnMediaResourceWidget::at_eventRuleAddedOrUpdated(const vms::event::RulePtr& rule)
{
    const auto ruleId = rule->id();
    const auto it = lowerBoundbyTriggerName(rule);
    if (it == m_triggers.end() || it->ruleId != ruleId)
    {
        /* Create trigger if the rule is relevant: */
        createTriggerIfRelevant(rule);
    }
    else
    {
        /* Delete trigger: */
        at_eventRuleRemoved(ruleId);

        /* Recreate trigger if the rule is still relevant: */
        createTriggerIfRelevant(rule);
    }

    // Forcing update of trigger button
    updateTriggerAvailability(rule);
};

rest::Handle QnMediaResourceWidget::invokeTrigger(
    const QString& id,
    std::function<void(bool, rest::Handle)> resultHandler,
    vms::api::EventState toggleState)
{
    if (!accessController()->hasGlobalPermission(GlobalPermission::userInput))
        return rest::Handle();

    const auto responseHandler =
        [this, resultHandler, id](bool success, rest::Handle handle, const QnJsonRestResult& result)
        {
            Q_UNUSED(handle);
            success = success && result.error == QnRestResult::NoError;

            if (!success)
            {
                NX_ERROR(this, tr("Failed to invoke trigger %1 (%2)")
                    .arg(id).arg(result.errorString));
            }

            if (resultHandler)
                resultHandler(success, handle);
        };

    return commonModule()->currentServer()->restConnection()->softwareTriggerCommand(
        d->resource->getId(), id, toggleState,
        responseHandler, QThread::currentThread());
}
