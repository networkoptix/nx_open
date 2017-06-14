#include "notifications_collection_widget.h"

#include <QtGui/QDesktopServices>

#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QStyle>

#include <nx/utils/log/log.h>

#include <business/business_strings_helper.h>

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

//TODO: #gdm think about moving out pages enums
#include <ui/dialogs/resource_properties/user_settings_dialog.h>
#include <ui/dialogs/resource_properties/server_settings_dialog.h>

#include <ui/graphics/items/generic/blinking_image_widget.h>
#include <ui/graphics/items/notifications/notification_widget.h>
#include <ui/graphics/items/notifications/notification_list_widget.h>
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
#include <health/system_health_helper.h>

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

using namespace nx::client::desktop;
using namespace nx::client::desktop::ui;

namespace {

static const QSize kDefaultThumbnailSize(0, QnThumbnailRequestData::kMinimumSize);

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
    m_helper(new QnBusinessStringsHelper(commonModule()))
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
    connect(handler,    &QnWorkbenchNotificationsHandler::notificationAdded,        this,   &QnNotificationsCollectionWidget::showBusinessAction);
    connect(handler,    &QnWorkbenchNotificationsHandler::notificationRemoved,      this,   &QnNotificationsCollectionWidget::hideBusinessAction);
    connect(handler,    &QnWorkbenchNotificationsHandler::systemHealthEventAdded,   this,   &QnNotificationsCollectionWidget::showSystemHealthMessage);
    connect(handler,    &QnWorkbenchNotificationsHandler::systemHealthEventRemoved, this,   &QnNotificationsCollectionWidget::hideSystemHealthMessage);
    connect(handler,    &QnWorkbenchNotificationsHandler::cleared,                  this,   &QnNotificationsCollectionWidget::hideAll);

    using namespace nx::client::desktop;
    connect(this->context()->instance<ServerNotificationCache>(),
        &ServerNotificationCache::fileDownloaded,
        this,
        [this](const QString& filename, ServerFileCache::OperationResult status)
        {
            if (status != ServerFileCache::OperationResult::ok)
                return;
            at_notificationCache_fileDownloaded(filename);
        });
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
    QnSingleThumbnailLoader* loader = new QnSingleThumbnailLoader(
        camera,
        msecSinceEpoch,
        QnThumbnailRequestData::kDefaultRotation,
        kDefaultThumbnailSize,
        QnThumbnailRequestData::JpgFormat,
        item);

    item->setImageProvider(loader);
}

void QnNotificationsCollectionWidget::loadThumbnailForItem(
    QnNotificationWidget* item,
    const QnVirtualCameraResourceList& cameraList,
    qint64 msecSinceEpoch)
{
    if (cameraList.isEmpty())
        return;

    QnMultiImageProvider::Providers providers;
    for (const auto& camera: cameraList)
    {
        NX_ASSERT(accessController()->hasPermissions(camera, Qn::ViewContentPermission));
        std::unique_ptr<QnImageProvider> provider(new QnSingleThumbnailLoader(
            camera,
            msecSinceEpoch,
            QnThumbnailRequestData::kDefaultRotation,
            kDefaultThumbnailSize,
            QnThumbnailRequestData::JpgFormat));

        providers.push_back(std::move(provider));
    }

    item->setImageProvider(new QnMultiImageProvider(std::move(providers), Qt::Vertical, kMultiThumbnailSpacing, item));
}

void QnNotificationsCollectionWidget::handleShowPopupAction(
    const QnAbstractBusinessActionPtr& businessAction,
    QnNotificationWidget* widget)
{
    const auto params = businessAction->getParams();
    if (params.targetActionType == QnBusiness::UndefinedAction)
        return;

    if (params.targetActionType != QnBusiness::BookmarkAction)
        return;

    QnCameraBookmarkList bookmarks;
    for (const auto& resourceId: businessAction->getResources())
    {
        using namespace nx::client::desktop::ui;
        const auto camera = resourcePool()->getResourceById<QnSecurityCamResource>(resourceId);
        if (!camera)
        {
            NX_EXPECT(false, "Invalid camera resource");
            continue;
        }

        const auto bookmark = helpers::bookmarkFromAction(businessAction, camera, commonModule());
        if (!bookmark.isValid())
        {
            NX_EXPECT(false, "Invalid bookmark");
            continue;
        }

        bookmarks.append(bookmark);
    }

    if (bookmarks.isEmpty())
        return;

    widget->addTextButton(QIcon(), tr("Bookmark it"),
        [bookmarks]()
        {
            for (const auto bookmark: bookmarks)
                qnCameraBookmarksManager->addCameraBookmark(bookmark);
        });
}

