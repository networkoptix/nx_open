#include "system_health_list_model_p.h"

#include <chrono>

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
#include <utils/common/delayed.h>

#include <nx/client/desktop/ui/actions/action.h>
#include <nx/client/desktop/ui/actions/action_parameters.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/strings_helper.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

const auto kDisplayTimeout = std::chrono::milliseconds(12500);

} // namespace

using namespace ui;

SystemHealthListModel::Private::Private(SystemHealthListModel* q):
    base_type(),
    QnWorkbenchContextAware(q),
    q(q),
    m_helper(new vms::event::StringsHelper(commonModule())),
    m_uuidHashes()
{
    const auto handler = context()->instance<QnWorkbenchNotificationsHandler>();
    connect(handler, &QnWorkbenchNotificationsHandler::cleared, q, &EventListModel::clear);
    connect(handler, &QnWorkbenchNotificationsHandler::systemHealthEventAdded,
        this, &Private::addSystemHealthEvent);
    connect(handler, &QnWorkbenchNotificationsHandler::systemHealthEventRemoved,
        this, &Private::removeSystemHealthEvent);

    connect(q, &EventListModel::modelReset, this,
        [this]()
        {
            for (auto& hash: m_uuidHashes)
                hash.clear();
        });

    connect(q, &EventListModel::rowsAboutToBeRemoved, this,
        [this](const QModelIndex& /*parent*/, int first, int last)
        {
            for (int row = first; row <= last; ++row)
            {
                const auto extraData = Private::extraData(this->q->getEvent(row));
                m_uuidHashes[extraData.first].remove(extraData.second);
            }
        });
}

SystemHealthListModel::Private::~Private()
{
}

SystemHealthListModel::Private::ExtraData SystemHealthListModel::Private::extraData(
    const EventData& event)
{
    NX_ASSERT(event.extraData.canConvert<ExtraData>(), Q_FUNC_INFO);
    return event.extraData.value<ExtraData>();
}

int SystemHealthListModel::Private::eventPriority(const EventData& event) const
{
    const auto extraData = Private::extraData(event);
    switch (extraData.first)
    {
        case QnSystemHealth::CloudPromo:
            return int(QnNotificationLevel::Value::LevelCount); //< The highest.

        default:
            return int(QnNotificationLevel::valueOf(extraData.first));
    }
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
    if (m_uuidHashes[message].contains(resource))
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
    eventData.removable = true;
    eventData.extraData = qVariantFromValue(ExtraData(message, resource));

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

    if (!q->addEvent(eventData))
        return;

    m_uuidHashes[message][resource] = eventData.id;

    if (!QnSystemHealth::isMessageLocked(message))
    {
        executeDelayedParented([this, id = eventData.id]() { q->removeEvent(id); },
            std::chrono::milliseconds(kDisplayTimeout).count(), this);
    }
}

void SystemHealthListModel::Private::removeSystemHealthEvent(
    QnSystemHealth::MessageType message,
    const QVariant& params)
{
    QnResourcePtr resource;
    if (params.canConvert<QnResourcePtr>())
        resource = params.value<QnResourcePtr>();

    const auto& uuidHash = m_uuidHashes[message];

    if (resource)
    {
        if (uuidHash.contains(resource))
            q->removeEvent(uuidHash.value(resource));
    }
    else
    {
        for (const auto& id: uuidHash.values()) //< Must iterate a copy of the list.
            q->removeEvent(id);
    }
}

} // namespace
} // namespace client
} // namespace nx
