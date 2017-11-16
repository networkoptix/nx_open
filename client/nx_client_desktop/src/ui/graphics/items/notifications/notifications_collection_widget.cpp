#include "notifications_collection_widget.h"

#include <QtGui/QDesktopServices>

#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QStyle>

#include <nx/utils/log/log.h>

#include <nx/vms/event/strings_helper.h>

#include <business/business_resource_validation.h>

#include <camera/single_thumbnail_loader.h>

#include <core/resource/resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/resource_display_info.h>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <ui/animation/opacity_animator.h>
#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/common/geometry.h>
#include <ui/common/notification_levels.h>
#include <ui/graphics/items/generic/graphics_message_box.h>

// TODO: #gdm think about moving out pages enums
#include <ui/dialogs/resource_properties/user_settings_dialog.h>
#include <ui/dialogs/resource_properties/server_settings_dialog.h>

#include <ui/graphics/items/generic/blinking_image_widget.h>
#include <ui/graphics/items/notifications/notification_widget.h>
#include <ui/graphics/items/notifications/legacy_notification_list_widget.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/help/business_help.h>
#include <ui/style/skin.h>
#include <ui/style/globals.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/style/software_trigger_pixmaps.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/handlers/workbench_notifications_handler.h>
#include <ui/workbench/watchers/default_password_cameras_watcher.h>
#include <nx/client/desktop/ui/actions/action.h>

#include <health/system_health_helper.h>

#include <utils/common/delayed.h>
#include <utils/common/delete_later.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/util.h> /* For random. */
#include <utils/math/color_transformations.h>
#include <utils/camera/bookmark_helpers.h>
#include <nx/client/desktop/utils/server_notification_cache.h>
#include <utils/multi_image_provider.h>

#include <nx/fusion/model_functions.h>
#include <camera/camera_bookmarks_manager.h>
#include <core/resource/security_cam_resource.h>

using namespace nx;
using namespace nx::client::desktop;
using namespace nx::client::desktop::ui;

namespace {

static const QSize kDefaultThumbnailSize(224, 0);

/** We limit the maximal number of notification items to prevent crashes due
 * to reaching GDI resource limit. */
static const int kMaxNotificationItems = 128;
static const int kMultiThumbnailSpacing = 1;
static const int kMaxThumbnailCount = 5;

const char* kItemResourcePropertyName = "_qn_itemResource";
const char* kItemActionTypePropertyName = "_qn_itemActionType";
const char* kItemTimeStampPropertyName = "_qn_itemTimeStamp";

} //anonymous namespace

