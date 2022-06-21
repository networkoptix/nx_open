// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_health_list_model_p.h"

#include <chrono>
#include <limits>

#include <QtCore/QCollator>

#include <client/client_settings.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <health/system_health_strings_helper.h>
#include <nx/utils/string.h>
#include <nx/vms/client/core/resource/session_resources_signal_listener.h>
#include <nx/vms/client/desktop/resource_properties/camera/camera_settings_tab.h>
#include <nx/vms/client/desktop/system_health/system_health_state.h>
#include <nx/vms/client/desktop/ui/actions/action.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_health/system_health_data_helper.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/strings_helper.h>
#include <ui/common/notification_levels.h>
// TODO: #vkutin Dialogs are included just for tab identifiers. Needs refactoring to avoid it.
#include <ui/dialogs/resource_properties/server_settings_dialog.h>
#include <ui/dialogs/resource_properties/user_settings_dialog.h>
#include <ui/dialogs/system_administration_dialog.h>
#include <ui/help/business_help.h>
#include <ui/workbench/handlers/workbench_notifications_handler.h>
#include <ui/workbench/workbench_context.h>

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
    m_helper(new vms::event::StringsHelper(systemContext())),
    m_popupSystemHealthFilter(qnSettings->popupSystemHealth())
{
    // Handle system health state.

    const auto systemHealthState = context()->instance<SystemHealthState>();
    connect(systemHealthState, &SystemHealthState::stateChanged, this, &Private::toggleItem);
    connect(systemHealthState, &SystemHealthState::dataChanged, this, &Private::updateItem);

    for (const auto index: QnSystemHealth::allVisibleMessageTypes())
    {
        if (systemHealthState->state(index))
            doAddItem(index, {}, true /*initial*/);
    }

    auto usernameChangesListener = new core::SessionResourcesSignalListener<QnUserResource>(this);
    usernameChangesListener->addOnSignalHandler(
        &QnUserResource::nameChanged,
        [this](const QnUserResourcePtr& user)
        {
            if (m_usersWithInvalidEmail.contains(user))
                updateItem(QnSystemHealth::MessageType::UsersEmailIsEmpty);
        });
    usernameChangesListener->start();

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

    auto cachesCleaner = new core::SessionResourcesSignalListener<QnResource>(this);
    cachesCleaner->setOnRemovedHandler(
        [this](const QnResourceList& resources)
        {
            for (const auto& resource: resources)
            {
                if (auto user = resource.dynamicCast<QnUserResource>())
                    m_usersWithInvalidEmail.removeAll(user);
                if (auto camera = resource.dynamicCast<QnVirtualCameraResource>())
                {
                    m_camerasWithDefaultPassword.removeAll(camera);
                    m_camerasWithInvalidSchedule.removeAll(camera);
                }
                if (auto server = resource.dynamicCast<QnMediaServerResource>())
                {
                    m_serversWithoutStorages.removeAll(server);
                    m_serversWithoutBackupStorages.removeAll(server);
                }
            }
        });
    cachesCleaner->start();
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

        case QnSystemHealth::cameraRecordingScheduleIsInvalid:
        {
            return tr("Recording schedule is invalid for %n cameras", "",
                m_camerasWithInvalidSchedule.size());
        }

        case QnSystemHealth::replacedDeviceDiscovered:
        {
            using namespace nx::vms::common;

            static const auto kDeviceNameColor = ColorTheme::instance()->color("light10");

            const auto attributes = item.serverData->getRuntimeParams().attributes;

            const auto caption = tr("Replaced camera discovered");

            auto result = html::paragraph(caption);
            if (const auto discoveredDeviceModel =
                system_health::getDeviceModelFromAttributes(attributes))
            {
                result += html::paragraph(html::colored(*discoveredDeviceModel, kDeviceNameColor));
            }

            return result;
        }

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
                QnResourceDisplayInfo(item.resource).toString(qnSettings->resourceInfoLevel()));
    }
}

QnResourceList SystemHealthListModel::Private::displayedResourceList(int index) const
{
    const auto& item = m_items[index];
    switch (item.message)
    {
        case QnSystemHealth::UsersEmailIsEmpty:
            return m_usersWithInvalidEmail;

        case QnSystemHealth::StoragesNotConfigured:
            return m_serversWithoutStorages;

        case QnSystemHealth::backupStoragesNotConfigured:
            return m_serversWithoutBackupStorages;

        case QnSystemHealth::cameraRecordingScheduleIsInvalid:
            return m_camerasWithInvalidSchedule;

        default:
            return {};
    }
}

