#include "system_health_list_model_p.h"

#include <chrono>
#include <limits>

#include <client/client_settings.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_changes_listener.h>
#include <core/resource_management/resource_pool.h>
#include <health/system_health_helper.h>
#include <ui/common/notification_levels.h>
#include <ui/help/business_help.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/handlers/workbench_notifications_handler.h>
// TODO: #vkutin Dialogs are included just for tab identifiers. Needs refactoring to avoid it.
#include <ui/dialogs/resource_properties/user_settings_dialog.h>
#include <ui/dialogs/resource_properties/server_settings_dialog.h>
#include <ui/dialogs/system_administration_dialog.h>

#include <nx/vms/client/desktop/ui/actions/action.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/system_health/system_health_state.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/strings_helper.h>
#include <nx/utils/string.h>

namespace nx::vms::client::desktop {

using namespace ui;

//-------------------------------------------------------------------------------------------------
// SystemHealthListModel::Private::Item

SystemHealthListModel::Private::Item::Item(
    QnSystemHealth::MessageType message,
    const QnResourcePtr& resource)
    :
    message(message),
    resource(resource)
{
}

SystemHealthListModel::Private::Item::operator QnSystemHealth::MessageType() const
{
    return message;
}

bool SystemHealthListModel::Private::Item::operator==(const Item& other) const
{
    return message == other.message && resource == other.resource;
}

bool SystemHealthListModel::Private::Item::operator!=(const Item& other) const
{
    return !(*this == other);
}

bool SystemHealthListModel::Private::Item::operator<(const Item& other) const
{
    return message != other.message
        ? message < other.message
        : resource < other.resource;
}

//-------------------------------------------------------------------------------------------------
// SystemHealthListModel::Private

SystemHealthListModel::Private::Private(SystemHealthListModel* q) :
    base_type(),
    QnWorkbenchContextAware(q),
    q(q),
    m_helper(new vms::event::StringsHelper(commonModule())),
    m_popupSystemHealthFilter(qnSettings->popupSystemHealth())
{
    // Handle system health state.

    const auto systemHealthState = context()->instance<SystemHealthState>();
    connect(systemHealthState, &SystemHealthState::stateChanged, this, &Private::toggleItem);
    connect(systemHealthState, &SystemHealthState::dataChanged, this, &Private::updateItem);

    for (const auto index: QnSystemHealth::allVisibleMessageTypes())
        toggleItem(index, systemHealthState->state(index));

    auto userChangesListener = new QnResourceChangesListener(this);
    userChangesListener->connectToResources<QnUserResource>(&QnUserResource::nameChanged,
        [this](const QnUserResourcePtr& user)
        {
            if (m_usersWithInvalidEmail.contains(user))
                updateItem(QnSystemHealth::MessageType::UsersEmailIsEmpty);
        });

    // Handle system health notifications.

    const auto notificationHandler = context()->instance<QnWorkbenchNotificationsHandler>();
    connect(notificationHandler, &QnWorkbenchNotificationsHandler::cleared, this, &Private::clear);

    connect(notificationHandler, &QnWorkbenchNotificationsHandler::systemHealthEventAdded,
        this, &Private::addItem);

    connect(notificationHandler, &QnWorkbenchNotificationsHandler::systemHealthEventRemoved,
        this, &Private::removeItem);

    connect(qnSettings->notifier(QnClientSettings::POPUP_SYSTEM_HEALTH),
        &QnPropertyNotifier::valueChanged, this,
        [this](int id)
        {
            if (id != QnClientSettings::POPUP_SYSTEM_HEALTH)
                return;

            const auto filter = qnSettings->popupSystemHealth();
            const auto systemHealthState = context()->instance<SystemHealthState>();

            for (const auto index: QnSystemHealth::allVisibleMessageTypes())
            {
                const bool wasVisible = m_popupSystemHealthFilter.contains(index);
                const bool isVisible = filter.contains(index);
                if (wasVisible != isVisible)
                    toggleItem(index, isVisible && systemHealthState->state(index));
            }

            m_popupSystemHealthFilter = filter;
        });
}

SystemHealthListModel::Private::~Private()
{
}

int SystemHealthListModel::Private::count() const
{
    return int(m_items.size());
}

QnSystemHealth::MessageType SystemHealthListModel::Private::message(int index) const
{
    return m_items[index].message;
}

QnResourcePtr SystemHealthListModel::Private::resource(int index) const
{
    return m_items[index].resource;
}

QString SystemHealthListModel::Private::text(int index) const
{
    const auto& item = m_items[index];
    switch (item.message)
    {
        case QnSystemHealth::UsersEmailIsEmpty:
            return tr("Email address is not set for %n users", "", m_usersWithInvalidEmail.size());

        case QnSystemHealth::RemoteArchiveSyncStarted:
        case QnSystemHealth::RemoteArchiveSyncFinished:
        case QnSystemHealth::RemoteArchiveSyncProgress:
        case QnSystemHealth::RemoteArchiveSyncError:
        {
            // TODO: #vkutin This is bad, remove it after VMS-7724 refactor is done.
            const auto description = item.serverData->getRuntimeParams().description;
            if (!description.isEmpty())
                return description;
        }
        [[fallthrough]];

        default:
            return QnSystemHealthStringsHelper::messageText(item.message,
                QnResourceDisplayInfo(item.resource).toString(qnSettings->extraInfoInTree()));
    }
}

QnResourceList SystemHealthListModel::Private::displayedResourceList(int index) const
{
    const auto& item = m_items[index];
    switch (item.message)
    {
        case QnSystemHealth::UsersEmailIsEmpty:
            return m_usersWithInvalidEmail;

        default:
            return {};
    }
}

QString SystemHealthListModel::Private::toolTip(int index) const
{
    const auto& item = m_items[index];
    return QnSystemHealthStringsHelper::messageTooltip(item.message,
        QnResourceDisplayInfo(item.resource).toString(qnSettings->extraInfoInTree()));
}

QPixmap SystemHealthListModel::Private::pixmap(int index) const
{
    return pixmap(m_items[index].message);
}

QColor SystemHealthListModel::Private::color(int index) const
{
    return QnNotificationLevel::notificationTextColor(
        QnNotificationLevel::valueOf(m_items[index].message));
}

int SystemHealthListModel::Private::helpId(int index) const
{
    return QnBusiness::healthHelpId(m_items[index].message);
}

int SystemHealthListModel::Private::priority(int index) const
{
    return priority(m_items[index].message);
}

bool SystemHealthListModel::Private::locked(int index) const
{
    return QnSystemHealth::isMessageLocked(m_items[index].message);
}

bool SystemHealthListModel::Private::isCloseable(int index) const
{
    return m_items[index].message != QnSystemHealth::DefaultCameraPasswords;
}

CommandActionPtr SystemHealthListModel::Private::commandAction(int index) const
{
    if (m_items[index].message != QnSystemHealth::DefaultCameraPasswords)
        return {};

    auto action = CommandActionPtr(new CommandAction(tr("Set Passwords")));
    connect(action.data(), &QAction::triggered, this,
        [this]()
        {
            const auto state = context()->instance<SystemHealthState>();
            const auto parameters = action::Parameters(m_camerasWithDefaultPassword)
                .withArgument(Qn::ForceShowCamerasList, true);

            menu()->triggerIfPossible(action::ChangeDefaultCameraPasswordAction, parameters);
        });

    return action;
}

action::IDType SystemHealthListModel::Private::action(int index) const
{
    switch (m_items[index].message)
    {
        case QnSystemHealth::EmailIsEmpty:
            return action::UserSettingsAction;

        case QnSystemHealth::NoLicenses:
            return action::PreferencesLicensesTabAction;

        case QnSystemHealth::SmtpIsNotSet:
            return action::PreferencesSmtpTabAction;

        case QnSystemHealth::UsersEmailIsEmpty:
            return action::UserSettingsAction;

        case QnSystemHealth::EmailSendError:
            return action::PreferencesSmtpTabAction;
            break;

        case QnSystemHealth::StoragesNotConfigured:
        case QnSystemHealth::ArchiveRebuildFinished:
        case QnSystemHealth::ArchiveRebuildCanceled:
            return action::ServerSettingsAction;

         case QnSystemHealth::NoInternetForTimeSync:
            return action::SystemAdministrationAction;

        case QnSystemHealth::CloudPromo:
            return action::PreferencesCloudTabAction;

        default:
            return action::NoAction;
    }
}

action::Parameters SystemHealthListModel::Private::parameters(int index) const
{
    switch (m_items[index].message)
    {
        case QnSystemHealth::EmailIsEmpty:
            return action::Parameters(context()->user())
                .withArgument(Qn::FocusElementRole, lit("email"))
                .withArgument(Qn::FocusTabRole, QnUserSettingsDialog::SettingsPage);

        case QnSystemHealth::UsersEmailIsEmpty:
        {
            const auto user = m_usersWithInvalidEmail.empty()
                ? QnUserResourcePtr()
                : m_usersWithInvalidEmail.front();

            return action::Parameters(user)
                .withArgument(Qn::FocusElementRole, lit("email"))
                .withArgument(Qn::FocusTabRole, QnUserSettingsDialog::SettingsPage);
        }

        case QnSystemHealth::NoInternetForTimeSync:
            return action::Parameters()
                .withArgument(Qn::FocusElementRole, lit("syncWithInternet"))
                .withArgument(Qn::FocusTabRole,
                    int(QnSystemAdministrationDialog::TimeServerSelection));

        case QnSystemHealth::StoragesNotConfigured:
        case QnSystemHealth::ArchiveRebuildFinished:
        case QnSystemHealth::ArchiveRebuildCanceled:
            return action::Parameters(m_items[index].resource)
                .withArgument(Qn::FocusTabRole, QnServerSettingsDialog::StorageManagmentPage);

        default:
            return action::Parameters();
    }
}

void SystemHealthListModel::Private::addItem(
    QnSystemHealth::MessageType message,
    const QVariant& params)
{
    if (!QnSystemHealth::isMessageVisible(message))
        return;

    if (QnSystemHealth::isMessageVisibleInSettings(message))
    {
        if (!qnSettings->popupSystemHealth().contains(message))
            return; //< Not allowed by filter.
    }

    QnResourcePtr resource;
    if (params.canConvert<QnResourcePtr>())
        resource = params.value<QnResourcePtr>();

    vms::event::AbstractActionPtr action;
    if (params.canConvert<vms::event::AbstractActionPtr>())
    {
        action = params.value<vms::event::AbstractActionPtr>();
        if (action)
        {
            const auto runtimeParameters = action->getRuntimeParams();
            resource = runtimeParameters.metadata.cameraRefs.empty()
                ? resourcePool()->getResourceById(runtimeParameters.eventResourceId)
                : static_cast<QnResourcePtr>(camera_id_helper::findCameraByFlexibleId(resourcePool(),
                    runtimeParameters.metadata.cameraRefs[0]));
        }
    }

    Item item(message, resource);
    item.serverData = action;

    const auto position = std::lower_bound(m_items.cbegin(), m_items.cend(), item);
    const auto index = std::distance(m_items.cbegin(), position);

    if (position != m_items.end() && *position == item)
        return; //< Item already exists.

    updateCachedData(message);

    // New item.
    ScopedInsertRows insertRows(q, index, index);
    m_items.insert(position, item);
}

void SystemHealthListModel::Private::removeItem(
    QnSystemHealth::MessageType message,
    const QVariant& params)
{
    QnResourcePtr resource;
    if (params.canConvert<QnResourcePtr>())
        resource = params.value<QnResourcePtr>();

    if (resource)
    {
        const Item item(message, resource);
        const auto position = std::lower_bound(m_items.cbegin(), m_items.cend(), item);
        if (position != m_items.cend() && *position == item)
            q->removeRows(std::distance(m_items.cbegin(), position), 1);
    }
    else
    {
        const auto range = std::equal_range(m_items.cbegin(), m_items.cend(), message);
        const auto count = std::distance(range.first, range.second);
        if (count > 0)
            q->removeRows(std::distance(m_items.cbegin(), range.first), count);
    }
}

void SystemHealthListModel::Private::toggleItem(
    QnSystemHealth::MessageType message, bool isOn)
{
    if (isOn)
        addItem(message, {});
    else
        removeItem(message, {});
}

void SystemHealthListModel::Private::updateItem(QnSystemHealth::MessageType message)
{
    Item item(message, {});
    const auto position = std::lower_bound(m_items.cbegin(), m_items.cend(), item);

    if (position == m_items.end() || *position != item)
        return; //< Item does not exist.

    updateCachedData(message);

    const auto index = q->index(std::distance(m_items.cbegin(), position));
    emit q->dataChanged(index, index);
}

void SystemHealthListModel::Private::updateCachedData(QnSystemHealth::MessageType message)
{
    const auto systemHealthState = context()->instance<SystemHealthState>();
    switch (message)
    {
        case QnSystemHealth::MessageType::UsersEmailIsEmpty:
        {
            m_usersWithInvalidEmail = systemHealthState->data(
                QnSystemHealth::MessageType::UsersEmailIsEmpty).value<QnUserResourceList>();

            std::sort(m_usersWithInvalidEmail.begin(), m_usersWithInvalidEmail.end(),
                [](const QnUserResourcePtr& left, const QnUserResourcePtr& right)
                {
                    return nx::utils::naturalStringCompare(
                        left->getName(), right->getName(), Qt::CaseInsensitive) < 0;
                });

            break;
        }

        case QnSystemHealth::MessageType::DefaultCameraPasswords:
        {
            m_camerasWithDefaultPassword =
                systemHealthState->data(QnSystemHealth::MessageType::DefaultCameraPasswords)
                    .value<QnVirtualCameraResourceList>();
            break;
        }

        default:
            break;
    }
}

void SystemHealthListModel::Private::clear()
{
    if (m_items.empty())
        return;

    ScopedReset reset(q);
    m_items.clear();
}

void SystemHealthListModel::Private::remove(int first, int count)
{
    ScopedRemoveRows removeRows(q,  first, first + count - 1);
    m_items.erase(m_items.begin() + first, m_items.begin() + (first + count));
}

int SystemHealthListModel::Private::priority(QnSystemHealth::MessageType message)
{
    // Custom priorities from higher to lower.
    enum CustomPriority
    {
        kCloudPromoPriority,
        kDefaultCameraPasswordsPriority,
    };

    const auto priorityValue =
        [](CustomPriority custom) { return std::numeric_limits<int>::max() - custom; };

    switch (message)
    {
        case QnSystemHealth::CloudPromo:
            return priorityValue(kCloudPromoPriority);

        case QnSystemHealth::DefaultCameraPasswords:
            return priorityValue(kDefaultCameraPasswordsPriority);

        default:
            return int(QnNotificationLevel::valueOf(message));
    }
}

QPixmap SystemHealthListModel::Private::pixmap(QnSystemHealth::MessageType message)
{
    switch (QnNotificationLevel::valueOf(message))
    {
        case QnNotificationLevel::Value::CriticalNotification:
            return qnSkin->pixmap("events/alert_red.png");

        case QnNotificationLevel::Value::ImportantNotification:
            return qnSkin->pixmap("events/alert_yellow.png");

        case QnNotificationLevel::Value::SuccessNotification:
            return qnSkin->pixmap("events/success_mark.png");

        default:
            break;
    }

    switch (message)
    {
        case QnSystemHealth::SmtpIsNotSet:
        case QnSystemHealth::EmailIsEmpty:
        case QnSystemHealth::UsersEmailIsEmpty:
        case QnSystemHealth::EmailSendError:
            return qnSkin->pixmap("events/email.png");

        case QnSystemHealth::NoLicenses:
            return qnSkin->pixmap("events/license.png");

        case QnSystemHealth::SystemIsReadOnly:
            return qnSkin->pixmap("tree/system.png");

        case QnSystemHealth::StoragesNotConfigured:
        case QnSystemHealth::ArchiveRebuildFinished:
        case QnSystemHealth::ArchiveRebuildCanceled:
            return qnSkin->pixmap("events/storage.png");

        case QnSystemHealth::CloudPromo:
            return qnSkin->pixmap("cloud/cloud_20.png");

        default:
            return QPixmap();
    }
}

} // namespace nx::vms::client::desktop