QnNotificationsCollectionWidget::QnNotificationsCollectionWidget(QGraphicsItem* parent, Qt::WindowFlags flags, QnWorkbenchContext* context) :
    base_type(parent, flags),
    QnWorkbenchContextAware(context),
    m_headerWidget(new GraphicsWidget(this)),
    m_helper(new vms::event::StringsHelper(commonModule()))
{
    int maxIconSize = QApplication::style()->pixelMetric(QStyle::PM_ToolBarIconSize, nullptr, nullptr);
    auto newButton = [this, maxIconSize](action::IDType actionId, int helpTopicId)
    {
        QnImageButtonWidget* button = new QnImageButtonWidget(m_headerWidget);
        button->setDefaultAction(action(actionId));
        button->setFixedSize(button->defaultAction()->icon().actualSize(QSize(maxIconSize, maxIconSize)));

        if (helpTopicId != Qn::Empty_Help)
            setHelpTopic(button, helpTopicId);

        connect(this->context(), &QnWorkbenchContext::userChanged, this, [this, button, actionId]
        {
            button->setVisible(this->menu()->canTrigger(actionId));
        });
        button->setVisible(this->menu()->canTrigger(actionId));

        const auto statAlias = lit("%1_%2").arg(lit("notifications_collection_widget"), QnLexical::serialized(actionId));
        this->context()->statisticsModule()->registerButton(statAlias, button);

        return button;
    };

    QGraphicsLinearLayout* controlsLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    controlsLayout->setSpacing(2.0);
    controlsLayout->setContentsMargins(2.0, 1.0, 2.0, 0.0);
    controlsLayout->addStretch();

    controlsLayout->addItem(newButton(action::OpenBusinessLogAction, Qn::MainWindow_Notifications_EventLog_Help));
    controlsLayout->addItem(newButton(action::BusinessEventsAction, -1));
    controlsLayout->addItem(newButton(action::PreferencesNotificationTabAction, -1));
    m_headerWidget->setLayout(controlsLayout);

    QGraphicsLinearLayout* layout = new QGraphicsLinearLayout(Qt::Vertical);
    layout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    layout->setSpacing(0.0);
    layout->addItem(m_headerWidget);

    m_list = new QnNotificationListWidget(this);
    layout->addItem(m_list);

    connect(m_list, &QnNotificationListWidget::itemRemoved,         this, &QnNotificationsCollectionWidget::at_list_itemRemoved);
    connect(m_list, &QnNotificationListWidget::visibleSizeChanged,  this, &QnNotificationsCollectionWidget::visibleSizeChanged);
    connect(m_list, &QnNotificationListWidget::sizeHintChanged,     this, &QnNotificationsCollectionWidget::sizeHintChanged);

    setLayout(layout);

    QnWorkbenchNotificationsHandler* handler = this->context()->instance<QnWorkbenchNotificationsHandler>();
    connect(handler,    &QnWorkbenchNotificationsHandler::notificationAdded,        this,   &QnNotificationsCollectionWidget::showEventAction);
    connect(handler,    &QnWorkbenchNotificationsHandler::notificationRemoved,      this,   &QnNotificationsCollectionWidget::hideEventAction);
    connect(handler,    &QnWorkbenchNotificationsHandler::systemHealthEventAdded,   this,   &QnNotificationsCollectionWidget::showSystemHealthMessage);
    connect(handler,    &QnWorkbenchNotificationsHandler::systemHealthEventRemoved, this,   &QnNotificationsCollectionWidget::hideSystemHealthMessage);
    connect(handler,    &QnWorkbenchNotificationsHandler::cleared,                  this,   &QnNotificationsCollectionWidget::hideAll);

    connect(this->context()->instance<ServerNotificationCache>(),
        &ServerNotificationCache::fileDownloaded,
        this,
        [this](const QString& filename, ServerFileCache::OperationResult status)
        {
            if (status != ServerFileCache::OperationResult::ok)
                return;
            at_notificationCache_fileDownloaded(filename);
        });

    const auto defaultPasswordWatcher = context->instance<DefaultPasswordCamerasWatcher>();
    const auto updateDefaultCameraPasswordNotification =
        [this, defaultPasswordWatcher]()
        {
            if (!defaultPasswordWatcher->notificationIsVisible())
            {
                cleanUpItem(m_currentDefaultPasswordChangeWidget);
                m_currentDefaultPasswordChangeWidget = nullptr;
            }
            else if (accessController()->hasGlobalPermission(Qn::GlobalAdminPermission))
            {
                NX_EXPECT(!m_currentDefaultPasswordChangeWidget, "Can't show this popup twice!");
                const auto parametersGetter =
                    [defaultPasswordWatcher]()
                    {
                        return action::Parameters(
                            defaultPasswordWatcher->camerasWithDefaultPassword())
                            .withArgument(Qn::ShowSingleCameraRole, true);
                    };

                m_currentDefaultPasswordChangeWidget = addCustomPopup(
                    action::ChangeDefaultCameraPasswordAction,
                    parametersGetter,
                    QnNotificationLevel::Value::ImportantNotification,
                    tr("Set Passwords"),
                    false);
            }
        };

    updateDefaultCameraPasswordNotification();
    connect(defaultPasswordWatcher, &DefaultPasswordCamerasWatcher::notificationIsVisibleChanged,
        this, updateDefaultCameraPasswordNotification);
    connect(context, &QnWorkbenchContext::userChanged,
        this, updateDefaultCameraPasswordNotification);
}

QnNotificationsCollectionWidget::~QnNotificationsCollectionWidget()
{
}

QRectF QnNotificationsCollectionWidget::headerGeometry() const
{
    return m_headerWidget->geometry();
}

QRectF QnNotificationsCollectionWidget::visibleGeometry() const
{
    return m_headerWidget->geometry().adjusted(0, 0, 0, m_list->visibleSize().height());
}

void QnNotificationsCollectionWidget::setToolTipsEnclosingRect(const QRectF& rect)
{
    m_list->setToolTipsEnclosingRect(mapRectToItem(m_list, rect));
}

