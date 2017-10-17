#include "system_health_list_model_p.h"

#include <client/client_settings.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <health/system_health_helper.h>
#include <ui/common/notification_levels.h>
#include <ui/dialogs/resource_properties/user_settings_dialog.h>
#include <ui/dialogs/resource_properties/server_settings_dialog.h>
#include <ui/help/business_help.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/handlers/workbench_notifications_handler.h>

#include <nx/client/desktop/ui/actions/action.h>
#include <nx/client/desktop/ui/actions/action_parameters.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/strings_helper.h>

namespace nx {
namespace client {
namespace desktop {

using namespace ui;

SystemHealthListModel::Private::Private(SystemHealthListModel* q):
    base_type(),
    QnWorkbenchContextAware(q),
    q(q),
    m_helper(new vms::event::StringsHelper(commonModule()))
{
    const auto handler = context()->instance<QnWorkbenchNotificationsHandler>();
    connect(handler, &QnWorkbenchNotificationsHandler::cleared, q, &EventListModel::clear);
    connect(handler, &QnWorkbenchNotificationsHandler::systemHealthEventAdded,
        this, &Private::addSystemHealthEvent);
    connect(handler, &QnWorkbenchNotificationsHandler::systemHealthEventRemoved,
        this, &Private::removeSystemHealthEvent);
}

SystemHealthListModel::Private::~Private()
{
}

void SystemHealthListModel::Private::addSystemHealthEvent(
    QnSystemHealth::MessageType message,
    const QVariant& params)
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
            auto resourceId = action->getRuntimeParams().eventResourceId;
            resource = resourcePool()->getResourceById(resourceId);
        }
    }

    // TODO: #vkutin We may want multiple resource aggregation as one event.

    const auto uuidKey = qMakePair(message, resource.data());
    if (m_uuidHash.contains(uuidKey))
        return;

    const auto resourceName = QnResourceDisplayInfo(resource).toString(qnSettings->extraInfoInTree());
    const auto messageText = QnSystemHealthStringsHelper::messageText(message, resourceName);
    NX_ASSERT(!messageText.isEmpty(), Q_FUNC_INFO, "Undefined system health message ");
    if (messageText.isEmpty())
        return;

    EventData eventData;
    eventData.id = QnUuid::createUuid();
    eventData.title = messageText;
    eventData.toolTip = QnSystemHealthStringsHelper::messageTooltip(message, resourceName);
    eventData.helpId = QnBusiness::healthHelpId(message);

    eventData.titleColor = QnNotificationLevel::notificationColor(
        QnNotificationLevel::valueOf(message));

    switch (message)
    {
        case QnSystemHealth::EmailIsEmpty:
            eventData.icon = qnSkin->pixmap("events/email.png");
            eventData.actionId = action::UserSettingsAction;
            eventData.actionParameters = action::Parameters(context()->user())
                .withArgument(Qn::FocusElementRole, lit("email"))
                .withArgument(Qn::FocusTabRole, QnUserSettingsDialog::SettingsPage);
            break;

        case QnSystemHealth::NoLicenses:
            eventData.icon = qnSkin->pixmap("events/license.png");
            eventData.actionId = action::PreferencesLicensesTabAction;
            break;

        case QnSystemHealth::SmtpIsNotSet:
            eventData.icon = qnSkin->pixmap("events/email.png");
            eventData.actionId = action::PreferencesSmtpTabAction;
            break;

        case QnSystemHealth::UsersEmailIsEmpty:
            eventData.icon = qnSkin->pixmap("events/email.png");
            eventData.actionId = action::UserSettingsAction;
            eventData.actionParameters = action::Parameters(resource)
                .withArgument(Qn::FocusElementRole, lit("email"))
                .withArgument(Qn::FocusTabRole, QnUserSettingsDialog::SettingsPage);
            break;

        case QnSystemHealth::SystemIsReadOnly:
            eventData.icon = qnSkin->pixmap("tree/system.png");
            break;

        case QnSystemHealth::EmailSendError:
            eventData.icon = qnSkin->pixmap("events/email.png");
            eventData.actionId = action::PreferencesSmtpTabAction;
            break;

        case QnSystemHealth::StoragesNotConfigured:
        case QnSystemHealth::StoragesAreFull:
        case QnSystemHealth::ArchiveRebuildFinished:
        case QnSystemHealth::ArchiveRebuildCanceled:
            eventData.icon = qnSkin->pixmap("events/storage.png");
            eventData.actionId = action::ServerSettingsAction;
            action::Parameters(resource)
                .withArgument(Qn::FocusTabRole, QnServerSettingsDialog::StorageManagmentPage);
            break;

        case QnSystemHealth::CloudPromo:
        {
            eventData.icon = qnSkin->pixmap("cloud/cloud_20.png");
            eventData.actionId = action::PreferencesCloudTabAction;
            // TODO: #vkutin Restore functionality.
            /*
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
                */
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

    // TODO: #vkutin Restore functionality.
    // QnSystemHealth::isMessageLocked(message)

    m_uuidHash[uuidKey] = eventData.id;
    q->addEvent(eventData);
}

void SystemHealthListModel::Private::removeSystemHealthEvent(
    QnSystemHealth::MessageType message,
    const QVariant& params)
{
}

} // namespace
} // namespace client
} // namespace nx
