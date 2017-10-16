#include "media_resource_widget.h"

#include <QtCore/QTimer>
#include <QtCore/QVarLengthArray>
#include <QtGui/QPainter>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsLinearLayout>

#include <boost/algorithm/cxx11/all_of.hpp>

#include <api/app_server_connection.h>
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

#include <client_core/client_core_module.h>

#include <common/common_module.h>

#include <core/resource/media_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_history.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/ptz/ptz_controller_pool.h>
#include <core/ptz/preset_ptz_controller.h>
#include <core/ptz/tour_ptz_controller.h>
#include <core/ptz/fallback_ptz_controller.h>
#include <core/ptz/activity_ptz_controller.h>
#include <core/ptz/home_ptz_controller.h>
#include <core/ptz/viewport_ptz_controller.h>
#include <core/ptz/fisheye_home_ptz_controller.h>

#include <motion/motion_detection.h>

#include <nx/streaming/media_data_packet.h>
#include <nx/streaming/abstract_archive_stream_reader.h>
#include <nx/utils/log/log.h>
#include <nx/utils/collection.h>

#include <nx/client/desktop/utils/entropix_image_enhancer.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/ui/common/painter_transform_scale_stripper.h>
#include <nx/client/core/utils/geometry.h>
#include <ui/common/recording_status_helper.h>
#include <ui/common/text_pixmap_cache.h>
#include <ui/fisheye/fisheye_ptz_controller.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/items/controls/html_text_item.h>
#include <ui/graphics/items/generic/proxy_label.h>
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
#include <ui/style/helper.h>
#include <ui/style/skin.h>
#include <ui/style/nx_style.h>
#include <ui/style/software_trigger_pixmaps.h>
#include <ui/widgets/properties/camera_settings_tab.h>
#include <ui/workaround/gl_native_painting.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>
#include <ui/workbench/watchers/workbench_render_watcher.h>
#include "ui/workbench/workbench_item.h"

#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/synctime.h>
#include <utils/common/html.h>
#include <utils/common/delayed.h>
#include <utils/license_usage_helper.h>
#include <utils/math/color_transformations.h>
#include <api/common_message_processor.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <utils/media/sse_helper.h>
#include <plugins/resource/avi/avi_resource.h>

using namespace nx;
using namespace client::desktop::ui;

using nx::client::core::utils::Geometry;

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

/* Motion sensitivity grid: */
using MotionGrid = std::array<std::array<int, Qn::kMotionGridWidth>, Qn::kMotionGridHeight>;

/* Fill entire motion grid with zeros: */
void resetMotionGrid(MotionGrid& grid)
{
    for (auto& row : grid)
        row.fill(0);
}

/* Fill specified rectangle of motion grid with specified value: */
void fillMotionRect(MotionGrid& grid, const QRect& rect, int value)
{
    NX_ASSERT(rect.top() >= 0 && rect.bottom() < (int) grid.size());
    NX_ASSERT(rect.left() >= 0 && rect.right() < (int) grid[0].size());

    for (int row = rect.top(); row <= rect.bottom(); ++row)
        std::fill(grid[row].begin() + rect.left(), grid[row].begin() + rect.right() + 1, value);
}

/* Fill sensitivity region that contains specified position with zeros: */
void clearSensitivityRegion(MotionGrid& grid, const QPoint& at)
{
    NX_ASSERT(at.y() >= 0 && at.y() < (int) grid.size());
    NX_ASSERT(at.x() >= 0 && at.x() < (int) grid[0].size());

    int value = grid[at.y()][at.x()];
    if (value == 0)
        return;

    QVarLengthArray<QPoint> pointStack;
    grid[at.y()][at.x()] = 0;
    pointStack.push_back(at);

    while (!pointStack.empty())
    {
        QPoint p = pointStack.back();
        pointStack.pop_back();

        /* Spread left: */
        int x = p.x() - 1;
        if (x >= 0 && grid[p.y()][x] == value)
        {
            grid[p.y()][x] = 0;
            pointStack.push_back({ x, p.y() });
        }

        /* Spread right: */
        x = p.x() + 1;
        if (x < (int) grid[0].size() && grid[p.y()][x] == value)
        {
            grid[p.y()][x] = 0;
            pointStack.push_back({ x, p.y() });
        }

        /* Spread up: */
        int y = p.y() - 1;
        if (y >= 0 && grid[y][p.x()] == value)
        {
            grid[y][p.x()] = 0;
            pointStack.push_back({ p.x(), y });
        }

        /* Spread down: */
        y = p.y() + 1;
        if (y < (int) grid.size() && grid[y][p.x()] == value)
        {
            grid[y][p.x()] = 0;
            pointStack.push_back({ p.x(), y });
        }
    }
};

bool tourIsRunning(QnWorkbenchContext* context)
{
    return context->action(action::ToggleLayoutTourModeAction)->isChecked();
}

} // namespace