QnBlinkingImageButtonWidget* QnNotificationsCollectionWidget::blinker() const
{
    return m_blinker.data();
}

void QnNotificationsCollectionWidget::setBlinker(QnBlinkingImageButtonWidget* blinker)
{
    if (m_blinker)
        m_list->disconnect(m_blinker);

    m_blinker = blinker;

    if (m_blinker)
    {
        connect(m_list, &QnNotificationListWidget::itemCountChanged,         this, &QnNotificationsCollectionWidget::updateBlinker);
        connect(m_list, &QnNotificationListWidget::notificationLevelChanged, this, &QnNotificationsCollectionWidget::updateBlinker);
        updateBlinker();
    }
}

void QnNotificationsCollectionWidget::loadThumbnailForItem(
    QnNotificationWidget* item,
    const QnVirtualCameraResourcePtr& camera,
    qint64 msecSinceEpoch)
{
    if (!camera || !camera->hasVideo(nullptr))
        return;

    NX_ASSERT(accessController()->hasPermissions(camera, Qn::ViewContentPermission));

    const auto requiredPermission = msecSinceEpoch < 0
        ? Qn::ViewLivePermission
        : Qn::ViewFootagePermission;

    if (accessController()->hasPermissions(camera, requiredPermission))
        return;

    QnSingleThumbnailLoader* loader = new QnSingleThumbnailLoader(
        camera,
        msecSinceEpoch,
        QnThumbnailRequestData::kDefaultRotation,
        kDefaultThumbnailSize,
        QnThumbnailRequestData::JpgFormat,
        QnThumbnailRequestData::AspectRatio::AutoAspectRatio,
        QnThumbnailRequestData::RoundMethod::KeyFrameAfterMethod,
        item);

    item->setImageProvider(loader);
}

void QnNotificationsCollectionWidget::loadThumbnailForItem(
    QnNotificationWidget* item,
    const QnVirtualCameraResourceList& cameraList,
    qint64 msecSinceEpoch)
{
    const auto requiredPermission = msecSinceEpoch < 0
        ? Qn::ViewLivePermission
        : Qn::ViewFootagePermission;

    QnMultiImageProvider::Providers providers;
    for (const auto& camera: cameraList)
    {
        NX_ASSERT(accessController()->hasPermissions(camera, Qn::ViewContentPermission));
        if (accessController()->hasPermissions(camera, requiredPermission))
            continue;

        std::unique_ptr<QnImageProvider> provider(new QnSingleThumbnailLoader(
            camera,
            msecSinceEpoch,
            QnThumbnailRequestData::kDefaultRotation,
            kDefaultThumbnailSize,
            QnThumbnailRequestData::JpgFormat));

        providers.push_back(std::move(provider));
    }

    if (providers.empty())
        return;

    item->setImageProvider(new QnMultiImageProvider(std::move(providers), Qt::Vertical, kMultiThumbnailSpacing, item));
}

void QnNotificationsCollectionWidget::addAcknoledgeButtonIfNeeded(
    QnNotificationWidget* widget,
    const QnVirtualCameraResourcePtr& camera,
    const nx::vms::event::AbstractActionPtr& action)
{
    if (action->actionType() != vms::event::ActionType::showPopupAction)
    {
        NX_ASSERT(false, "Invalid action type.");
        return;
    }

    if (!context()->accessController()->hasGlobalPermission(Qn::GlobalManageBookmarksPermission))
        return;

    const auto actionParams = action->getParams();
    if (!actionParams.requireConfirmation(action->getRuntimeParams().eventType))
        return;

    if (!camera)
        return;

    NX_EXPECT(menu()->canTrigger(action::AcknowledgeEventAction, camera));
    if (!menu()->canTrigger(action::AcknowledgeEventAction, camera))
        return;

    widget->setCloseButtonAvailable(false);
    widget->setNotificationLevel(QnNotificationLevel::Value::CriticalNotification);
    widget->addTextButton(qnSkin->icon("buttons/acknowledge.png"), tr("Acknowledge"),
        [this, camera, action]()
        {
            auto& actionParams = action->getParams();
            actionParams.recordBeforeMs = 0;

            static const auto kDurationMs = std::chrono::milliseconds(std::chrono::seconds(10));
            actionParams.durationMs = kDurationMs.count();

            // Action will trigger additional event loop, which will cause problems here.
            executeDelayedParented(
                [this, camera, action]
                {
                    action::Parameters params(camera);
                    params.setArgument(Qn::ActionDataRole, action);
                    menu()->trigger(action::AcknowledgeEventAction, params);
                }, kDefaultDelay, this);
        });

    m_customPopupItems.insert(action->getParams().actionId, widget);
}