QString SystemHealthListModel::Private::toolTip(int index) const
{
    const auto& item = m_items[index];

    if (item.message == QnSystemHealth::cameraRecordingScheduleIsInvalid)
    {
        using namespace nx::vms::common::html;

        QStringList result;
        result << tr("Recording schedule on some cameras contains recording modes that are not "
            "supported.");
        result << ""; //< Additional line break by design.
        for (const auto& camera: m_camerasWithInvalidSchedule)
        {
            result << colored(camera->getName(), colorTheme()->color("light8"))
                + " "
                + colored(camera->getHostAddress(), colorTheme()->color("light16"));
        }
        return result.join(kLineBreak);
    }

    if (item.message == QnSystemHealth::replacedDeviceDiscovered)
    {
        using namespace nx::vms::common;

        const auto camera = item.resource.dynamicCast<QnVirtualCameraResource>();
        if (!NX_ASSERT(camera, "Invalid notification parameter"))
            return {};

        const auto attributes = item.serverData->getRuntimeParams().attributes;
        const auto discoveredDeviceUrl = system_health::getDeviceUrlFromAttributes(attributes);
        const auto discoveredDeviceModel = system_health::getDeviceModelFromAttributes(attributes);

        QStringList discoveredCameraTextLines;
        if (discoveredDeviceModel)
            discoveredCameraTextLines.append(html::bold(*discoveredDeviceModel));
        if (discoveredDeviceUrl)
            discoveredCameraTextLines.append(discoveredDeviceUrl->host());
        const auto discoveredCameraText = discoveredCameraTextLines.join(QChar::Space);

        const auto replacementCameraText = QStringList({
            html::bold(camera->getModel()),
            QnResourceDisplayInfo(camera).host()})
                .join(QChar::Space);

        QStringList tooltipLines;

        tooltipLines.append(tr("Camera %1 has been replaced by %2.")
            .arg(discoveredCameraText)
            .arg(replacementCameraText));

        tooltipLines.append(
            tr("Click on the \"Undo Replace\" button to continue using two devices."));

        return tooltipLines.join("<br>");
    }

    return QnSystemHealthStringsHelper::messageTooltip(item.message,
        QnResourceDisplayInfo(item.resource).toString(qnSettings->resourceInfoLevel()));
}

QString SystemHealthListModel::Private::decorationPath(int index) const
{
    return decorationPath(m_items[index].message);
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
    const auto& item = m_items[index];

    switch (item.message)
    {
        case QnSystemHealth::DefaultCameraPasswords:
        {
            const auto action = CommandActionPtr(new CommandAction(tr("Set Passwords")));
            connect(action.data(), &QAction::triggered, this,
                [this]
                {
                    const auto state = context()->instance<SystemHealthState>();
                    const auto parameters = action::Parameters(m_camerasWithDefaultPassword)
                        .withArgument(Qn::ForceShowCamerasList, true);

                    menu()->triggerIfPossible(
                        action::ChangeDefaultCameraPasswordAction, parameters);
                });

            return action;
        }

        case QnSystemHealth::replacedDeviceDiscovered:
        {
            const auto action = CommandActionPtr(new CommandAction(tr("Undo Replace")));
            connect(action.data(), &QAction::triggered, this,
                [this, resource = item.resource]
                {
                    const auto parameters = action::Parameters(resource);
                    menu()->triggerIfPossible(action::UndoReplaceCameraAction, parameters);
                });

            return action;
        }

        default:
            return {};
    }
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

        case QnSystemHealth::backupStoragesNotConfigured:
        case QnSystemHealth::StoragesNotConfigured:
        case QnSystemHealth::ArchiveRebuildFinished:
        case QnSystemHealth::ArchiveRebuildCanceled:
            return action::ServerSettingsAction;

         case QnSystemHealth::NoInternetForTimeSync:
            return action::SystemAdministrationAction;

        case QnSystemHealth::CloudPromo:
            return action::PreferencesCloudTabAction;

        case QnSystemHealth::cameraRecordingScheduleIsInvalid:
            return action::CameraSettingsAction;

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
        {
            if (!NX_ASSERT(!m_serversWithoutStorages.empty()))
                return {};

            return action::Parameters(m_serversWithoutStorages.front())
                .withArgument(Qn::FocusTabRole, QnServerSettingsDialog::StorageManagmentPage);
        }

        case QnSystemHealth::backupStoragesNotConfigured:
        {
            if (!NX_ASSERT(!m_serversWithoutBackupStorages.empty()))
                return {};

            return action::Parameters(m_serversWithoutBackupStorages.front())
                .withArgument(Qn::FocusTabRole, QnServerSettingsDialog::StorageManagmentPage);
        }

        case QnSystemHealth::ArchiveRebuildFinished:
        case QnSystemHealth::ArchiveRebuildCanceled:
        {
            return action::Parameters(m_items[index].resource)
                .withArgument(Qn::FocusTabRole, QnServerSettingsDialog::StorageManagmentPage);
        }

        case QnSystemHealth::cameraRecordingScheduleIsInvalid:
        {
            QnVirtualCameraResourcePtr camera;
            if (NX_ASSERT(!m_camerasWithInvalidSchedule.empty()))
                camera = m_camerasWithInvalidSchedule.front();

            return action::Parameters(camera)
                .withArgument(Qn::FocusTabRole, CameraSettingsTab::recording);
        }

        default:
            return action::Parameters();
    }
}

QnSystemHealth::MessageType SystemHealthListModel::Private::messageType(int index) const
{
    const auto& item = m_items[index];
    return item.message;
}