void QnNotificationsCollectionWidget::showBusinessAction(const QnAbstractBusinessActionPtr& businessAction)
{
    if (m_list->itemCount() >= kMaxNotificationItems)
        return; /* Just drop the notification if we already have too many of them in queue. */

    QnBusinessEventParameters params = businessAction->getRuntimeParams();
    QnBusiness::EventType eventType = params.eventType;
    QnUuid ruleId = businessAction->getBusinessRuleId();
    QString title = m_helper->eventAtResource(params, qnSettings->extraInfoInTree());
    qint64 timestampMs = params.eventTimestampUsec / 1000;

    auto alarmCameras = resourcePool()->getResources<QnVirtualCameraResource>(businessAction->getResources());
    if (businessAction->getParams().useSource)
        alarmCameras << resourcePool()->getResources<QnVirtualCameraResource>(businessAction->getSourceResources());
    alarmCameras = accessController()->filtered(alarmCameras, Qn::ViewContentPermission);
    alarmCameras = alarmCameras.toSet().toList();

    QnResourcePtr resource = resourcePool()->getResourceById(params.eventResourceId);
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();

    if (businessAction->actionType() == QnBusiness::ShowOnAlarmLayoutAction)
    {
        if (alarmCameras.isEmpty())
            return;

        auto findItemPredicate =
            [timestampMs](QnNotificationWidget* item)
            {
                return item->property(kItemActionTypePropertyName) == QnBusiness::ShowOnAlarmLayoutAction
                    && item->property(kItemTimeStampPropertyName) == timestampMs;
            };

        if (findItem(ruleId, findItemPredicate))
            return; /* Show 'Alarm Layout' notifications only once for each event of one rule. */

        title = tr("Alarm: %1").arg(title);
    }
    else
    {
        if (QnBusiness::isSourceCameraRequired(eventType))
        {
            NX_ASSERT(camera, Q_FUNC_INFO, "Event has occurred without its camera");
            if (!camera || !accessController()->hasPermissions(camera, Qn::ViewContentPermission))
                return;
        }

        if (QnBusiness::isSourceServerRequired(eventType))
        {
            NX_ASSERT(server, Q_FUNC_INFO, "Event has occurred without its server");
            /* Only admins should see notifications with servers. */
            if (!server || !accessController()->hasPermissions(server, Qn::ViewContentPermission))
                return;
        }
    }

    QStringList tooltip = m_helper->eventDescription(businessAction,
        QnBusinessAggregationInfo(), qnSettings->extraInfoInTree());

    //TODO: #GDM #3.1 move this code to ::eventDetails()
    if (eventType == QnBusiness::LicenseIssueEvent
        && params.reasonCode == QnBusiness::LicenseRemoved)
    {
        QStringList disabledCameras;
        for (const QString& stringId : params.description.split(L';'))
        {
            QnUuid id = QnUuid::fromStringSafe(stringId);
            NX_ASSERT(!id.isNull());
            if (auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(id))
                tooltip << QnResourceDisplayInfo(camera).toString(qnSettings->extraInfoInTree());
        }
    }


    QnNotificationWidget* item = new QnNotificationWidget(m_list);
    item->setText(title);
    item->setTooltipText(tooltip.join(L'\n'));
    item->setNotificationLevel(QnNotificationLevel::valueOf(businessAction));
    item->setProperty(kItemResourcePropertyName,   QVariant::fromValue<QnResourcePtr>(resource));
    item->setProperty(kItemActionTypePropertyName, businessAction->actionType());
    item->setProperty(kItemTimeStampPropertyName,  timestampMs);
    setHelpTopic(item, QnBusiness::eventHelpId(eventType));

    switch(businessAction->actionType())
    {
        case QnBusiness::PlaySoundAction:
        {
            QString soundUrl = businessAction->getParams().url;
            m_itemsByLoadingSound.insert(soundUrl, item);
            context()->instance<ServerNotificationCache>()->downloadFile(soundUrl);
            break;
        }
        case QnBusiness::ShowPopupAction:
            handleShowPopupAction(businessAction, item);
            break;
        default:
            break;
    };
    QIcon icon = iconForAction(businessAction);

    if (businessAction->actionType() == QnBusiness::ShowOnAlarmLayoutAction)
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
            case QnBusiness::CameraMotionEvent:
            case QnBusiness::SoftwareTriggerEvent:
            {
                item->addActionButton(
                    icon,
                    action::OpenInNewTabAction,
                    action::Parameters(camera).withArgument(Qn::ItemTimeRole, timestampMs));

                loadThumbnailForItem(item, camera, timestampMs);
                break;
            }

            case QnBusiness::CameraInputEvent:
            {
                item->addActionButton(
                    icon,
                    action::OpenInNewTabAction,
                    camera);

                loadThumbnailForItem(item, camera);
                break;
            }

            case QnBusiness::CameraDisconnectEvent:
            case QnBusiness::NetworkIssueEvent:
            {
                item->addActionButton(
                    icon,
                    action::CameraSettingsAction,
                    camera);

                loadThumbnailForItem(item, camera);
                break;
            }

            case QnBusiness::StorageFailureEvent:
            case QnBusiness::BackupFinishedEvent:
            case QnBusiness::ServerStartEvent:
            case QnBusiness::ServerFailureEvent:
            {
                item->addActionButton(
                    icon,
                    action::ServerSettingsAction,
                    server);
                break;
            }

            case QnBusiness::CameraIpConflictEvent:
            {
                QString webPageAddress = params.caption;
                item->addActionButton(
                    icon,
                    action::BrowseUrlAction,
                    {Qn::UrlRole, webPageAddress});
                break;
            }

            case QnBusiness::ServerConflictEvent:
            {
                item->addActionButton(icon);
                break;
            }

            case QnBusiness::LicenseIssueEvent:
            {
                item->addActionButton(
                    icon,
                    action::PreferencesLicensesTabAction);
                break;
            }

            case QnBusiness::UserDefinedEvent:
            {
                auto sourceCameras = resourcePool()->getResources<QnVirtualCameraResource>(params.metadata.cameraRefs);
                sourceCameras = accessController()->filtered(sourceCameras, Qn::ViewContentPermission);
                if (!sourceCameras.isEmpty())
                {
                    item->addActionButton(
                        icon,
                        action::OpenInNewTabAction,
                        action::Parameters(sourceCameras).withArgument(Qn::ItemTimeRole, timestampMs));

                    loadThumbnailForItem(item, sourceCameras.mid(0, kMaxThumbnailCount), timestampMs);
                }
                break;
            }

            default:
                break;
        }
    }


    m_itemsByBusinessRuleId.insert(ruleId, item);

    connect(item, &QnNotificationWidget::buttonClicked, this, [this](const QString& alias)
    {
        context()->statisticsModule()->registerClick(alias);
    });

    /* We use Qt::QueuedConnection as our handler may start the event loop. */
    connect(item, &QnNotificationWidget::actionTriggered, this, &QnNotificationsCollectionWidget::at_item_actionTriggered, Qt::QueuedConnection);
    m_list->addItem(item, businessAction->actionType() == QnBusiness::PlaySoundAction);
}