QnNotificationWidget* QnNotificationsCollectionWidget::addCustomPopup(
    action::IDType actionId,
    const ParametersGetter& parametersGetter,
    QnNotificationLevel::Value notificationLevel,
    const QString& buttonText,
    bool closeable)
{
    if (m_list->itemCount() >= kMaxNotificationItems)
        return nullptr;

    const auto action = menu()->action(actionId);
    if (!action)
        return nullptr;

    auto item = new QnNotificationWidget(m_list);
    item->setText(action->text());
    item->setNotificationLevel(notificationLevel);
    item->setCloseButtonAvailable(closeable);

    if (!buttonText.isEmpty())
    {
        item->addTextButton(action->icon(),
            buttonText,
            [this, actionId, parametersGetter]()
            {
                const auto triggerAction =
                    [this, actionId, parametersGetter]
                    {
                        menu()->trigger(actionId, parametersGetter());
                    };

                // Action will trigger additional event loop, which will cause problems here.
                executeDelayedParented(triggerAction, kDefaultDelay, this);
            });
    }

    item->addActionButton(qnSkin->icon("events/alert.png"), actionId);

    m_list->addItem(item, !closeable);
    return item;
}

void QnNotificationsCollectionWidget::showEventAction(const vms::event::AbstractActionPtr& action)
{
    if (m_list->itemCount() >= kMaxNotificationItems)
        return; /* Just drop the notification if we already have too many of them in queue. */

    vms::event::EventParameters params = action->getRuntimeParams();
    NX_ASSERT(QnBusiness::hasAccessToSource(params, context()->user())); //< Checked already.

    vms::event::EventType eventType = params.eventType;
    QnUuid ruleId = action->getRuleId();
    QString title = m_helper->eventAtResource(params, qnSettings->extraInfoInTree());
    qint64 timestampMs = params.eventTimestampUsec / 1000;

    auto alarmCameras = resourcePool()->getResources<QnVirtualCameraResource>(action->getResources());
    if (action->getParams().useSource)
        alarmCameras << resourcePool()->getResources<QnVirtualCameraResource>(action->getSourceResources());
    alarmCameras = accessController()->filtered(alarmCameras, Qn::ViewContentPermission);
    alarmCameras = alarmCameras.toSet().toList();

    QnResourcePtr resource = resourcePool()->getResourceById(params.eventResourceId);
    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    const auto server = resource.dynamicCast<QnMediaServerResource>();
    const bool hasViewPermission = resource && accessController()->hasPermissions(resource,
        Qn::ViewContentPermission);

    if (action->actionType() == vms::event::showOnAlarmLayoutAction)
    {
        if (alarmCameras.isEmpty())
            return;

        auto findItemPredicate =
            [timestampMs](QnNotificationWidget* item)
            {
                return item->property(kItemActionTypePropertyName) == vms::event::showOnAlarmLayoutAction
                    && item->property(kItemTimeStampPropertyName) == timestampMs;
            };

        if (findItem(ruleId, findItemPredicate))
            return; /* Show 'Alarm Layout' notifications only once for each event of one rule. */

        title = tr("Alarm: %1").arg(title);
    }

    QStringList tooltip = m_helper->eventDescription(action,
        vms::event::AggregationInfo(), qnSettings->extraInfoInTree());

    // TODO: #GDM #3.1 move this code to ::eventDetails()
    if (eventType == vms::event::licenseIssueEvent
        && params.reasonCode == vms::event::EventReason::licenseRemoved)
    {
        QStringList disabledCameras;
        for (const QString& stringId: params.description.split(L';'))
        {
            QnUuid id = QnUuid::fromStringSafe(stringId);
            NX_ASSERT(!id.isNull());
            if (auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(id))
            {
                if (accessController()->hasPermissions(camera, Qn::ViewContentPermission))
                    tooltip << QnResourceDisplayInfo(camera).toString(qnSettings->extraInfoInTree());
            }
        }
    }

    QnNotificationWidget* item = new QnNotificationWidget(m_list);
    item->setText(title);
    item->setTooltipText(tooltip.join(L'\n'));
    item->setNotificationLevel(QnNotificationLevel::valueOf(action));
    item->setProperty(kItemResourcePropertyName,   QVariant::fromValue<QnResourcePtr>(resource));
    item->setProperty(kItemActionTypePropertyName, action->actionType());
    item->setProperty(kItemTimeStampPropertyName,  timestampMs);
    setHelpTopic(item, QnBusiness::eventHelpId(eventType));

    switch (action->actionType())
    {
        // TODO: #GDM not the best place for it.
        case vms::event::playSoundAction:
        {
            QString soundUrl = action->getParams().url;
            m_itemsByLoadingSound.insert(soundUrl, item);
            context()->instance<ServerNotificationCache>()->downloadFile(soundUrl);
            break;
        }
        default:
            break;
    };

    QIcon icon = iconForAction(action);

    if (action->actionType() == vms::event::showOnAlarmLayoutAction)
    {
        item->addActionButton(
            icon,
            action::OpenInAlarmLayoutAction,
            alarmCameras);

        loadThumbnailForItem(item, alarmCameras.mid(0, kMaxThumbnailCount));
    }
    else
    {
        switch (eventType)
        {
            case vms::event::cameraMotionEvent:
            case vms::event::softwareTriggerEvent:
            case vms::event::analyticsSdkEvent:
            {
                NX_ASSERT(hasViewPermission);
                item->addActionButton(
                    icon,
                    action::OpenInNewTabAction,
                    action::Parameters(camera).withArgument(Qn::ItemTimeRole, timestampMs));

                loadThumbnailForItem(item, camera, timestampMs);
                addAcknoledgeButtonIfNeeded(item, camera, action);
                break;
            }

            case vms::event::cameraInputEvent:
            {
                NX_ASSERT(hasViewPermission);
                item->addActionButton(
                    icon,
                    action::OpenInNewTabAction,
                    camera);

                loadThumbnailForItem(item, camera);
                addAcknoledgeButtonIfNeeded(item, camera, action);
                break;
            }

            case vms::event::cameraDisconnectEvent:
            case vms::event::networkIssueEvent:
            {
                NX_ASSERT(hasViewPermission);
                item->addActionButton(
                    icon,
                    action::CameraSettingsAction,
                    camera);

                loadThumbnailForItem(item, camera);
                addAcknoledgeButtonIfNeeded(item, camera, action);
                break;
            }

            case vms::event::storageFailureEvent:
            case vms::event::backupFinishedEvent:
            case vms::event::serverStartEvent:
            case vms::event::serverFailureEvent:
            {
                NX_ASSERT(hasViewPermission);
                item->addActionButton(
                    icon,
                    action::ServerSettingsAction,
                    server);
                break;
            }

            case vms::event::cameraIpConflictEvent:
            {
                QString webPageAddress = params.caption;
                item->addActionButton(
                    icon,
                    action::BrowseUrlAction,
                    {Qn::UrlRole, webPageAddress});
                break;
            }

            case vms::event::serverConflictEvent:
            {
                item->addActionButton(icon);
                break;
            }

            case vms::event::licenseIssueEvent:
            {
                item->addActionButton(
                    icon,
                    action::PreferencesLicensesTabAction);
                break;
            }

            case vms::event::userDefinedEvent:
            {
                auto sourceCameras = resourcePool()->getResources<QnVirtualCameraResource>(params.metadata.cameraRefs);
                sourceCameras = accessController()->filtered(sourceCameras, Qn::ViewFootagePermission);
                if (!sourceCameras.isEmpty())
                {
                    item->addActionButton(
                        icon,
                        action::OpenInNewTabAction,
                        action::Parameters(sourceCameras).withArgument(Qn::ItemTimeRole, timestampMs));

                    loadThumbnailForItem(item, sourceCameras.mid(0, kMaxThumbnailCount), timestampMs);
                    addAcknoledgeButtonIfNeeded(item, sourceCameras.first(), action);
                }
                break;
            }

            default:
                break;
        }
    }

    m_itemsByEventRuleId.insert(ruleId, item);

    connect(item, &QnNotificationWidget::buttonClicked, this,
        [this](const QString& alias)
        {
            context()->statisticsModule()->registerClick(alias);
        });

    /* We use Qt::QueuedConnection as our handler may start the event loop. */
    connect(item, &QnNotificationWidget::actionTriggered, this,
        &QnNotificationsCollectionWidget::at_item_actionTriggered, Qt::QueuedConnection);

    const bool isPlaySoundAction = action->actionType() == vms::event::playSoundAction;

    const bool isLockedItem = isPlaySoundAction || !item->isCloseButtonAvailable();
    m_list->addItem(item, isLockedItem);
}

