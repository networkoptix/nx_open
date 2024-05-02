// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_health_list_model_p.h"

#include <chrono>
#include <limits>

#include <QtCore/QCollator>

#include <client/client_settings.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <health/system_health_strings_helper.h>
#include <nx/utils/metatypes.h>
#include <nx/utils/string.h>
#include <nx/vms/client/core/resource/session_resources_signal_listener.h>
#include <nx/vms/client/desktop/common/utils/progress_state.h>
#include <nx/vms/client/desktop/resource_properties/camera/camera_settings_tab.h>
#include <nx/vms/client/desktop/system_context.h>
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
// SystemHealthListModel::Private::Item definition.

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
// SystemHealthListModel::Private definition.

SystemHealthListModel::Private::Private(SystemHealthListModel* q):
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
            if (getResourceSet(QnSystemHealth::UsersEmailIsEmpty).contains(user))
                updateItem(QnSystemHealth::UsersEmailIsEmpty);
        });
    usernameChangesListener->start();

    const auto cameraNameChangesListener =
        new core::SessionResourcesSignalListener<QnVirtualCameraResource>(this);
    cameraNameChangesListener->addOnSignalHandler(
        &QnVirtualCameraResource::nameChanged,
        [this](const QnVirtualCameraResourcePtr& camera)
        {
            if (getResourceSet(QnSystemHealth::cameraRecordingScheduleIsInvalid).contains(camera))
                updateItem(QnSystemHealth::cameraRecordingScheduleIsInvalid);
        });
    cameraNameChangesListener->start();

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
    const QString resourceName = item.resource ? item.resource->getName() : "";

    switch (item.message)
    {
        case QnSystemHealth::UsersEmailIsEmpty:
            return tr("Email address is not set for %n users", "",
                getResourceSet(item.message).size());

        case QnSystemHealth::backupStoragesNotConfigured:
            return tr("Backup storage is not configured on %n Servers", "",
                getResourceSet(item.message).size());

        case QnSystemHealth::cameraRecordingScheduleIsInvalid:
            return tr("Recording schedule is invalid for %n cameras", "",
                getResourceSet(item.message).size());

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

        case QnSystemHealth::RemoteArchiveSyncAvailable:
            return tr("On-device recordings were found");

        case QnSystemHealth::RemoteArchiveSyncProgress:
            return tr("Import in progress...");

        case QnSystemHealth::RemoteArchiveSyncFinished:
            return tr("Import archive from %1 completed").arg(resourceName);

        case QnSystemHealth::RemoteArchiveSyncError:
            return tr("Import archive from %1 failed").arg(resourceName);

        case QnSystemHealth::RemoteArchiveSyncStopSchedule:
        case QnSystemHealth::RemoteArchiveSyncStopAutoMode:
            return tr("Import archive from %1 stopped").arg(resourceName);

        case QnSystemHealth::metadataStorageNotSet:
            return tr("Storage for analytics data is not set on %n Servers", "",
                getResourceSet(item.message).size());

        case QnSystemHealth::metadataOnSystemStorage:
            return tr("System storage is used for analytics data on %n Servers", "",
                getResourceSet(item.message).size());

        default:
            return QnSystemHealthStringsHelper::messageText(item.message,
                QnResourceDisplayInfo(item.resource).toString(qnSettings->resourceInfoLevel()));
    }
}