QnMediaResourceWidget::QnMediaResourceWidget(QnWorkbenchContext* context, QnWorkbenchItem* item, QGraphicsItem* parent):
    base_type(context, item, parent),
    m_resource(base_type::resource().dynamicCast<QnMediaResource>()),
    m_camera(base_type::resource().dynamicCast<QnVirtualCameraResource>()),
    m_display(nullptr),
    m_renderer(nullptr),
    m_motionSelection(),
    m_motionSelectionPathCache(),
    m_paintedChannels(),
    m_motionSensitivity(),
    m_motionSensitivityValid(false),
    m_binaryMotionMask(),
    m_binaryMotionMaskValid(false),
    m_motionSelectionCacheValid(false),
    m_motionLabelPositionsValid(false),
    m_sensStaticText(),
    m_ptzController(nullptr),
    m_homePtzController(nullptr),
    m_dewarpingParams(),
    m_ioModuleOverlayWidget(nullptr),
    m_ioCouldBeShown(false),
    m_ioLicenceStatusHelper(), /// Will be created only for I/O modules
    m_posUtcMs(DATETIME_INVALID)
{
    NX_ASSERT(m_resource, "Media resource widget was created with a non-media resource.");

    setupHud();

    // TODO: #Elric
    // Strictly speaking, this is a hack.
    // We shouldn't be using OpenGL context in class constructor.
    QGraphicsView *view = QnWorkbenchContextAware::display()->view();
    const QGLWidget *viewport = qobject_cast<const QGLWidget *>(view ? view->viewport() : NULL);
    m_renderer = new QnResourceWidgetRenderer(NULL, viewport ? viewport->context() : NULL);
    connect(m_renderer, &QnResourceWidgetRenderer::sourceSizeChanged, this,
        &QnMediaResourceWidget::updateAspectRatio);
    connect(base_type::resource(), &QnResource::propertyChanged, this,
        &QnMediaResourceWidget::at_resource_propertyChanged);
    connect(base_type::resource(), &QnResource::mediaDewarpingParamsChanged, this,
        &QnMediaResourceWidget::updateDewarpingParams);
    connect(this, &QnResourceWidget::zoomTargetWidgetChanged, this,
        &QnMediaResourceWidget::updateDisplay);
    connect(item, &QnWorkbenchItem::dewarpingParamsChanged, this,
        &QnMediaResourceWidget::updateFisheye);
    connect(item, &QnWorkbenchItem::dewarpingParamsChanged, this,
        &QnMediaResourceWidget::dewarpingParamsChanged);
    connect(item, &QnWorkbenchItem::imageEnhancementChanged, this,
        &QnMediaResourceWidget::at_item_imageEnhancementChanged);
    connect(this, &QnMediaResourceWidget::dewarpingParamsChanged, this,
        &QnMediaResourceWidget::updateFisheye);
    connect(this, &QnResourceWidget::zoomRectChanged, this,
        &QnMediaResourceWidget::updateFisheye);
    connect(this, &QnMediaResourceWidget::dewarpingParamsChanged, this,
        &QnMediaResourceWidget::updateButtonsVisibility);
    if (m_camera)
        connect(m_camera, &QnVirtualCameraResource::motionRegionChanged, this,
            &QnMediaResourceWidget::invalidateMotionSensitivity);
    connect(navigator(), &QnWorkbenchNavigator::bookmarksModeEnabledChanged, this,
        &QnMediaResourceWidget::updateCompositeOverlayMode);

    connect(qnCommonMessageProcessor, &QnCommonMessageProcessor::businessActionReceived, this,
        [this](const vms::event::AbstractActionPtr &businessAction)
        {
            if (businessAction->actionType() != vms::event::executePtzPresetAction)
                return;
            const auto &actionParams = businessAction->getParams();
            if (actionParams.actionResourceId != m_resource->toResource()->getId())
                return;
            if (m_ptzController)
                m_ptzController->activatePreset(actionParams.presetId, QnAbstractPtzController::MaxPtzSpeed);
        });

    connect(context, &QnWorkbenchContext::userChanged,
        this, &QnMediaResourceWidget::resetTriggers);

    updateDisplay();
    updateDewarpingParams();

    /* Set up static text. */
    for (int i = 0; i < QnMotionRegion::kSensitivityLevelCount; ++i)
    {
        m_sensStaticText[i].setText(QString::number(i));
        m_sensStaticText[i].setPerformanceHint(QStaticText::AggressiveCaching);
    }

    updateAspectRatio();
    createPtzController();

    /* Set up info updates. */
    connect(this, &QnMediaResourceWidget::updateInfoTextLater, this,
        &QnMediaResourceWidget::updateInfoText, Qt::QueuedConnection);

    /* Set up overlays */
    if (m_camera && m_camera->hasFlags(Qn::io_module))
    {
        // TODO: #vkutin #gdm #common Make a style metric that holds this value.
        auto topMargin = titleBar()
            ? titleBar()->leftButtonsBar()->uniformButtonSize().height()
            : 0.0;

        m_ioLicenceStatusHelper.reset(new QnSingleCamLicenceStatusHelper(m_camera));
        m_ioModuleOverlayWidget = new QnIoModuleOverlayWidget();
        m_ioModuleOverlayWidget->setIOModule(m_camera);
        m_ioModuleOverlayWidget->setAcceptedMouseButtons(0);
        m_ioModuleOverlayWidget->setUserInputEnabled(accessController()->hasGlobalPermission(Qn::GlobalUserInputPermission));
        m_ioModuleOverlayWidget->setContentsMargins(0.0, topMargin, 0.0, 0.0);
        addOverlayWidget(m_ioModuleOverlayWidget, detail::OverlayParams(Visible, true, true));

        connect(m_ioLicenceStatusHelper, &QnSingleCamLicenceStatusHelper::licenceStatusChanged,
            this,
            [this]
            {
                updateIoModuleVisibility(animationAllowed());
            });

        updateButtonsVisibility();
        updateIoModuleVisibility(false);
    }

    ensureTwoWayAudioWidget();

    /* Set up buttons. */
    createButtons();

    if (m_camera)
    {
        QTimer *timer = new QTimer(this);

        connect(timer, &QTimer::timeout, this, &QnMediaResourceWidget::updateIconButton);
        connect(context->instance<QnWorkbenchServerTimeWatcher>(),
            &QnWorkbenchServerTimeWatcher::displayOffsetsChanged, this,
            &QnMediaResourceWidget::updateIconButton);
        connect(m_camera, &QnResource::statusChanged, this,
            &QnMediaResourceWidget::updateIconButton);

        if (m_camera->hasFlags(Qn::io_module))
        {
            connect(m_camera, &QnResource::statusChanged, this,
                [this]
                {
                    updateIoModuleVisibility(animationAllowed());
                });
        }

        connect(m_camera, &QnSecurityCamResource::scheduleTasksChanged, this,
            &QnMediaResourceWidget::updateIconButton);
        timer->start(1000 * 60); /* Update icon button every minute. */

        const auto controller = statusOverlayController();
        connect(controller, &QnStatusOverlayController::buttonClicked, this,
            [this](Qn::ResourceOverlayButton button)
            {
                switch (button)
                {
                    case Qn::ResourceOverlayButton::Diagnostics:
                        processDiagnosticsRequest();
                        break;
                    case Qn::ResourceOverlayButton::IoEnable:
                        processIoEnableRequest();
                        break;
                    case Qn::ResourceOverlayButton::Settings:
                        processSettingsRequest();
                        break;
                    case Qn::ResourceOverlayButton::MoreLicenses:
                        processMoreLicensesRequest();
                        break;
                    default:
                        break;
                }
            });
    }

    connect(base_type::resource(), &QnResource::resourceChanged, this,
        &QnMediaResourceWidget::updateButtonsVisibility); // TODO: #GDM #Common get rid of resourceChanged

    connect(this, &QnResourceWidget::zoomRectChanged, this,
        &QnMediaResourceWidget::at_zoomRectChanged);
    connect(context->instance<QnWorkbenchRenderWatcher>(), &QnWorkbenchRenderWatcher::widgetChanged,
        this, &QnMediaResourceWidget::at_renderWatcher_widgetChanged);

    // Update buttons for single layout tour start/stop
    connect(action(action::ToggleLayoutTourModeAction), &QAction::toggled, this,
        &QnMediaResourceWidget::updateButtonsVisibility);

    at_camDisplay_liveChanged();
    at_ptzButton_toggled(false);
    at_histogramButton_toggled(item->imageEnhancement().enabled);
    updateButtonsVisibility();
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

    connect(this, &QnMediaResourceWidget::updateInfoTextLater, this,
        &QnMediaResourceWidget::updateCurrentUtcPosMs);

    connect(this, &QnMediaResourceWidget::positionChanged, this,
        &QnMediaResourceWidget::clearEntropixEnhancedImage);
    connect(this, &QnMediaResourceWidget::zoomRectChanged, this,
        &QnMediaResourceWidget::clearEntropixEnhancedImage);
}

QnMediaResourceWidget::~QnMediaResourceWidget()
{
    ensureAboutToBeDestroyedEmitted();

    if (m_display)
        m_display->removeRenderer(m_renderer);

    m_renderer->destroyAsync();

    for (auto* data : m_binaryMotionMask)
        qFreeAligned(data);
    m_binaryMotionMask.clear();
}