void QnNotificationsCollectionWidget::hideEventAction(const vms::event::AbstractActionPtr& action)
{
    const auto customPopupId = action->getParams().actionId;
    const auto it = m_customPopupItems.find(customPopupId);
    if (it != m_customPopupItems.end())
    {
        cleanUpItem(it.value());
        return;
    }

    QnUuid ruleId = action->getRuleId();

    if (action->actionType() == vms::event::playSoundAction)
    {
        for (QnNotificationWidget* item: m_itemsByEventRuleId.values(ruleId))
            cleanUpItem(item);
    }

    QnResourcePtr resource = resourcePool()->getResourceById(action->getRuntimeParams().eventResourceId);
    if (!resource)
        return;

    QnNotificationWidget* item = findItem(ruleId, resource);
    if (!item)
        return;

    cleanUpItem(item);
}

QnNotificationWidget* QnNotificationsCollectionWidget::findItem(QnSystemHealth::MessageType message, const QnResourcePtr& resource)
{
    for (QnNotificationWidget* item: m_itemsByMessageType.values(message))
        if (resource == item->property(kItemResourcePropertyName).value<QnResourcePtr>())
            return item;

    return nullptr;
}

QnNotificationWidget* QnNotificationsCollectionWidget::findItem(const QnUuid& RuleId, const QnResourcePtr& resource)
{
    for (QnNotificationWidget* item: m_itemsByEventRuleId.values(RuleId))
        if (resource == item->property(kItemResourcePropertyName).value<QnResourcePtr>())
            return item;

    return nullptr;
}