QnResourceList SystemHealthListModel::Private::displayedResourceList(int index) const
{
    const auto message = messageType(index);
    switch (message)
    {
        case QnSystemHealth::UsersEmailIsEmpty:
        case QnSystemHealth::StoragesNotConfigured:
        case QnSystemHealth::backupStoragesNotConfigured:
        case QnSystemHealth::cameraRecordingScheduleIsInvalid:
        case QnSystemHealth::metadataStorageNotSet:
        case QnSystemHealth::metadataOnSystemStorage:
            return getSortedResourceList(message);

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

        const auto cameraResources =
            getSortedResourceList(QnSystemHealth::cameraRecordingScheduleIsInvalid);

        for (const auto& cameraResource: cameraResources)
        {
            const auto camera = cameraResource.dynamicCast<QnVirtualCameraResource>();
            if (!NX_ASSERT(camera))
                continue;

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

QString SystemHealthListModel::Private::description(int index) const
{
    const auto& item = m_items[index];
    const auto camera = item.resource.dynamicCast<QnVirtualCameraResource>();
    const QString resourceName = item.resource ? item.resource->getName() : "";

    switch (item.message)
    {
        case QnSystemHealth::RemoteArchiveSyncAvailable:
            return QnDeviceDependentStrings::getNameFromSet(
                resourcePool(),
                QnCameraDeviceStringSet(
                    tr("Not imported archive found on device %1").arg(resourceName),
                    tr("Not imported archive found on camera %1").arg(resourceName)),
                camera);

        case QnSystemHealth::RemoteArchiveSyncProgress:
            return tr("Import archive from %1").arg(resourceName);

        case QnSystemHealth::RemoteArchiveSyncStopSchedule:
            return tr("The archive stream settings have been changed by the user");

        case QnSystemHealth::RemoteArchiveSyncStopAutoMode:
            return tr("The recording settings have been changed by the user");

        default:
            return {};
    }
}

QVariant SystemHealthListModel::Private::progress(int index) const
{
    const auto& item = m_items[index];
    switch (item.message)
    {
        case QnSystemHealth::RemoteArchiveSyncProgress:
            return QVariant::fromValue(
                ProgressState(item.serverData->getRuntimeParams().progress));

        default:
            return {};
    }
}

QVariant SystemHealthListModel::Private::timestamp(int index) const
{
    const auto& item = m_items[index];
    switch (item.message)
    {
        case QnSystemHealth::RemoteArchiveSyncFinished:
        case QnSystemHealth::RemoteArchiveSyncError:
        case QnSystemHealth::RemoteArchiveSyncStopSchedule:
        case QnSystemHealth::RemoteArchiveSyncStopAutoMode:
            return QVariant::fromValue(std::chrono::microseconds(
                item.serverData->getRuntimeParams().eventTimestampUsec));

        default:
            return {};
    }
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
    return m_items[index].message != QnSystemHealth::DefaultCameraPasswords
        && m_items[index].message != QnSystemHealth::RemoteArchiveSyncProgress;
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
                    const auto parameters = action::Parameters(
                        getSortedResourceList(QnSystemHealth::DefaultCameraPasswords))
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

        case QnSystemHealth::RemoteArchiveSyncAvailable:
        {
            const auto action = CommandActionPtr(new CommandAction(tr("Export")));
            connect(action.data(), &QAction::triggered, this,
                [this, resource = item.resource]
                {
                    auto camera = resource.dynamicCast<QnSecurityCamResource>();
                    if (NX_ASSERT(camera))
                    {
                        camera->synchronizeRemoteArchiveOnce();
                        camera->savePropertiesAsync();
                    }
                });

            return action;
        }

        default:
            return {};
    }
}

action::IDType SystemHealthListModel::Private::action(int index) const
{
    const auto message = messageType(index);
    switch (message)
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
        case QnSystemHealth::metadataStorageNotSet:
        case QnSystemHealth::metadataOnSystemStorage:
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
    const auto message = messageType(index);
    switch (message)
    {
        case QnSystemHealth::EmailIsEmpty:
            return action::Parameters(context()->user())
                .withArgument(Qn::FocusElementRole, lit("email"))
                .withArgument(Qn::FocusTabRole, QnUserSettingsDialog::SettingsPage);

        case QnSystemHealth::UsersEmailIsEmpty:
        {
            const auto users = getSortedResourceList(message);
            if (!NX_ASSERT(!users.empty()))
                return {};

            return action::Parameters(users.front())
                .withArgument(Qn::FocusElementRole, lit("email"))
                .withArgument(Qn::FocusTabRole, QnUserSettingsDialog::SettingsPage);
        }

        case QnSystemHealth::NoInternetForTimeSync:
            return action::Parameters()
                .withArgument(Qn::FocusElementRole, lit("syncWithInternet"))
                .withArgument(Qn::FocusTabRole,
                    int(QnSystemAdministrationDialog::TimeServerSelection));

        case QnSystemHealth::StoragesNotConfigured:
        case QnSystemHealth::backupStoragesNotConfigured:
        case QnSystemHealth::metadataStorageNotSet:
        case QnSystemHealth::metadataOnSystemStorage:
        {
            const auto servers = getSortedResourceList(message);
            if (!NX_ASSERT(!servers.empty()))
                return {};

            return action::Parameters(servers.front())
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
            const auto cameras = getSortedResourceList(message);
            if (!NX_ASSERT(!cameras.empty()))
                return {};

            return action::Parameters(cameras.front())
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

QSet<QnResourcePtr> SystemHealthListModel::Private::getResourceSet(
    QnSystemHealth::MessageType message) const
{
    const auto systemHealthState = context()->instance<SystemHealthState>();
    return systemHealthState->data(message).value<QSet<QnResourcePtr>>();
}

QnResourceList SystemHealthListModel::Private::getSortedResourceList(
    QnSystemHealth::MessageType message) const
{
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);

    const auto resourceNameLessThan =
        [collator = std::move(collator)](const QnResourcePtr& lhs, const QnResourcePtr& rhs)
        {
            return collator.compare(lhs->getName(), rhs->getName()) < 0;
        };

    const auto resourceSet = getResourceSet(message);
    QnResourceList result(resourceSet.cbegin(), resourceSet.cend());
    std::sort(result.begin(), result.end(), resourceNameLessThan);
    return result;
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
            const auto sourceResources = event::sourceResources(
                action->getRuntimeParams(),
                resourcePool());
            if (sourceResources && !sourceResources->isEmpty())
                resource = sourceResources->first();
        }
    }

    if (message == QnSystemHealth::MessageType::RemoteArchiveSyncFinished
        || message == QnSystemHealth::MessageType::RemoteArchiveSyncError
        || message == QnSystemHealth::MessageType::RemoteArchiveSyncStopSchedule
        || message == QnSystemHealth::MessageType::RemoteArchiveSyncStopAutoMode)
    {
        removeItemForResource(QnSystemHealth::MessageType::RemoteArchiveSyncProgress, resource);
    }

    if (message == QnSystemHealth::MessageType::RemoteArchiveSyncProgress
        || message == QnSystemHealth::MessageType::RemoteArchiveSyncFinished
        || message == QnSystemHealth::MessageType::RemoteArchiveSyncError
        || message == QnSystemHealth::MessageType::RemoteArchiveSyncStopSchedule
        || message == QnSystemHealth::MessageType::RemoteArchiveSyncStopAutoMode)
    {
        removeItemForResource(QnSystemHealth::MessageType::RemoteArchiveSyncAvailable, resource);
    }

    Item item(message, resource);
    item.serverData = action;

    auto position = std::lower_bound(m_items.begin(), m_items.end(), item);
    const auto index = std::distance(m_items.begin(), position);

    if (position != m_items.end() && *position == item)
    {
        if (message == QnSystemHealth::MessageType::RemoteArchiveSyncProgress)
        {
            *position = item;
            q->dataChanged(q->index(index), q->index(index), {
                    Qt::DisplayRole,
                    Qn::DescriptionTextRole,
                    Qn::ProgressValueRole
                });
        }
        return; //< Item already exists.
    }

    {
        // New item.
        ScopedInsertRows insertRows(q, index, index, !initial);
        m_items.insert(position, item);
    }
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
        removeItemForResource(message, resource);
    }
    else
    {
        NX_VERBOSE(this, "Removing a system health message %1", toString(message));

        const auto range = std::equal_range(m_items.cbegin(), m_items.cend(), message);
        const auto count = std::distance(range.first, range.second);
        if (count > 0)
            q->removeRows(std::distance(m_items.cbegin(), range.first), count);
    }
}

void SystemHealthListModel::Private::removeItemForResource(
    QnSystemHealth::MessageType message,
    const QnResourcePtr& resource)
{
    NX_VERBOSE(this, "Removing a system health message %1", toString(message));

    const Item item(message, resource);
    const auto position = std::lower_bound(m_items.cbegin(), m_items.cend(), item);
    if (position != m_items.cend() && *position == item)
        q->removeRows(std::distance(m_items.cbegin(), position), 1);
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

    const auto index = q->index(std::distance(m_items.cbegin(), position));
    emit q->dataChanged(index, index);
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
        case QnSystemHealth::metadataOnSystemStorage: //< TODO: #vbreus Probably should be changed to the 'analytics' icon.
            return "events/storage.png";

        case QnSystemHealth::CloudPromo:
            return "cloud/cloud_20.png";

        case QnSystemHealth::RemoteArchiveSyncAvailable:
            return "events/sd_card.png";

        default:
            return QString();
    }
}

} // namespace nx::vms::client::desktop