void QnMediaResourceWidget::initSoftwareTriggers()
{
    if (!display()->camDisplay()->isRealTimeSource())
        return;

    if (item()->layout()->isSearchLayout())
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

void QnMediaResourceWidget::updateTriggerAvailability(const vms::event::RulePtr& rule)
{
    if (!rule)
        return;

    const auto triggerIt = m_softwareTriggers.find(rule->id());
    if (triggerIt == m_softwareTriggers.end())
        return;

    const auto button = qobject_cast<QnSoftwareTriggerButton*>(
        m_triggersContainer->item(triggerIt.value().overlayItemId));

    if (!button)
        return;

    const bool buttonEnabled = rule && rule->isScheduleMatchTime(qnSyncTime->currentDateTime())
        || !button->isLive();

    if (button->isEnabled() == buttonEnabled)
        return;

    const auto info = triggerIt.value().info;

    if (!buttonEnabled)
    {
        const bool longPressed = info.prolonged &&
            button->state() == QnSoftwareTriggerButton::State::Waiting;
        if (longPressed)
            button->setState(QnSoftwareTriggerButton::State::Failure);
    }

    button->setEnabled(buttonEnabled);
    updateTriggerButtonTooltip(button, info, buttonEnabled);
}

void QnMediaResourceWidget::updateTriggersAvailability()
{
    for (auto ruleId: m_softwareTriggers.keys())
        updateTriggerAvailability(commonModule()->eventRuleManager()->rule(ruleId));
}

void QnMediaResourceWidget::createButtons()
{
    {
        QnImageButtonWidget *screenshotButton = createStatisticAwareButton(lit("media_widget_screenshot"));
        screenshotButton->setIcon(qnSkin->icon("item/screenshot.png"));
        screenshotButton->setCheckable(false);
        screenshotButton->setToolTip(tr("Screenshot"));
        setHelpTopic(screenshotButton, Qn::MainWindow_MediaItem_Screenshot_Help);
        connect(screenshotButton, &QnImageButtonWidget::clicked, this,
            &QnMediaResourceWidget::at_screenshotButton_clicked);
        titleBar()->rightButtonsBar()->addButton(Qn::ScreenshotButton, screenshotButton);
    }

    {
        QnImageButtonWidget *searchButton = createStatisticAwareButton(lit("media_widget_msearch"));
        searchButton->setIcon(qnSkin->icon("item/search.png"));
        searchButton->setCheckable(true);
        searchButton->setToolTip(tr("Smart Search"));
        setHelpTopic(searchButton, Qn::MainWindow_MediaItem_SmartSearch_Help);
        connect(searchButton, &QnImageButtonWidget::toggled, this,
            &QnMediaResourceWidget::setMotionSearchModeEnabled);
        titleBar()->rightButtonsBar()->addButton(Qn::MotionSearchButton, searchButton);
    }

    {
        QnImageButtonWidget *ptzButton = createStatisticAwareButton(lit("media_widget_ptz"));
        ptzButton->setIcon(qnSkin->icon("item/ptz.png"));
        ptzButton->setCheckable(true);
        ptzButton->setToolTip(tr("PTZ"));
        setHelpTopic(ptzButton, Qn::MainWindow_MediaItem_Ptz_Help);
        connect(ptzButton, &QnImageButtonWidget::toggled, this,
            &QnMediaResourceWidget::at_ptzButton_toggled);
        titleBar()->rightButtonsBar()->addButton(Qn::PtzButton, ptzButton);
    }

    {
        QnImageButtonWidget *fishEyeButton = createStatisticAwareButton(lit("media_widget_fisheye"));
        fishEyeButton->setIcon(qnSkin->icon("item/fisheye.png"));
        fishEyeButton->setCheckable(true);
        fishEyeButton->setToolTip(tr("Dewarping"));
        fishEyeButton->setChecked(item()->dewarpingParams().enabled);
        setHelpTopic(fishEyeButton, Qn::MainWindow_MediaItem_Dewarping_Help);
        connect(fishEyeButton, &QnImageButtonWidget::toggled, this,
            &QnMediaResourceWidget::at_fishEyeButton_toggled);
        titleBar()->rightButtonsBar()->addButton(Qn::FishEyeButton, fishEyeButton);
    }

    {
        QnImageButtonWidget *zoomWindowButton = createStatisticAwareButton(lit("media_widget_zoom"));
        zoomWindowButton->setIcon(qnSkin->icon("item/zoom_window.png"));
        zoomWindowButton->setCheckable(true);
        zoomWindowButton->setToolTip(tr("Create Zoom Window"));
        setHelpTopic(zoomWindowButton, Qn::MainWindow_MediaItem_ZoomWindows_Help);
        connect(zoomWindowButton, &QnImageButtonWidget::toggled, this,
            &QnMediaResourceWidget::setZoomWindowCreationModeEnabled);
        titleBar()->rightButtonsBar()->addButton(Qn::ZoomWindowButton, zoomWindowButton);
    }

    {
        QnImageButtonWidget *enhancementButton = createStatisticAwareButton(lit("media_widget_enhancement"));
        enhancementButton->setIcon(qnSkin->icon("item/image_enhancement.png"));
        enhancementButton->setCheckable(true);
        enhancementButton->setToolTip(tr("Image Enhancement"));
        enhancementButton->setChecked(item()->imageEnhancement().enabled);
        setHelpTopic(enhancementButton, Qn::MainWindow_MediaItem_ImageEnhancement_Help);
        connect(enhancementButton, &QnImageButtonWidget::toggled, this,
            &QnMediaResourceWidget::at_histogramButton_toggled);
        titleBar()->rightButtonsBar()->addButton(Qn::EnhancementButton, enhancementButton);
    }

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

    {
        auto entropixEnhancementButton =
            createStatisticAwareButton(lit("media_widget_entropix_enhancement"));
        entropixEnhancementButton->setIcon(qnSkin->icon("item/image_enhancement.png"));
        entropixEnhancementButton->setToolTip(lit("Entropix Image Enhancement"));
        connect(entropixEnhancementButton, &QnImageButtonWidget::clicked, this,
            &QnMediaResourceWidget::at_entropixEnhancementButton_clicked);
        titleBar()->rightButtonsBar()->addButton(
            Qn::EntropixEnhancementButton, entropixEnhancementButton);
    }
}

void QnMediaResourceWidget::createPtzController()
{
    auto threadPool = qnClientCoreModule->ptzControllerPool()->commandThreadPool();
    auto executorThread = qnClientCoreModule->ptzControllerPool()->executorThread();

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

    if (m_camera)
    {
        if (QnPtzControllerPtr serverController = qnPtzPool->controller(m_camera))
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
}

qreal QnMediaResourceWidget::calculateVideoAspectRatio() const
{
    const auto aviResource = m_resource.dynamicCast<QnAviResource>();
    if (aviResource && aviResource->flags().testFlag(Qn::still_image))
    {
        const auto aspect = aviResource->imageAspectRatio();
        if (aspect.isValid())
            return aspect.toFloat();
    }

    /* Here we get 0.0 if no custom aspect ratio set. */
    qreal result = resource()->customAspectRatio();
    if (!qFuzzyIsNull(result))
        return result;

    const auto& camera = resource()->toResourcePtr().dynamicCast<QnVirtualCameraResource>();

    if (m_renderer && !m_renderer->sourceSize().isEmpty())
    {
        auto sourceSize = m_renderer->sourceSize();
        if (camera)
        {
            const auto& sensor = camera->combinedSensorsDescription().mainSensor();
            if (sensor.isValid())
                sourceSize = Geometry::cwiseMul(sourceSize, sensor.geometry.size()).toSize();
        }
        return Geometry::aspectRatio(sourceSize);
    }

    if (camera)
    {
        const auto cameraAr = camera->aspectRatio();
        if (cameraAr.isValid())
            return cameraAr.toFloat();
    }

    return defaultAspectRatio(); /*< Here we can get -1.0 if there are no predefined AR set */
}

const QnMediaResourcePtr &QnMediaResourceWidget::resource() const
{
    return m_resource;
}

QnResourceDisplayPtr QnMediaResourceWidget::display() const
{
    return m_display;
}

QnResourceWidgetRenderer* QnMediaResourceWidget::renderer() const
{
    return m_renderer;
}

bool QnMediaResourceWidget::hasVideo() const
{
    return m_resource
        && m_resource->hasVideo(m_display ? m_display->mediaProvider() : nullptr);
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
        channelLayout()->size(), QSize(Qn::kMotionGridWidth, Qn::kMotionGridHeight));
}

QPoint QnMediaResourceWidget::channelGridOffset(int channel) const
{
    return Geometry::cwiseMul(
        channelLayout()->position(channel), QSize(Qn::kMotionGridWidth, Qn::kMotionGridHeight));
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

    qreal opacity = effectiveOpacity();
    bool opaque = qFuzzyCompare(opacity, 1.0);
    // always use blending for images --gdm
    if (!opaque || (base_type::resource()->flags() & Qn::still_image))
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    m_renderer->setBlurFactor(m_statusOverlay->opacity());
    const auto result = m_renderer->paint(channel, sourceSubRect, targetRect, effectiveOpacity());
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

    const auto updateTriggersMinHeight =
        [this]()
        {
            // Calculate minimum height for downscaling no more than kMaxDownscaleFactor times.
            static const qreal kMaxDownscaleFactor = 2.0;
            const auto content = m_hudOverlay->content();
            const qreal available = content->fixedSize().height() / content->sceneScale();
            const qreal desired = m_triggersContainer->effectiveSizeHint(Qt::PreferredSize).height();
            const qreal extra = content->size().height() - m_triggersContainer->size().height();
            const qreal min = qMin(desired, available * kMaxDownscaleFactor - extra);
            m_triggersContainer->setMinimumHeight(min);
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

void QnMediaResourceWidget::ensureTwoWayAudioWidget()
{
    /* Check if widget is already created. */
    if (m_twoWayAudioWidget)
        return;

    bool hasTwoWayAudio = m_camera && m_camera->hasTwoWayAudio()
        && accessController()->hasGlobalPermission(Qn::GlobalUserInputPermission);

    if (!hasTwoWayAudio)
        return;

    m_twoWayAudioWidget = new QnTwoWayAudioWidget();
    m_twoWayAudioWidget->setCamera(m_camera);
    m_twoWayAudioWidget->setFixedHeight(kTriggerButtonSize);
    context()->statisticsModule()->registerButton(lit("two_way_audio"), m_twoWayAudioWidget);

    /* Items are ordered left-to-right and top-to bottom, so we are inserting two-way audio item on top. */
    m_triggersContainer->insertItem(0, m_twoWayAudioWidget);
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
    ensureMotionSensitivity();

    bool changed = false;

    for (int i = 0; i < channelCount(); ++i)
    {
        QRect rect = gridRect.translated(-channelGridOffset(i)).intersected(QRect(0, 0, Qn::kMotionGridWidth, Qn::kMotionGridHeight));
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

void QnMediaResourceWidget::clearMotionSelection()
{
    if (isMotionSelectionEmpty())
        return;

    for (int i = 0; i < m_motionSelection.size(); ++i)
        m_motionSelection[i] = QRegion();

    invalidateMotionSelectionCache();
    emit motionSelectionChanged();
}

void QnMediaResourceWidget::setMotionSelection(const QList<QRegion> &regions)
{
    if (regions.size() != m_motionSelection.size())
    {
        qWarning() << "invalid motion selection list";
        return;
    }

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

    if (m_camera)
    {
        m_motionSensitivity = m_camera->getMotionRegionList();

        if (m_motionSensitivity.size() != channelCount())
        {
            qnWarning("Camera '%1' returned a motion sensitivity list of invalid size.", m_camera->getName());
            qnResizeList(m_motionSensitivity, channelCount());
        }
    }
    else if (m_resource->toResource()->hasFlags(Qn::motion))
    {
        for (int i = 0, count = channelCount(); i < count; i++)
            m_motionSensitivity.push_back(QnMotionRegion());
    }
    else
    {
        m_motionSensitivity.clear();
    }

    invalidateMotionLabelPositions();
    invalidateBinaryMotionMask();

    m_motionSensitivityValid = true;
}

bool QnMediaResourceWidget::addToMotionSensitivity(const QRect &gridRect, int sensitivity)
{
    ensureMotionSensitivity();

    bool changed = false;
    if (m_camera)
    {
        QnConstResourceVideoLayoutPtr layout = m_camera->getVideoLayout();

        for (int i = 0; i < layout->channelCount(); ++i)
        {
            QRect r(0, 0, Qn::kMotionGridWidth, Qn::kMotionGridHeight);
            r.translate(channelGridOffset(i));
            r = gridRect.intersected(r);
            r.translate(-channelGridOffset(i));
            if (!r.isEmpty())
            {
                m_motionSensitivity[i].addRect(sensitivity, r);
                changed = true;
            }
        }
    }

    if (sensitivity == 0)
        invalidateBinaryMotionMask();

    invalidateMotionLabelPositions();

    return changed;
}

bool QnMediaResourceWidget::setMotionSensitivityFilled(const QPoint &gridPos, int sensitivity)
{
    ensureMotionSensitivity();

    int channel = 0;
    QPoint channelPos = gridPos;
    if (m_camera)
    {
        QnConstResourceVideoLayoutPtr layout = m_camera->getVideoLayout();

        for (int i = 0; i < layout->channelCount(); ++i)
        {
            QRect r(channelGridOffset(i), QSize(Qn::kMotionGridWidth, Qn::kMotionGridHeight));
            if (r.contains(channelPos))
            {
                channelPos -= r.topLeft();
                channel = i;
                break;
            }
        }
    }

    if (!m_motionSensitivity[channel].updateSensitivityAt(channelPos, sensitivity))
        return false;

    invalidateBinaryMotionMask();
    invalidateMotionLabelPositions();

    return true;
}

void QnMediaResourceWidget::clearMotionSensitivity()
{
    for (int i = 0; i < channelCount(); i++)
        m_motionSensitivity[i] = QnMotionRegion();
    m_motionSensitivityValid = true;

    invalidateBinaryMotionMask();
    invalidateMotionLabelPositions();
}

const QList<QnMotionRegion> &QnMediaResourceWidget::motionSensitivity() const
{
    ensureMotionSensitivity();

    return m_motionSensitivity;
}

void QnMediaResourceWidget::ensureBinaryMotionMask() const
{
    if (m_binaryMotionMaskValid)
        return;

    ensureMotionSensitivity();
    for (int i = 0; i < channelCount(); ++i)
        QnMetaDataV1::createMask(m_motionSensitivity[i].getMotionMask(), reinterpret_cast<char *>(m_binaryMotionMask[i]));
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

void QnMediaResourceWidget::ensureMotionLabelPositions() const
{
    /* Ensure motion sensitivity.
     * m_motionLabelPositionsValid may be cleared in process. */
    ensureMotionSensitivity();

    if (m_motionLabelPositionsValid)
        return;

    MotionGrid grid;

    /* Clear existing positions without freeing memory to avoid unnecessary reallocations: */
    m_motionLabelPositions.resize(m_motionSensitivity.size());
    for (auto& positionArrays : m_motionLabelPositions)
        for (auto& positionArray : positionArrays)
            positionArray.resize(0);

    /* Analyze motion sensitivity for each channel: */
    for (int channel = 0; channel < m_motionSensitivity.size(); ++channel)
    {
        /* Clear grid initially: */
        resetMotionGrid(grid);

        /* Fill grid with sensitivity numbers: */
        for (int sensitivity = 1; sensitivity < QnMotionRegion::kSensitivityLevelCount; ++sensitivity)
        {
            for (const auto& rect : m_motionSensitivity[channel].getRectsBySens(sensitivity))
                fillMotionRect(grid, rect, sensitivity);
        }

        /* Label takes 1x2 cells. Find good areas to fit labels in,
         * going from the top to the bottom, from the left to the right:
         */
        for (int y = 0; y < (int) grid.size() - 1; ++y)
        {
            for (int x = 0; x < (int) grid[0].size(); ++x)
            {
                int sensitivity = grid[y][x];

                /* If there's no motion detection region, or we already labeled it: */
                if (sensitivity == 0)
                    continue;

                /* If a label doesn't fit: */
                if (grid[y + 1][x] != sensitivity)
                    continue;

                /* Add label position: */
                QPoint pos(x, y);
                m_motionLabelPositions[channel][sensitivity] << pos;

                /* Clear processed region: */
                clearSensitivityRegion(grid, pos);
            }
        }
    }
}

void QnMediaResourceWidget::invalidateMotionLabelPositions() const
{
    m_motionLabelPositionsValid = false;
}

void QnMediaResourceWidget::setDisplay(const QnResourceDisplayPtr &display)
{
    if (display == m_display)
        return;

    if (m_display)
    {
        m_display->removeRenderer(m_renderer);
        disconnect(m_display->camDisplay(), NULL, this, NULL);
    }

    m_display = display;

    if (m_display)
    {
        connect(m_display->camDisplay(), SIGNAL(stillImageChanged()), this, SLOT(updateButtonsVisibility()));
        connect(m_display->camDisplay(), SIGNAL(liveMode(bool)), this, SLOT(at_camDisplay_liveChanged()));
        connect(m_resource->toResource(), SIGNAL(videoLayoutChanged(const QnResourcePtr &)), this, SLOT(at_videoLayoutChanged()));

        connect(m_display->camDisplay(), &QnCamDisplay::liveMode, this,
            [this](bool /* live */)
            {
                if (m_camera && m_camera->hasFlags(Qn::io_module))
                    updateIoModuleVisibility(animationAllowed());
            });

        if (const auto archiveReader = m_display->archiveReader())
        {
            connect(archiveReader, &QnAbstractArchiveStreamReader::streamPaused,
                this, &QnMediaResourceWidget::updateButtonsVisibility);
            connect(archiveReader, &QnAbstractArchiveStreamReader::streamResumed,
                this, &QnMediaResourceWidget::updateButtonsVisibility);
            connect(archiveReader, &QnAbstractArchiveStreamReader::streamResumed,
                this, &QnMediaResourceWidget::clearEntropixEnhancedImage);
        }

        setChannelLayout(m_display->videoLayout());
        m_display->addRenderer(m_renderer);
        m_renderer->setChannelCount(m_display->videoLayout()->channelCount());
        updateCustomAspectRatio();
    }
    else
    {
        setChannelLayout(QnConstResourceVideoLayoutPtr(new QnDefaultResourceVideoLayout()));
        m_renderer->setChannelCount(0);
    }

    const bool canRotate = accessController()->hasPermissions(item()->layout()->resource(),
        Qn::WritePermission);
    setOption(QnResourceWidget::WindowRotationForbidden, !hasVideo() || !canRotate);

    clearEntropixEnhancedImage();

    emit displayChanged();
}

void QnMediaResourceWidget::at_videoLayoutChanged()
{
    setChannelLayout(m_display->videoLayout());
}

void QnMediaResourceWidget::updateDisplay()
{
    QnMediaResourceWidget *zoomTargetWidget = dynamic_cast<QnMediaResourceWidget *>(this->zoomTargetWidget());

    QnResourceDisplayPtr display;
    if (zoomTargetWidget)
    {
        display = zoomTargetWidget->display();
    }
    else
    {
        display = QnResourceDisplayPtr(new QnResourceDisplay(m_resource->toResourcePtr(), this));
    }

    setDisplay(display);
}

void QnMediaResourceWidget::updateIconButton()
{
    auto buttonsBar = titleBar()->leftButtonsBar();
    if (isZoomWindow())
    {
        auto iconButton = buttonsBar->button(Qn::RecordingStatusIconButton);
        iconButton->setIcon(qnSkin->icon("item/zoom_window_hovered.png"));
        iconButton->setToolTip(tr("Zoom Window"));

        buttonsBar->setButtonsVisible(Qn::RecordingStatusIconButton, true);
        return;
    }

    if (!m_camera)
    {
        buttonsBar->setButtonsVisible(Qn::RecordingStatusIconButton, false);
        return;
    }

    int recordingMode = QnRecordingStatusHelper::currentRecordingMode(m_camera);
    QIcon recIcon = QnRecordingStatusHelper::icon(recordingMode);

    buttonsBar->setButtonsVisible(Qn::RecordingStatusIconButton, !recIcon.isNull());

    auto iconButton = buttonsBar->button(Qn::RecordingStatusIconButton);
    iconButton->setIcon(recIcon);
    iconButton->setToolTip(QnRecordingStatusHelper::tooltip(recordingMode));
}

void QnMediaResourceWidget::updateRendererEnabled()
{
    if (m_resource->toResourcePtr()->hasFlags(Qn::still_image))
        return;

    for (int channel = 0; channel < channelCount(); channel++)
        m_renderer->setEnabled(channel, !exposedRect(channel, true, true, false).isEmpty());
}

ImageCorrectionParams QnMediaResourceWidget::imageEnhancement() const
{
    return item()->imageEnhancement();
}

void QnMediaResourceWidget::setImageEnhancement(const ImageCorrectionParams &imageEnhancement)
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

    if (m_camera && m_camera->hasCombinedSensors())
    {
        const auto& sensor = m_camera->combinedSensorsDescription().mainSensor();
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
    else
    {
        const PainterTransformScaleStripper scaleStripper(painter);
        result = paintVideoTexture(painter,
            channel,
            sourceSubRect,
            scaleStripper.mapRect(paintRect));
    }

    if (result != Qn::NewFrameRendered && result != Qn::OldFrameRendered)
        base_type::paintChannelBackground(painter, channel, channelRect, paintRect);

    return result;
}

void QnMediaResourceWidget::paintChannelForeground(QPainter *painter, int channel, const QRectF &rect)
{
    if (options() & DisplayMotion)
    {
        ensureMotionSelectionCache();

        auto metadata = m_renderer->lastFrameMetadata(channel);
        if (!metadata || metadata->dataType != QnAbstractMediaData::DataType::META_V1)
            return;

        paintMotionGrid(
            painter,
            channel,
            rect,
            std::dynamic_pointer_cast<QnMetaDataV1>(metadata));

        paintMotionSensitivity(painter, channel, rect);

        /* Motion selection. */
        if (!m_motionSelection[channel].isEmpty())
        {
            QColor color = toTransparent(qnGlobals->mrsColor(), 0.2);
            paintFilledRegionPath(painter, rect, m_motionSelectionPathCache[channel], color, color);
        }
    }

    if (m_entropixProgress >= 0)
        paintProgress(painter, rect, m_entropixProgress);
}

void QnMediaResourceWidget::paintMotionGrid(QPainter *painter, int channel, const QRectF &rect, const QnMetaDataV1Ptr &motion)
{
    // 5-7 fps

    ensureMotionSensitivity();

    qreal xStep = rect.width() / Qn::kMotionGridWidth;
    qreal yStep = rect.height() / Qn::kMotionGridHeight;

    QVector<QPointF> gridLines[2];

    if (motion && motion->channelNumber == (quint32)channel)
    {
        // 2-3 fps

        ensureBinaryMotionMask();
        motion->removeMotion(m_binaryMotionMask[channel]);

        /* Horizontal lines. */
        for (int y = 1; y < Qn::kMotionGridHeight; ++y)
        {
            bool isMotion = motion->isMotionAt(0, y - 1) || motion->isMotionAt(0, y);
            gridLines[isMotion] << QPointF(0, y * yStep);
            int x = 1;
            while (x < Qn::kMotionGridWidth)
            {
                while (x < Qn::kMotionGridWidth && isMotion == (motion->isMotionAt(x, y - 1) || motion->isMotionAt(x, y)))
                    x++;
                gridLines[isMotion] << QPointF(x * xStep, y * yStep);
                if (x < Qn::kMotionGridWidth)
                {
                    isMotion = !isMotion;
                    gridLines[isMotion] << QPointF(x * xStep, y * yStep);
                }
            }
        }

        /* Vertical lines. */
        for (int x = 1; x < Qn::kMotionGridWidth; ++x)
        {
            bool isMotion = motion->isMotionAt(x - 1, 0) || motion->isMotionAt(x, 0);
            gridLines[isMotion] << QPointF(x * xStep, 0);
            int y = 1;
            while (y < Qn::kMotionGridHeight)
            {
                while (y < Qn::kMotionGridHeight && isMotion == (motion->isMotionAt(x - 1, y) || motion->isMotionAt(x, y)))
                    y++;
                gridLines[isMotion] << QPointF(x * xStep, y * yStep);
                if (y < Qn::kMotionGridHeight)
                {
                    isMotion = !isMotion;
                    gridLines[isMotion] << QPointF(x * xStep, y * yStep);
                }
            }
        }
    }
    else
    {
        for (int x = 1; x < Qn::kMotionGridWidth; ++x)
            gridLines[0] << QPointF(x * xStep, 0.0) << QPointF(x * xStep, rect.height());
        for (int y = 1; y < Qn::kMotionGridHeight; ++y)
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
    painter->scale(rect.width() / Qn::kMotionGridWidth, rect.height() / Qn::kMotionGridHeight);
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

void QnMediaResourceWidget::paintMotionSensitivityIndicators(QPainter* painter, int channel, const QRectF& rect)
{
    QPoint channelOffset = channelGridOffset(channel);

    qreal xStep = rect.width() / Qn::kMotionGridWidth;
    qreal yStep = rect.height() / Qn::kMotionGridHeight;
    qreal xOffset = channelOffset.x() + 0.1;
    qreal yOffset = channelOffset.y();
    qreal fontIncrement = 1.2;

    painter->setPen(Qt::black);
    QFont font;
    font.setPointSizeF(yStep * fontIncrement);
    font.setBold(true);
    painter->setFont(font);

    ensureMotionLabelPositions();

    /* Zero sensitivity is skipped as there should not be painted zeros */
    for (int sensitivity = 1; sensitivity < QnMotionRegion::kSensitivityLevelCount; ++sensitivity)
    {
        m_sensStaticText[sensitivity].prepare(painter->transform(), font);

        for (const auto& pos : m_motionLabelPositions[channel][sensitivity])
        {
            painter->drawStaticText(
                (pos.x() + xOffset) * xStep,
                (pos.y() + yOffset) * yStep,
                m_sensStaticText[sensitivity]);
        }
    }
}

void QnMediaResourceWidget::paintMotionSensitivity(QPainter* painter, int channel, const QRectF& rect)
{
    ensureMotionSensitivity();

    if (options() & DisplayMotionSensitivity)
    {
        NX_ASSERT(m_motionSensitivityColors.size() == QnMotionRegion::kSensitivityLevelCount, Q_FUNC_INFO);
        for (int i = 1; i < QnMotionRegion::kSensitivityLevelCount; ++i)
        {
            QColor color = i < m_motionSensitivityColors.size() ? m_motionSensitivityColors[i] : QColor(Qt::darkRed);
            color.setAlphaF(kMotionRegionAlpha);
            QPainterPath path = m_motionSensitivity[channel].getRegionBySensPath(i);
            paintFilledRegionPath(painter, rect, path, color, Qt::black);
        }

        paintMotionSensitivityIndicators(painter, channel, rect);
    }
    else
    {
        paintFilledRegionPath(painter, rect, m_motionSensitivity[channel].getMotionMaskPath(), qnGlobals->motionMaskColor(), qnGlobals->motionMaskColor());
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

    emit dewarpingParamsChanged();
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
int QnMediaResourceWidget::helpTopicAt(const QPointF &) const
{

    auto isIoModule = [this]()
        {
            if (!m_resource->toResource()->flags().testFlag(Qn::io_module))
                return false;

            if (m_camera
                && m_display
                && !m_camera->hasVideo(m_display->mediaProvider()))
                return true;

            return (m_ioModuleOverlayWidget && overlayWidgetVisibility(m_ioModuleOverlayWidget) == OverlayVisibility::Visible);
        };

    if (action(action::ToggleLayoutTourModeAction)->isChecked())
        return Qn::MainWindow_Scene_TourInProgress_Help;

    const Qn::ResourceStatusOverlay statusOverlay = statusOverlayController()->statusOverlay();

    if (statusOverlay == Qn::AnalogWithoutLicenseOverlay)
        return Qn::MainWindow_MediaItem_AnalogLicense_Help;

    if (statusOverlay == Qn::OfflineOverlay)
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

    if (options().testFlag(DisplayMotionSensitivity))
        return Qn::CameraSettings_Motion_Help;

    if (options().testFlag(DisplayMotion))
        return Qn::MainWindow_MediaItem_SmartSearch_Help;

    if (isIoModule())
        return Qn::IOModules_Help;

    if (m_resource->toResource()->hasFlags(Qn::local))
        return Qn::MainWindow_MediaItem_Local_Help;

    if (m_camera && m_camera->isDtsBased())
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
        m_binaryMotionMask.push_back(static_cast<simd128i *>(qMallocAligned(Qn::kMotionGridWidth * Qn::kMotionGridHeight / 8, 32)));
        memset(m_binaryMotionMask.back(), 0, Qn::kMotionGridWidth * Qn::kMotionGridHeight / 8);
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
        if (QnAbstractArchiveStreamReader *reader = m_display->archiveReader())
            reader->setSendMotion(options() & DisplayMotion);

        titleBar()->rightButtonsBar()->setButtonsChecked(Qn::MotionSearchButton, options() & DisplayMotion);

        if (options().testFlag(DisplayMotion))
        {
            setProperty(Qn::MotionSelectionModifiers, 0);
        }
        else
        {
            setProperty(Qn::MotionSelectionModifiers, QVariant()); /* Use defaults. */
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
        const QnMediaStreamStatistics *statistics = m_display->mediaProvider()->getStatistics(i);
        if (statistics->isConnectionLost()) // TODO: #GDM check does not work, case #3993
            continue;
        fps = qMax(fps, static_cast<qreal>(statistics->getFrameRate()));
        mbps += statistics->getBitrateMbps();
    }

    QSize size = m_display->camDisplay()->getRawDataSize();
    size.setWidth(size.width() * m_display->camDisplay()->channelsCount());

    QString codecString;
    if (QnConstMediaContextPtr codecContext = m_display->mediaProvider()->getCodecContext())
        codecString = codecContext->getCodecName();


    QString hqLqString;
    if (hasVideo() && !m_resource->toResource()->hasFlags(Qn::local))
        hqLqString = (m_renderer->isLowQualityImage(0)) ? tr("Lo-Res") : tr("Hi-Res");

    static const int kDetailsTextPixelSize = 11;

    QString result;
    if (hasVideo())
    {
        result.append(htmlFormattedParagraph(lit("%1x%2").arg(size.width()).arg(size.height()), kDetailsTextPixelSize, true));
        result.append(htmlFormattedParagraph(lit("%1fps").arg(fps, 0, 'f', 2), kDetailsTextPixelSize, true));
    }
    result.append(htmlFormattedParagraph(lit("%1Mbps").arg(mbps, 0, 'f', 2), kDetailsTextPixelSize, true));
    result.append(htmlFormattedParagraph(codecString, kDetailsTextPixelSize, true));
    result.append(htmlFormattedParagraph(hqLqString, kDetailsTextPixelSize, true));

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
    if (!m_resource->toResourcePtr()->flags().testFlag(Qn::utc))
        return QString();

    static const auto extractTime = [](qint64 dateTimeUsec)
        {
            if (isSpecialDateTimeValueUsec(dateTimeUsec))
                return QString();

            static const auto kOutputFormat = lit("yyyy-MM-dd hh:mm:ss");

            const auto dateTimeMs = dateTimeUsec / kMicroInMilliSeconds;
            return QDateTime::fromMSecsSinceEpoch(dateTimeMs).toString(kOutputFormat);
        };

    const QString timeString = (m_display->camDisplay()->isRealTimeSource()
        ? tr("LIVE") : extractTime(getDisplayTimeUsec()));

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
    bool hasVideo = this->hasVideo();

    if (qnRuntime->isDevMode())
        result |= Qn::DbgScreenshotButton;

    if (hasVideo && !base_type::resource()->hasFlags(Qn::still_image))
        result |= Qn::ScreenshotButton;

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

    if (!rgbImage && hasVideo)
        result |= Qn::EnhancementButton;

    if (isZoomWindow())
    {
        if (m_display && m_display->isPaused()
            && m_camera && m_camera->hasCombinedSensors())
        {
            result |= Qn::EntropixEnhancementButton;
        }

        return result;
    }

    if (hasVideo && base_type::resource()->hasFlags(Qn::motion))
    {
        result |= Qn::MotionSearchButton;
    }

    bool isExportedLayout = item()
        && item()->layout()
        && item()->layout()->resource()
        && item()->layout()->resource()->isFile();

    bool isPreviewSearchLayout = item()
        && item()->layout()
        && item()->layout()->isSearchLayout();

    if (m_camera
        && m_camera->hasAnyOfPtzCapabilities(Ptz::ContinuousPtzCapabilities)
        && !m_camera->hasAnyOfPtzCapabilities(Ptz::VirtualPtzCapability)
        && accessController()->hasPermissions(m_resource->toResourcePtr(), Qn::WritePtzPermission)
        && !isExportedLayout
        && !isPreviewSearchLayout
        )
    {
        result |= Qn::PtzButton;
    }

    if (m_dewarpingParams.enabled)
    {
        result |= Qn::FishEyeButton;
        result &= ~Qn::PtzButton;
    }

    if (hasVideo && base_type::resource()->hasFlags(Qn::io_module))
        result |= Qn::IoModuleButton;

    if (hasVideo && !qnSettings->lightMode().testFlag(Qn::LightModeNoZoomWindows))
    {
        if (item()
            && item()->layout()
            && accessController()->hasPermissions(item()->layout()->resource(),
                Qn::WritePermission | Qn::AddRemoveItemsPermission)
            && !tourIsRunning(context())
            )
            result |= Qn::ZoomWindowButton;
    }

    return result;
}

Qn::ResourceStatusOverlay QnMediaResourceWidget::calculateStatusOverlay() const
{
    if (qnRuntime->isVideoWallMode() && !QnVideoWallLicenseUsageHelper(commonModule()).isValid())
        return Qn::VideowallWithoutLicenseOverlay;

    QnResourcePtr resource = m_display->resource();

    /// TODO: #ynikitenkov It needs to refactor error\status overlays totally!
    const ResourceStates states = getResourceStates();

    // TODO: #GDM #3.1 This really requires hell a lot of refactoring
    // for live video make a quick check: status has higher priority than EOF
    if (states.isRealTimeSource)
    {
        if (states.isOffline)
            return Qn::OfflineOverlay;

        if (states.isUnauthorized)
            return Qn::UnauthorizedOverlay;
    }

    if (m_camera && m_camera->hasFlags(Qn::io_module))
    {
        if (states.isOffline)
            return Qn::OfflineOverlay;

        if (states.isUnauthorized)
            return Qn::UnauthorizedOverlay;

        if (!states.isRealTimeSource)
        {
            if (m_display->camDisplay()->isLongWaiting())
                return Qn::NoDataOverlay;
            return Qn::NoVideoDataOverlay;
        }

        if (m_ioCouldBeShown) /// If widget could be shown then licenses Ok
            return Qn::EmptyOverlay;

        if (m_ioLicenceStatusHelper->status() != QnSingleCamLicenceStatusHelper::LicenseUsed)
            return Qn::IoModuleDisabledOverlay;
    }

    if (m_display->camDisplay()->isEOFReached())
    {
        return resource->getStatus() == Qn::Online && states.isRealTimeSource
            ? Qn::LoadingOverlay
            : Qn::NoDataOverlay;
    }

    if (resource->hasFlags(Qn::local_image))
    {
        if (resource->getStatus() == Qn::Offline)
            return Qn::NoDataOverlay;
        return Qn::EmptyOverlay;
    }

    if (resource->hasFlags(Qn::local_video) && resource->getStatus() == Qn::Offline)
        return Qn::NoDataOverlay;

    if (states.isOffline)
        return Qn::OfflineOverlay;

    if (states.isUnauthorized)
        return Qn::UnauthorizedOverlay;

    if (m_camera && m_camera->isDtsBased() && !m_camera->isLicenseUsed())
        return Qn::AnalogWithoutLicenseOverlay;

    if (options().testFlag(DisplayActivity) && m_display->isPaused())
    {
        if (!qnRuntime->isVideoWallMode())
            return Qn::PausedOverlay;

        return Qn::EmptyOverlay;
    }

    if (m_display->camDisplay()->isLongWaiting())
    {
        auto loader = context()->instance<QnCameraDataManager>()->loader(m_resource, false);
        if (loader && loader->periods(Qn::RecordingContent).containTime(m_display->camDisplay()->getExternalTime() / 1000))
            return base_type::calculateStatusOverlay(Qn::Online, states.hasVideo);

        return Qn::NoDataOverlay;
    }

    if (m_display->isPaused())
    {
        if (!states.hasVideo)
            return Qn::NoVideoDataOverlay;

        return Qn::EmptyOverlay;
    }

    return base_type::calculateStatusOverlay(Qn::Online, states.hasVideo);
}

Qn::ResourceOverlayButton QnMediaResourceWidget::calculateOverlayButton(
    Qn::ResourceStatusOverlay statusOverlay) const
{
    if (!m_camera || !m_camera->resourcePool())
        return Qn::ResourceOverlayButton::Empty;

    const bool canChangeSettings = accessController()->hasPermissions(m_camera,
        Qn::SavePermission | Qn::WritePermission);

    switch (statusOverlay)
    {
        case Qn::IoModuleDisabledOverlay:
        {
            NX_ASSERT(m_ioLicenceStatusHelper, Q_FUNC_INFO,
                "Query I/O status overlay for resource widget which is not containing I/O module");

            if (m_ioLicenceStatusHelper && canChangeSettings)
            {
                switch (m_ioLicenceStatusHelper->status())
                {
                    case QnSingleCamLicenceStatusHelper::LicenseNotUsed:
                        return Qn::ResourceOverlayButton::IoEnable;

                    case QnSingleCamLicenceStatusHelper::LicenseOverflow:
                        return Qn::ResourceOverlayButton::MoreLicenses;
                    default:
                        break;
                }
            }

            return Qn::ResourceOverlayButton::Empty;
        }

        case Qn::OfflineOverlay:
        {
            return menu()->canTrigger(action::CameraDiagnosticsAction, m_camera)
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

void QnMediaResourceWidget::at_resource_propertyChanged(const QnResourcePtr &resource, const QString &key)
{
    Q_UNUSED(resource);
    if (key == QnMediaResource::customAspectRatioKey())
        updateCustomAspectRatio();
    else if (key == Qn::CAMERA_CAPABILITIES_PARAM_NAME)
        ensureTwoWayAudioWidget();
    else if (key == Qn::kCombinedSensorsDescriptionParamName)
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
    bool isLive = m_display->camDisplay()->isRealTimeSource();

    if (!isLive)
    {
        titleBar()->rightButtonsBar()->setButtonsChecked(Qn::PtzButton, false);
        suspendHomePtzController();
    }
    else
    {
        resumeHomePtzController();
    }

    updateCompositeOverlayMode();
}

void QnMediaResourceWidget::at_screenshotButton_clicked()
{
    menu()->trigger(action::TakeScreenshotAction, this);
}

void QnMediaResourceWidget::at_ptzButton_toggled(bool checked)
{
    bool ptzEnabled =
        checked && (m_camera && (m_camera->getPtzCapabilities() & Ptz::ContinuousPtzCapabilities));

    setOption(ControlPtz, ptzEnabled);
    setOption(DisplayCrosshair, ptzEnabled);
    if (checked)
    {
        titleBar()->rightButtonsBar()->setButtonsChecked(Qn::MotionSearchButton | Qn::ZoomWindowButton, false);
        action(action::JumpToLiveAction)->trigger(); // TODO: #Elric evil hack! Won't work if SYNC is off and this item is not selected?
    }
}

void QnMediaResourceWidget::at_fishEyeButton_toggled(bool checked)
{
    QnItemDewarpingParams params = item()->dewarpingParams();
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
        ptzController()->continuousMove(QVector3D(0, 0, 0));
        suspendHomePtzController();
    }

    updateButtonsVisibility();
}

void QnMediaResourceWidget::at_histogramButton_toggled(bool checked)
{
    ImageCorrectionParams params = item()->imageEnhancement();
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
        m_ptzController->absoluteMove(Qn::LogicalPtzCoordinateSpace,
            QnFisheyePtzController::positionFromRect(m_dewarpingParams, zoomRect()), 2.0);
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
    using nx::client::desktop::EntropixImageEnhancer;

    m_entropixEnhancer.reset(new EntropixImageEnhancer(m_camera));
    connect(m_entropixEnhancer, &EntropixImageEnhancer::cameraScreenshotReady,
        this, &QnMediaResourceWidget::at_entropixImageLoaded);
    connect(m_entropixEnhancer, &EntropixImageEnhancer::progressChanged, this,
        [this](int progress)
        {
            m_entropixProgress = progress < 100 ? progress : -1;
        });

    m_entropixEnhancer->requestScreenshot(m_display->currentTimeUSec() / 1000, zoomRect());
}

void QnMediaResourceWidget::at_entropixImageLoaded(const QImage& image)
{
    m_entropixEnhancedImage = image;
}

void QnMediaResourceWidget::updateDewarpingParams()
{
    if (m_dewarpingParams == m_resource->getDewarpingParams())
        return;

    m_dewarpingParams = m_resource->getDewarpingParams();
    emit dewarpingParamsChanged();
}

void QnMediaResourceWidget::updateFisheye()
{
    QnItemDewarpingParams itemParams = item()->dewarpingParams();

    // Zoom windows have no own "dewarping" button, so counting it always pressed.
    bool enabled = isZoomWindow() || itemParams.enabled;

    bool fisheyeEnabled = enabled && m_dewarpingParams.enabled;

    setOption(ControlPtz, fisheyeEnabled && zoomRect().isEmpty());
    setOption(DisplayCrosshair, fisheyeEnabled && zoomRect().isEmpty());
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

    if (titleBar()->rightButtonsBar()->visibleButtons() & Qn::PtzButton)
        at_ptzButton_toggled(titleBar()->rightButtonsBar()->checkedButtons() & Qn::PtzButton); // TODO: #Elric doesn't belong here, hack
}

void QnMediaResourceWidget::updateCustomAspectRatio()
{
    if (!m_display || !m_resource)
        return;

    m_display->camDisplay()->setOverridenAspectRatio(m_resource->customAspectRatio());
}

QnMediaResourceWidget::ResourceStates QnMediaResourceWidget::getResourceStates() const
{
    const auto camDisplay = (m_display ? m_display->camDisplay() : nullptr);
    const auto resource = (m_display ? m_display->resource() : QnResourcePtr());

    ResourceStates result;
    result.isRealTimeSource = (camDisplay ? camDisplay->isRealTimeSource() : false);
    result.isOffline = (result.isRealTimeSource && (!resource || (resource->getStatus() == Qn::Offline)));
    result.isUnauthorized = (result.isRealTimeSource && (resource && (resource->getStatus() == Qn::Unauthorized)));
    result.hasVideo = hasVideo();

    return result;
}

void QnMediaResourceWidget::updateIoModuleVisibility(bool animate)
{
    NX_ASSERT(m_camera && m_camera->hasFlags(Qn::io_module) && m_ioLicenceStatusHelper,
        Q_FUNC_INFO, "updateIoModuleVisibility should be called only for I/O modules");

    if (!m_camera || !m_camera->hasFlags(Qn::io_module) || !m_ioLicenceStatusHelper)
        return;

    const QnImageButtonWidget * const button = titleBar()->rightButtonsBar()->button(Qn::IoModuleButton);
    const bool ioBtnChecked = (button && button->isChecked());
    const bool onlyIoData = !hasVideo();
    const bool correctLicenceStatus = (m_ioLicenceStatusHelper->status() == QnSingleCamLicenceStatusHelper::LicenseUsed);

    /// TODO: #ynikitenkov It needs to refactor error\status overlays totally!

    m_ioCouldBeShown = ((ioBtnChecked || onlyIoData) && correctLicenceStatus);
    const ResourceStates states = getResourceStates();
    const bool correctState = (!states.isOffline && !states.isUnauthorized && states.isRealTimeSource);
    const OverlayVisibility visibility = (m_ioCouldBeShown && correctState ? Visible : Invisible);
    setOverlayWidgetVisibility(m_ioModuleOverlayWidget, visibility, animate);
    updateOverlayWidgetsVisibility(animate);

    updateStatusOverlay(animate);
    updateOverlayButton();
}

void QnMediaResourceWidget::processDiagnosticsRequest()
{
    context()->statisticsModule()->registerClick(lit("resource_status_overlay_diagnostics"));

    if (m_camera)
        menu()->trigger(action::CameraDiagnosticsAction, m_camera);
}

void QnMediaResourceWidget::processIoEnableRequest()
{
    context()->statisticsModule()->registerClick(lit("resource_status_overlay_io_enable"));

    NX_ASSERT(m_ioLicenceStatusHelper, Q_FUNC_INFO,
        "at_statusOverlayWidget_ioEnableRequested could not be processed for non-I/O modules");

    if (!m_ioLicenceStatusHelper)
        return;

    const auto licenceStatus = m_ioLicenceStatusHelper->status();
    if (licenceStatus != QnSingleCamLicenceStatusHelper::LicenseNotUsed)
        return;

    qnResourcesChangesManager->saveCamera(m_camera,
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
    if (!m_camera)
        return;

    int selectedTab = Qn::GeneralSettingsTab;
    menu()->trigger(action::CameraSettingsAction, action::Parameters(m_camera)
        .withArgument(Qn::FocusTabRole, selectedTab));
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
    if (!m_compositeOverlay || m_camera.isNull())
        return;

    const bool isLive = (m_display && m_display->camDisplay()
        ? m_display->camDisplay()->isRealTimeSource() : false);

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
        if (auto button = qobject_cast<QnSoftwareTriggerButton*>(m_triggersContainer->item(i)))
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
        ? m_renderer->getTimestampOfNextFrameToRender(channel)
        : display()->camDisplay()->getCurrentTime();

    return timestampUsec;
}

qint64 QnMediaResourceWidget::getUtcCurrentTimeMs() const
{
    qint64 datetimeUsec = getUtcCurrentTimeUsec();
    if (datetimeUsec == DATETIME_NOW)
        return qnSyncTime->currentMSecsSinceEpoch();
    else if (datetimeUsec == AV_NOPTS_VALUE)
        return 0;
    else
        return datetimeUsec / 1000;
}

qint64 QnMediaResourceWidget::getDisplayTimeUsec() const
{
    qint64 result = getUtcCurrentTimeUsec();
    if (!isSpecialDateTimeValueUsec(result))
        result += context()->instance<QnWorkbenchServerTimeWatcher>()->displayOffset(m_resource) * 1000ll;
    return result;
}

QnScrollableTextItemsWidget* QnMediaResourceWidget::bookmarksContainer()
{
    return m_bookmarksContainer;
}

QVector<QColor> QnMediaResourceWidget::motionSensitivityColors() const
{
    return m_motionSensitivityColors;
}

void QnMediaResourceWidget::setMotionSensitivityColors(const QVector<QColor>& value)
{
    m_motionSensitivityColors = value;
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
    titleBar()->rightButtonsBar()->setButtonsChecked(Qn::MotionSearchButton, enabled);

    if (enabled)
    {
        titleBar()->rightButtonsBar()->setButtonsChecked(
            Qn::PtzButton | Qn::FishEyeButton | Qn::ZoomWindowButton, false);
    }

    setOption(WindowResizingForbidden, enabled);

    emit motionSearchModeEnabled(enabled);
}

QnSpeedRange QnMediaResourceWidget::speedRange() const
{
    static constexpr qreal kUnitSpeed = 1.0;
    static constexpr qreal kZeroSpeed = 0.0;

    if (!m_display || !m_display->archiveReader())
        return QnSpeedRange(kZeroSpeed, kZeroSpeed);

    if (!hasVideo())
        return QnSpeedRange(kUnitSpeed, kZeroSpeed);

    const qreal backward = m_display->archiveReader()->isNegativeSpeedSupported()
        ? availableSpeedRange().backward
        : kZeroSpeed;

    return QnSpeedRange(availableSpeedRange().forward, backward);
}

const QnSpeedRange& QnMediaResourceWidget::availableSpeedRange()
{
    static const QnSpeedRange kAvailableSpeedRange(kMaxForwardSpeed, kMaxBackwardSpeed);
    return kAvailableSpeedRange;
}

/*
* Soft Triggers
*/

QnMediaResourceWidget::SoftwareTrigger* QnMediaResourceWidget::createTriggerIfRelevant(
    const vms::event::RulePtr& rule)
{
    NX_ASSERT(!m_softwareTriggers.contains(rule->id()));

    if (!isRelevantTriggerRule(rule))
        return nullptr;

    const SoftwareTriggerInfo info({
        rule->eventParams().inputPortId,
        rule->eventParams().caption,
        rule->eventParams().description,
        rule->isActionProlonged() });

    std::function<void()> clientSideHandler;

    if (rule->actionType() == nx::vms::event::bookmarkAction)
    {
        clientSideHandler =
            [this] { action(action::BookmarksModeAction)->setChecked(true); };
    }

    const auto button = new QnSoftwareTriggerButton(this);
    configureTriggerButton(button, info, clientSideHandler);

    connect(button, &QnSoftwareTriggerButton::isLiveChanged, this,
        [this, button, rule]()
        {
            updateTriggerAvailability(rule);
        });

    // TODO: #vkutin #3.1 For now rule buttons are NOT sorted. Implement sorting by UUID later.
    const auto overlayItemId = m_triggersContainer->insertItem(0, button);

    auto& trigger = m_softwareTriggers[rule->id()];
    trigger = SoftwareTrigger({ info, overlayItemId });
    return &trigger;
}

bool QnMediaResourceWidget::isRelevantTriggerRule(const vms::event::RulePtr& rule) const
{
    if (rule->isDisabled() || rule->eventType() != vms::event::softwareTriggerEvent)
        return false;

    const auto resourceId = m_resource->toResource()->getId();
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
    QnSoftwareTriggerButton* button,
    const SoftwareTriggerInfo& info,
    bool enabledBySchedule)
{
    if (!button)
    {
        NX_EXPECT(false, "Trigger button is null");
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

void QnMediaResourceWidget::configureTriggerButton(QnSoftwareTriggerButton* button,
    const SoftwareTriggerInfo& info, std::function<void()> clientSideHandler)
{
    NX_EXPECT(button);

    button->setIcon(info.icon);
    button->setProlonged(info.prolonged);
    updateTriggerButtonTooltip(button, info, true);

    const auto resultHandler =
        [button = QPointer<QnSoftwareTriggerButton>(button)](bool success, qint64 requestId)
        {
            if (!button || button->property(
                kTriggerRequestIdProperty).value<rest::Handle>() != requestId)
            {
                return;
            }

            button->setEnabled(true);

            button->setState(success
                ? QnSoftwareTriggerButton::State::Success
                : QnSoftwareTriggerButton::State::Failure);
        };

    if (info.prolonged)
    {
        connect(button, &QnSoftwareTriggerButton::pressed, this,
            [this, button, resultHandler, clientSideHandler, id = info.triggerId]()
            {
                if (!button->isLive())
                    return;

                const auto requestId = invokeTrigger(id, resultHandler, vms::event::EventState::active);
                const bool success = requestId != rest::Handle();
                button->setProperty(kTriggerRequestIdProperty, requestId);
                button->setState(success
                    ? QnSoftwareTriggerButton::State::Waiting
                    : QnSoftwareTriggerButton::State::Failure);

                if (success && clientSideHandler)
                    clientSideHandler();
            });

        connect(button, &QnSoftwareTriggerButton::released, this,
            [this, button, resultHandler, id = info.triggerId]()
            {
                if (!button->isLive())
                    return;

                /* In case of activation error don't try to deactivate: */
                if (button->state() == QnSoftwareTriggerButton::State::Failure)
                    return;

                const auto requestId = invokeTrigger(id, resultHandler, vms::event::EventState::inactive);
                const bool success = requestId != rest::Handle();
                button->setProperty(kTriggerRequestIdProperty, requestId);
                button->setState(success
                    ? QnSoftwareTriggerButton::State::Default
                    : QnSoftwareTriggerButton::State::Failure);
            });
    }
    else
    {
        connect(button, &QnSoftwareTriggerButton::clicked, this,
            [this, button, resultHandler, clientSideHandler, id = info.triggerId]()
            {
                if (!button->isLive())
                    return;

                const auto requestId = invokeTrigger(id, resultHandler);
                const bool success = requestId != rest::Handle();
                button->setProperty(kTriggerRequestIdProperty, requestId);
                button->setEnabled(!success);
                button->setState(success
                    ? QnSoftwareTriggerButton::State::Waiting
                    : QnSoftwareTriggerButton::State::Failure);

                if (success && clientSideHandler)
                    clientSideHandler();
            });
    }

    // Go-to-live handler.
    connect(button, &QnSoftwareTriggerButton::clicked, this,
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
    for (const auto& trigger: m_softwareTriggers)
        m_triggersContainer->deleteItem(trigger.overlayItemId);

    /* Clear triggers information: */
    m_softwareTriggers.clear();

    if (!accessController()->hasGlobalPermission(Qn::GlobalUserInputPermission))
        return;

    /* Create new relevant triggers: */
    for (const auto& rule: commonModule()->eventRuleManager()->rules())
        createTriggerIfRelevant(rule); //< creates a trigger only if the rule is relevant

    updateTriggersAvailability();
}

void QnMediaResourceWidget::at_eventRuleRemoved(const QnUuid& id)
{
    const auto iter = m_softwareTriggers.find(id);
    if (iter == m_softwareTriggers.end())
        return;

    m_triggersContainer->deleteItem(iter->overlayItemId);
    m_softwareTriggers.erase(iter);
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
    const auto iter = m_softwareTriggers.find(rule->id());
    if (iter == m_softwareTriggers.end())
    {
        /* Create trigger if the rule is relevant: */
        createTriggerIfRelevant(rule);
    }
    else
    {
        /* Delete trigger: */
        at_eventRuleRemoved(rule->id());

        /* Recreate trigger if the rule is still relevant: */
        createTriggerIfRelevant(rule);
    }

    updateTriggerAvailability(rule);
};

rest::Handle QnMediaResourceWidget::invokeTrigger(
    const QString& id,
    std::function<void(bool, rest::Handle)> resultHandler,
    vms::event::EventState toggleState)
{
    if (!accessController()->hasGlobalPermission(Qn::GlobalUserInputPermission))
        return rest::Handle();

    const auto responseHandler =
        [this, resultHandler, id](bool success, rest::Handle handle, const QnJsonRestResult& result)
        {
            Q_UNUSED(handle);
            success = success && result.error == QnRestResult::NoError;

            if (!success)
            {
                NX_LOG(tr("Failed to invoke trigger %1 (%2)")
                    .arg(id).arg(result.errorString), cl_logERROR);
            }

            if (resultHandler)
                resultHandler(success, handle);
        };

    return commonModule()->currentServer()->restConnection()->softwareTriggerCommand(
        m_resource->toResource()->getId(), id, toggleState,
        responseHandler, QThread::currentThread());
}