QnNotificationWidget* QnNotificationsCollectionWidget::findItem(const QnUuid& RuleId, std::function<bool(QnNotificationWidget* item)> filter)
{
    for (QnNotificationWidget* item : m_itemsByEventRuleId.values(RuleId))
        if (filter(item))
            return item;

    return nullptr;
}

QIcon QnNotificationsCollectionWidget::iconForAction(const vms::event::AbstractActionPtr& action) const
{
    if (action->actionType() == vms::event::playSoundAction)
        return qnSkin->icon("events/sound.png");

    if (action->actionType() == vms::event::showOnAlarmLayoutAction)
        return qnSkin->icon("events/alarm.png");

    auto params = action->getRuntimeParams();
    vms::event::EventType eventType = params.eventType;

    if (eventType >= vms::event::userDefinedEvent)
    {
        QnVirtualCameraResourceList camList = resourcePool()->getResources<QnVirtualCameraResource>(params.metadata.cameraRefs);
        if (!camList.isEmpty())
            return qnResIconCache->icon(QnResourceIconCache::Camera);

        return QIcon();
    }

    switch (eventType)
    {
        case vms::event::cameraMotionEvent:
        case vms::event::cameraInputEvent:
        case vms::event::cameraDisconnectEvent:
        case vms::event::cameraIpConflictEvent:
        case vms::event::networkIssueEvent:
        case vms::event::analyticsSdkEvent:
        {
            auto resource = resourcePool()->getResourceById(params.eventResourceId);
            return resource
                ? qnResIconCache->icon(resource)
                : qnResIconCache->icon(QnResourceIconCache::Camera);
        }

        case vms::event::softwareTriggerEvent:
        {
            return QnSoftwareTriggerPixmaps::colorizedPixmap(
                action->getRuntimeParams().description,
                palette().color(QPalette::WindowText));
        }

        case vms::event::storageFailureEvent:
            return qnSkin->icon("events/storage.png");

        case vms::event::serverStartEvent:
        case vms::event::serverFailureEvent:
        case vms::event::serverConflictEvent:
        case vms::event::backupFinishedEvent:
            return qnResIconCache->icon(QnResourceIconCache::Server);

        case vms::event::licenseIssueEvent:
            return qnSkin->icon("events/license.png");

        default:
            return QIcon();
    }
}