void SystemHealthListModel::Private::addItem(
    QnSystemHealth::MessageType message, const QVariant& params)
{
    doAddItem(message, params, false /*initial*/);
}

void SystemHealthListModel::Private::doAddItem(
    QnSystemHealth::MessageType message, const QVariant& params, bool initial)
{
    NX_VERBOSE(this, "Adding a system health message %1", message);

    if (!QnSystemHealth::isMessageVisible(message))
    {
        NX_VERBOSE(this, "The message %1 is not visible", message);
        return;
    }

    if (QnSystemHealth::isMessageVisibleInSettings(message))
    {
        if (!qnSettings->popupSystemHealth().contains(message))
        {
            NX_VERBOSE(this, "The message %1 is not allowed by the filter", message);
            return; //< Not allowed by filter.
        }
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
    ScopedInsertRows insertRows(q, index, index, !initial);
    m_items.insert(position, item);
}

void SystemHealthListModel::Private::removeItem(
    QnSystemHealth::MessageType message,
    const QVariant& params)
{
    NX_VERBOSE(this, "Removing a system health message %1", toString(message));

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

void SystemHealthListModel::Private::toggleItem(QnSystemHealth::MessageType message, bool isOn)
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
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);

    const auto lessResourceByName =
        [&collator](const QnResourcePtr& left, const QnResourcePtr& right)
        {
            return collator.compare(left->getName(), right->getName()) < 0;
        };

    const auto systemHealthState = context()->instance<SystemHealthState>();
    switch (message)
    {
        case QnSystemHealth::MessageType::UsersEmailIsEmpty:
        {
            m_usersWithInvalidEmail = systemHealthState->data(
                QnSystemHealth::MessageType::UsersEmailIsEmpty).value<QnUserResourceList>();
            std::sort(m_usersWithInvalidEmail.begin(), m_usersWithInvalidEmail.end(),
                lessResourceByName);
            break;
        }

        case QnSystemHealth::MessageType::DefaultCameraPasswords:
        {
            m_camerasWithDefaultPassword =
                systemHealthState->data(QnSystemHealth::MessageType::DefaultCameraPasswords)
                    .value<QnVirtualCameraResourceList>();
            break;
        }

        case QnSystemHealth::MessageType::StoragesNotConfigured:
        {
            m_serversWithoutStorages =
                systemHealthState->data(QnSystemHealth::MessageType::StoragesNotConfigured)
                    .value<QnMediaServerResourceList>();
            std::sort(m_serversWithoutStorages.begin(), m_serversWithoutStorages.end(),
                lessResourceByName);
            break;
        }
        case QnSystemHealth::MessageType::backupStoragesNotConfigured:
        {
            m_serversWithoutBackupStorages =
                systemHealthState->data(QnSystemHealth::MessageType::backupStoragesNotConfigured)
                .value<QnMediaServerResourceList>();
            std::sort(m_serversWithoutBackupStorages.begin(), m_serversWithoutBackupStorages.end(),
                lessResourceByName);
            break;
        }
        case QnSystemHealth::MessageType::cameraRecordingScheduleIsInvalid:
        {
            m_camerasWithInvalidSchedule = systemHealthState->data(
                QnSystemHealth::MessageType::cameraRecordingScheduleIsInvalid)
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
        kInvalidRecordingSchedulePriority,
    };

    const auto priorityValue =
        [](CustomPriority custom) { return std::numeric_limits<int>::max() - custom; };

    switch (message)
    {
        case QnSystemHealth::CloudPromo:
            return priorityValue(kCloudPromoPriority);

        case QnSystemHealth::DefaultCameraPasswords:
            return priorityValue(kDefaultCameraPasswordsPriority);

        case QnSystemHealth::MessageType::cameraRecordingScheduleIsInvalid:
            return priorityValue(kInvalidRecordingSchedulePriority);

        default:
            return int(QnNotificationLevel::valueOf(message));
    }
}

QString SystemHealthListModel::Private::decorationPath(QnSystemHealth::MessageType message)
{
    switch (QnNotificationLevel::valueOf(message))
    {
        case QnNotificationLevel::Value::CriticalNotification:
            return "events/alert_red.png";

        case QnNotificationLevel::Value::ImportantNotification:
            return "events/alert_yellow.png";

        case QnNotificationLevel::Value::SuccessNotification:
            return "events/success_mark.png";

        default:
            break;
    }

    switch (message)
    {
        case QnSystemHealth::SmtpIsNotSet:
        case QnSystemHealth::EmailIsEmpty:
        case QnSystemHealth::UsersEmailIsEmpty:
        case QnSystemHealth::EmailSendError:
            return "events/email.png";

        case QnSystemHealth::NoLicenses:
            return "events/license.png";

        case QnSystemHealth::backupStoragesNotConfigured:
        case QnSystemHealth::StoragesNotConfigured:
        case QnSystemHealth::ArchiveRebuildFinished:
        case QnSystemHealth::ArchiveRebuildCanceled:
            return "events/storage.png";

        case QnSystemHealth::CloudPromo:
            return "cloud/cloud_20.png";

        default:
            return QString();
    }
}

} // namespace nx::vms::client::desktop