void QnNotificationsCollectionWidget::hideBusinessAction(const QnAbstractBusinessActionPtr& businessAction)
{
    QnUuid ruleId = businessAction->getBusinessRuleId();

    if (businessAction->actionType() == QnBusiness::PlaySoundAction)
    {
        for (QnNotificationWidget* item: m_itemsByBusinessRuleId.values(ruleId))
        {
            m_list->removeItem(item);
            cleanUpItem(item);
        }
    }

    QnResourcePtr resource = resourcePool()->getResourceById(businessAction->getRuntimeParams().eventResourceId);
    if (!resource)
        return;

    QnNotificationWidget* item = findItem(ruleId, resource);
    if (!item)
        return;

    m_list->removeItem(item);
    cleanUpItem(item);
}

QnNotificationWidget* QnNotificationsCollectionWidget::findItem(QnSystemHealth::MessageType message, const QnResourcePtr& resource)
{
    for (QnNotificationWidget* item: m_itemsByMessageType.values(message))
        if (resource == item->property(kItemResourcePropertyName).value<QnResourcePtr>())
            return item;

    return nullptr;
}

QnNotificationWidget* QnNotificationsCollectionWidget::findItem(const QnUuid& businessRuleId, const QnResourcePtr& resource)
{
    for (QnNotificationWidget* item: m_itemsByBusinessRuleId.values(businessRuleId))
        if (resource == item->property(kItemResourcePropertyName).value<QnResourcePtr>())
            return item;

    return nullptr;
}