void QnNotificationsCollectionWidget::showSystemHealthMessage(QnSystemHealth::MessageType message, const QVariant& params)
{
    QnResourcePtr resource;
    if (params.canConvert<QnResourcePtr>())
        resource = params.value<QnResourcePtr>();

    vms::event::AbstractActionPtr action;
    if (params.canConvert<vms::event::AbstractActionPtr>())
    {
        action = params.value<vms::event::AbstractActionPtr>();
        if (action)
        {
            QnUuid resourceId;
            const auto runtimeParameters = action->getRuntimeParams();
            if (!runtimeParameters.metadata.cameraRefs.empty())
                resourceId = runtimeParameters.metadata.cameraRefs[0];
            else
                resourceId = runtimeParameters.eventResourceId;

            resource = resourcePool()->getResourceById(resourceId);
        }
    }

    QnNotificationWidget* item = findItem(message, resource);
    if (item)
        return;

    const QString resourceName = QnResourceDisplayInfo(resource).toString(qnSettings->extraInfoInTree());
    QString messageText = QnSystemHealthStringsHelper::messageText(message, resourceName);
    NX_ASSERT(!messageText.isEmpty(), Q_FUNC_INFO, "Undefined system health message ");
    if (messageText.isEmpty())
        return;

    if (action && isRemoteArchiveMessage(message))
    {
        auto description = action->getRuntimeParams().description;
        if (!description.isEmpty())
            messageText = description;
    }

    if (messageText.isEmpty())
        return;

    item = new QnNotificationWidget(m_list);

    action::Parameters actionParams;
    if (params.canConvert<action::Parameters>())
        actionParams = params.value<action::Parameters>();

    switch (message)
    {
        case QnSystemHealth::EmailIsEmpty:
            item->addActionButton(
                qnSkin->icon("events/email.png"),
                action::UserSettingsAction,
                action::Parameters(context()->user())
                    .withArgument(Qn::FocusElementRole, lit("email"))
                    .withArgument(Qn::FocusTabRole, QnUserSettingsDialog::SettingsPage)
            );
            break;

        case QnSystemHealth::NoLicenses:
            item->addActionButton(
                qnSkin->icon("events/license.png"),
                action::PreferencesLicensesTabAction);
            break;

        case QnSystemHealth::SmtpIsNotSet:
            item->addActionButton(
                qnSkin->icon("events/email.png"),
                action::PreferencesSmtpTabAction);
            break;

        case QnSystemHealth::UsersEmailIsEmpty:
            item->addActionButton(
                qnSkin->icon("events/email.png"),
                action::UserSettingsAction,
                action::Parameters(resource)
                    .withArgument(Qn::FocusElementRole, lit("email"))
                    .withArgument(Qn::FocusTabRole, QnUserSettingsDialog::SettingsPage)
                );
            break;

        case QnSystemHealth::SystemIsReadOnly:
            break;

        case QnSystemHealth::EmailSendError:
            item->addActionButton(
                qnSkin->icon("events/email.png"),
                action::PreferencesSmtpTabAction);
            break;

        case QnSystemHealth::StoragesNotConfigured:
        case QnSystemHealth::StoragesAreFull:
        case QnSystemHealth::ArchiveRebuildFinished:
        case QnSystemHealth::ArchiveRebuildCanceled:
            item->addActionButton(
                qnSkin->icon("events/storage.png"),
                action::ServerSettingsAction,
                action::Parameters(resource)
                    .withArgument(Qn::FocusTabRole, QnServerSettingsDialog::StorageManagmentPage)
            );
            break;

        case QnSystemHealth::CloudPromo:
        {
            item->addActionButton(
                qnSkin->icon("cloud/cloud_20.png"),
                action::PreferencesCloudTabAction);

            const auto hideCloudPromoNextRun =
                [this]
                {
                    menu()->trigger(action::HideCloudPromoAction);
                };

            connect(item, &QnNotificationWidget::actionTriggered, this, hideCloudPromoNextRun);
            connect(item, &QnNotificationWidget::closeTriggered, this, hideCloudPromoNextRun);

            connect(item, &QnNotificationWidget::linkActivated, this,
                [item](const QString& link)
                {
                    if (link.contains(lit("://")))
                        QDesktopServices::openUrl(link);
                    else
                        item->triggerDefaultAction();
                });

            break;
        }

        case QnSystemHealth::RemoteArchiveSyncStarted:
        case QnSystemHealth::RemoteArchiveSyncFinished:
        case QnSystemHealth::RemoteArchiveSyncError:
        case QnSystemHealth::RemoteArchiveSyncProgress:
            break;

        default:
            NX_ASSERT(false, Q_FUNC_INFO, "Undefined system health message ");
            break;
    }

    item->setText(messageText);
    item->setTooltipText(QnSystemHealthStringsHelper::messageTooltip(message, resourceName));
    item->setNotificationLevel(QnNotificationLevel::valueOf(message));
    item->setProperty(kItemResourcePropertyName, QVariant::fromValue<QnResourcePtr>(resource));
    setHelpTopic(item, QnBusiness::healthHelpId(message));

    /* We use Qt::QueuedConnection as our handler may start the event loop. */
    connect(item, &QnNotificationWidget::actionTriggered, this,
        &QnNotificationsCollectionWidget::at_item_actionTriggered, Qt::QueuedConnection);

    m_list->addItem(item, QnSystemHealth::isMessageLocked(message));
    m_itemsByMessageType.insert(message, item);
}