QnNotificationWidget* QnNotificationsCollectionWidget::findItem(const QnUuid& businessRuleId, std::function<bool(QnNotificationWidget* item)> filter)
{
    for (QnNotificationWidget* item : m_itemsByBusinessRuleId.values(businessRuleId))
        if (filter(item))
            return item;

    return nullptr;
}

QIcon QnNotificationsCollectionWidget::iconForAction(const QnAbstractBusinessActionPtr& businessAction) const
{
    if (businessAction->actionType() == QnBusiness::PlaySoundAction)
        return qnSkin->icon("events/sound.png");

    if (businessAction->actionType() == QnBusiness::ShowOnAlarmLayoutAction)
        return qnSkin->icon("events/alarm.png");

    auto params = businessAction->getRuntimeParams();
    QnBusiness::EventType eventType = params.eventType;

    if (eventType >= QnBusiness::UserDefinedEvent)
    {
        QnVirtualCameraResourceList camList = resourcePool()->getResources<QnVirtualCameraResource>(params.metadata.cameraRefs);
        if (!camList.isEmpty())
            return qnResIconCache->icon(QnResourceIconCache::Camera);

        return QIcon();
    }

    switch (eventType)
    {
        case QnBusiness::CameraMotionEvent:
        case QnBusiness::CameraInputEvent:
        case QnBusiness::CameraDisconnectEvent:
        case QnBusiness::CameraIpConflictEvent:
        case QnBusiness::NetworkIssueEvent:
        {
            auto resource = resourcePool()->getResourceById(params.eventResourceId);
            return resource
                ? qnResIconCache->icon(resource)
                : qnResIconCache->icon(QnResourceIconCache::Camera);
        }

        case QnBusiness::SoftwareTriggerEvent:
        {
            return QnSoftwareTriggerPixmaps::colorizedPixmap(
                businessAction->getRuntimeParams().description,
                palette().color(QPalette::WindowText));
        }

        case QnBusiness::StorageFailureEvent:
            return qnSkin->icon("events/storage.png");

        case QnBusiness::ServerStartEvent:
        case QnBusiness::ServerFailureEvent:
        case QnBusiness::ServerConflictEvent:
        case QnBusiness::BackupFinishedEvent:
            return qnResIconCache->icon(QnResourceIconCache::Server);

        case QnBusiness::LicenseIssueEvent:
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

    QnAbstractBusinessActionPtr businessAction;
    if (params.canConvert<QnAbstractBusinessActionPtr>())
    {
        businessAction = params.value<QnAbstractBusinessActionPtr>();
        if (businessAction)
        {
            auto resourceId = businessAction->getRuntimeParams().eventResourceId;
            resource = resourcePool()->getResourceById(resourceId);
        }
    }

    QnNotificationWidget* item = findItem(message, resource);
    if (item)
        return;

    const QString resourceName = QnResourceDisplayInfo(resource).toString(qnSettings->extraInfoInTree());
    const QString messageText = QnSystemHealthStringsHelper::messageText(message, resourceName);
    NX_ASSERT(!messageText.isEmpty(), Q_FUNC_INFO, "Undefined system health message ");
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

        case QnSystemHealth::NoPrimaryTimeServer:
            item->addActionButton(
                qnSkin->icon("events/settings.png"),
                action::SelectTimeServerAction,
                actionParams);
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
                    if (link.contains(lit("://"))) //< currently unused
                        QDesktopServices::openUrl(link);
                    else
                        item->triggerDefaultAction();
                });

            break;
        }

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
            m_list->removeItem(item);

        m_itemsByMessageType.remove(message);
        return;
    }

    QnNotificationWidget* target = findItem(message, resource);
    if (!target)
        return;

    m_list->removeItem(target);
    m_itemsByMessageType.remove(message, target);
}

void QnNotificationsCollectionWidget::hideAll()
{
    m_list->clear();
    m_itemsByMessageType.clear();
    m_itemsByLoadingSound.clear();
    m_itemsByBusinessRuleId.clear();
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
    for (QnSystemHealth::MessageType messageType : m_itemsByMessageType.keys(item))
        m_itemsByMessageType.remove(messageType, item);

    for (const QnUuid& ruleId : m_itemsByBusinessRuleId.keys(item))
        m_itemsByBusinessRuleId.remove(ruleId, item);

    for (QString soundPath : m_itemsByLoadingSound.keys(item))
        m_itemsByLoadingSound.remove(soundPath, item);
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