void QnNotificationsCollectionWidget::hideSystemHealthMessage(QnSystemHealth::MessageType message, const QVariant& params)
{
    QnResourcePtr resource;
    if (params.canConvert<QnResourcePtr>())
        resource = params.value<QnResourcePtr>();

    if (!resource)
    {
        for (QnNotificationWidget* item : m_itemsByMessageType.values(message))
            cleanUpItem(item);

        return;
    }

    QnNotificationWidget* target = findItem(message, resource);
    if (!target)
        return;

    cleanUpItem(target);
}

void QnNotificationsCollectionWidget::hideAll()
{
    m_list->clear();
    m_itemsByMessageType.clear();
    m_itemsByLoadingSound.clear();
    m_itemsByEventRuleId.clear();
}

void QnNotificationsCollectionWidget::updateBlinker()
{
    if (!blinker())
        return;

    blinker()->setNotificationCount(m_list->itemCount());
    blinker()->setColor(QnNotificationLevel::notificationColor(m_list->notificationLevel()));
}

void QnNotificationsCollectionWidget::at_list_itemRemoved(QnNotificationWidget* item)
{
    cleanUpItem(item);
    qnDeleteLater(item);
}

void QnNotificationsCollectionWidget::at_item_actionTriggered(action::IDType actionId,
    const action::Parameters& parameters)
{
    menu()->trigger(actionId, parameters);
}

void QnNotificationsCollectionWidget::at_notificationCache_fileDownloaded(const QString& filename)
{
    QString filePath = context()->instance<ServerNotificationCache>()->getFullPath(filename);
    for (QnNotificationWidget* item : m_itemsByLoadingSound.values(filename))
        item->setSound(filePath, true);

    m_itemsByLoadingSound.remove(filename);
}

void QnNotificationsCollectionWidget::cleanUpItem(QnNotificationWidget* item)
{
    m_list->removeItem(item);

    for (QnSystemHealth::MessageType messageType : m_itemsByMessageType.keys(item))
        m_itemsByMessageType.remove(messageType, item);

    for (const QnUuid& ruleId : m_itemsByEventRuleId.keys(item))
        m_itemsByEventRuleId.remove(ruleId, item);

    for (QString soundPath : m_itemsByLoadingSound.keys(item))
        m_itemsByLoadingSound.remove(soundPath, item);

    const auto key = m_customPopupItems.key(item);
    m_customPopupItems.remove(key);
}

void QnNotificationsCollectionWidget::paint(QPainter* painter,
    const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    base_type::paint(painter, option, widget);

    QnScopedPainterPenRollback penRollback(painter, QPen(palette().color(QPalette::Mid), 0.0));
    QnScopedPainterAntialiasingRollback aaRollback(painter, false);

    qreal y = m_headerWidget->rect().height() + 0.5;
    painter->drawLine(QPointF(1.0, y), QPointF(rect().width() - 1.0, y));
}
