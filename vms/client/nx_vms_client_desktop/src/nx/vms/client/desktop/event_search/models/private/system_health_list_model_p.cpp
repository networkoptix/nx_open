// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_health_list_model_p.h"

#include <chrono>
#include <limits>

#include <QtCore/QCollator>

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <health/system_health_strings_helper.h>
#include <nx/utils/metatypes.h>
#include <nx/utils/string.h>
#include <nx/vms/client/core/resource/session_resources_signal_listener.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/utils/progress_state.h>
#include <nx/vms/client/desktop/help/rules_help.h>
#include <nx/vms/client/desktop/resource_properties/camera/camera_settings_tab.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_health/system_health_state.h>
#include <nx/vms/client/desktop/system_update/update_contents.h>
#include <nx/vms/client/desktop/system_update/workbench_update_watcher.h>
#include <nx/vms/client/desktop/ui/actions/action.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/saas/saas_utils.h>
#include <nx/vms/common/system_health/system_health_data_helper.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/strings_helper.h>
#include <nx/vms/time/formatter.h>
#include <ui/common/notification_levels.h>
#include <ui/workbench/handlers/workbench_notifications_handler.h>
#include <ui/workbench/workbench_context.h>

// TODO: #vkutin Dialogs are included just for tab identifiers. Needs refactoring to avoid it.
#include <nx/vms/client/desktop/system_administration/dialogs/user_settings_dialog.h>
#include <ui/dialogs/resource_properties/server_settings_dialog.h>
#include <ui/dialogs/system_administration_dialog.h>

namespace {

using namespace nx::vms::common::system_health;

std::optional<QString> overusedSaasServiceType(MessageType messageType)
{
    switch (messageType)
    {
        case MessageType::saasLocalRecordingServicesOverused:
            return nx::vms::api::SaasService::kLocalRecordingServiceType;

        case MessageType::saasCloudStorageServicesOverused:
            return nx::vms::api::SaasService::kCloudRecordingType;

        case MessageType::saasIntegrationServicesOverused:
            return nx::vms::api::SaasService::kAnalyticsIntegrationServiceType;

        default:
            return {};
    }
}

bool messageIsSupported(MessageType message)
{
    return message != MessageType::showIntercomInformer
        && message != MessageType::showMissedCallInformer;
}

} // namespace

namespace nx::vms::client::desktop {

using namespace ui;

//-------------------------------------------------------------------------------------------------
// SystemHealthListModel::Private::Item definition.

SystemHealthListModel::Private::Item::Item(
    MessageType message,
    const QnResourcePtr& resource)
    :
    message(message),
    resource(resource)
{
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
    m_helper(new nx::vms::event::StringsHelper(systemContext())),
    m_popupSystemHealthFilter(appContext()->localSettings()->popupSystemHealth())
{
    // Handle system health state.

    const auto systemHealthState = context()->instance<SystemHealthState>();
    connect(systemHealthState, &SystemHealthState::stateChanged, this, &Private::toggleItem);
    connect(systemHealthState, &SystemHealthState::dataChanged, this, &Private::updateItem);

    for (const auto index: nx::vms::common::system_health::allVisibleMessageTypes())
    {
        if (systemHealthState->state(index))
            doAddItem(index, {}, true /*initial*/);
    }

    auto usernameChangesListener = new core::SessionResourcesSignalListener<QnUserResource>(this);
    usernameChangesListener->addOnSignalHandler(
        &QnUserResource::nameChanged,
        [this](const QnUserResourcePtr& user)
        {
            if (getResourceSet(MessageType::usersEmailIsEmpty).contains(user))
                updateItem(MessageType::usersEmailIsEmpty);
        });
    usernameChangesListener->start();

    const auto cameraNameChangesListener =
        new core::SessionResourcesSignalListener<QnVirtualCameraResource>(this);
    cameraNameChangesListener->addOnSignalHandler(
        &QnVirtualCameraResource::nameChanged,
        [this](const QnVirtualCameraResourcePtr& camera)
        {
            if (getResourceSet(MessageType::cameraRecordingScheduleIsInvalid).contains(camera))
                updateItem(MessageType::cameraRecordingScheduleIsInvalid);
        });
    cameraNameChangesListener->start();

    // Handle system health notifications.

    const auto notificationHandler = context()->instance<QnWorkbenchNotificationsHandler>();
    connect(notificationHandler, &QnWorkbenchNotificationsHandler::cleared, this, &Private::clear);

    connect(notificationHandler, &QnWorkbenchNotificationsHandler::systemHealthEventAdded,
        this, &Private::addItem);

    connect(notificationHandler, &QnWorkbenchNotificationsHandler::systemHealthEventRemoved,
        this, &Private::removeItem);

    connect(&appContext()->localSettings()->popupSystemHealth,
        &nx::utils::property_storage::BaseProperty::changed,
        this,
        [this]()
        {
            const auto filter = appContext()->localSettings()->popupSystemHealth();
            const auto systemHealthState = context()->instance<SystemHealthState>();

            for (const auto index: common::system_health::allVisibleMessageTypes())
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

common::system_health::MessageType SystemHealthListModel::Private::message(int index) const
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
        case MessageType::usersEmailIsEmpty:
            return tr("Email address is not set for %n users", "",
                getResourceSet(item.message).size());

        case MessageType::backupStoragesNotConfigured:
            return tr("Backup storage is not configured on %n Servers", "",
                getResourceSet(item.message).size());

        case MessageType::cameraRecordingScheduleIsInvalid:
            return tr("Recording schedule is invalid for %n cameras", "",
                getResourceSet(item.message).size());

        case MessageType::replacedDeviceDiscovered:
        {
            using namespace nx::vms::common;

            static const auto kDeviceNameColor = core::ColorTheme::instance()->color("light10");

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

        case MessageType::remoteArchiveSyncError:
            return tr("Import archive from %1 failed").arg(resourceName);

        case MessageType::metadataStorageNotSet:
            return tr("Storage for analytics data is not set on %n Servers", "",
                getResourceSet(item.message).size());

        case MessageType::metadataOnSystemStorage:
            return tr("System storage is used for analytics data on %n Servers", "",
                getResourceSet(item.message).size());

        default:
            return QnSystemHealthStringsHelper::messageText(item.message,
                QnResourceDisplayInfo(item.resource).toString(
                    appContext()->localSettings()->resourceInfoLevel()));
    }
}

QnResourceList SystemHealthListModel::Private::displayedResourceList(int index) const
{
    const auto message = messageType(index);
    switch (message)
    {
        case MessageType::usersEmailIsEmpty:
        case MessageType::storagesNotConfigured:
        case MessageType::backupStoragesNotConfigured:
        case MessageType::cameraRecordingScheduleIsInvalid:
        case MessageType::metadataStorageNotSet:
        case MessageType::metadataOnSystemStorage:
            return getSortedResourceList(message);

        default:
            return {};
    }
}

QString SystemHealthListModel::Private::toolTip(int index) const
{
    const auto& item = m_items[index];

    if (item.message == MessageType::cameraRecordingScheduleIsInvalid)
    {
        using namespace nx::vms::common::html;

        QStringList result;
        result << tr("Recording schedule on some cameras contains recording modes that are not "
            "supported.");
        result << ""; //< Additional line break by design.

        const auto cameraResources =
            getSortedResourceList(MessageType::cameraRecordingScheduleIsInvalid);

        for (const auto& cameraResource: cameraResources)
        {
            const auto camera = cameraResource.dynamicCast<QnVirtualCameraResource>();
            if (!NX_ASSERT(camera))
                continue;

            result << colored(camera->getName(), core::colorTheme()->color("light8"))
                + " "
                + colored(camera->getHostAddress(), core::colorTheme()->color("light16"));
        }
        return result.join(kLineBreak);
    }

    if (item.message == MessageType::replacedDeviceDiscovered)
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

    if (const auto serviceType = overusedSaasServiceType(item.message))
    {
        const auto serviceManager = systemContext()->saasServiceManager();
        const auto issueExpirationDate =
            serviceManager->serviceStatus(*serviceType).issueExpirationDate.date();
        const auto formatter = nx::vms::time::Formatter::system();

        QStringList tooltipLines;

        tooltipLines.append(
            tr("Add more services or fix overuse by stopping using services for some devices."));

        tooltipLines.append(tr("If no action is taken, required number of services will be "
            "released automatically on %1.").arg(formatter->toString(issueExpirationDate)));

        return tooltipLines.join("<br>");
    }

    if (item.message == MessageType::saasInSuspendedState
        || item.message == MessageType::saasInShutdownState)
    {
        using namespace nx::vms::common;

        const auto serviceManager = systemContext()->saasServiceManager();

        QStringList tooltipLines;

        tooltipLines.append(tr("Some features may not be available."));
        tooltipLines.append(saas::StringsHelper::recommendedAction(serviceManager->saasState()));

        return tooltipLines.join("<br>");
    }

    return QnSystemHealthStringsHelper::messageTooltip(item.message,
        QnResourceDisplayInfo(item.resource).toString(
            appContext()->localSettings()->resourceInfoLevel()));
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

QVariant SystemHealthListModel::Private::timestamp(int index) const
{
    const auto& item = m_items[index];
    switch (item.message)
    {
        case MessageType::remoteArchiveSyncError:
            return QVariant::fromValue(std::chrono::microseconds(
                item.serverData->getRuntimeParams().eventTimestampUsec));

        default:
            return {};
    }
}

int SystemHealthListModel::Private::helpId(int index) const
{
    return rules::healthHelpId(m_items[index].message);
}

int SystemHealthListModel::Private::priority(int index) const
{
    return priority(m_items[index].message);
}

bool SystemHealthListModel::Private::locked(int index) const
{
    return common::system_health::isMessageLocked(m_items[index].message);
}

bool SystemHealthListModel::Private::isCloseable(int index) const
{
    return m_items[index].message != MessageType::defaultCameraPasswords;
}

CommandActionPtr SystemHealthListModel::Private::commandAction(int index) const
{
    const auto& item = m_items[index];

    switch (item.message)
    {
        case MessageType::defaultCameraPasswords:
        {
            const auto action = CommandActionPtr(new CommandAction(tr("Set Passwords")));
            connect(action.data(), &QAction::triggered, this,
                [this]
                {
                    const auto parameters = action::Parameters(
                        getSortedResourceList(MessageType::defaultCameraPasswords))
                            .withArgument(Qn::ForceShowCamerasList, true);

                    menu()->triggerIfPossible(
                        action::ChangeDefaultCameraPasswordAction, parameters);
                });

            return action;
        }

        case MessageType::replacedDeviceDiscovered:
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
    const auto message = messageType(index);
    switch (message)
    {
        case MessageType::emailIsEmpty:
            return action::UserSettingsAction;

        case MessageType::noLicenses:
            return action::PreferencesLicensesTabAction;

        case MessageType::smtpIsNotSet:
            return action::PreferencesSmtpTabAction;

        case MessageType::usersEmailIsEmpty:
            return action::UserSettingsAction;

        case MessageType::emailSendError:
            return action::PreferencesSmtpTabAction;
            break;

        case MessageType::backupStoragesNotConfigured:
        case MessageType::storagesNotConfigured:
        case MessageType::archiveRebuildFinished:
        case MessageType::archiveRebuildCanceled:
        case MessageType::metadataStorageNotSet:
        case MessageType::metadataOnSystemStorage:
            return action::ServerSettingsAction;

        case MessageType::noInternetForTimeSync:
            return action::SystemAdministrationAction;

        case MessageType::cloudPromo:
            return action::PreferencesCloudTabAction;

        case MessageType::cameraRecordingScheduleIsInvalid:
            return action::CameraSettingsAction;

        case MessageType::remoteArchiveSyncError:
            return action::OpenImportFromDevices;

        default:
            return action::NoAction;
    }
}

action::Parameters SystemHealthListModel::Private::parameters(int index) const
{
    const auto message = messageType(index);
    switch (message)
    {
        case MessageType::emailIsEmpty:
            return action::Parameters(context()->user())
                // TODO: Support `FocusElementRole` in the new dialog.
                .withArgument(Qn::FocusElementRole, lit("email"))
                .withArgument(Qn::FocusTabRole, UserSettingsDialog::GeneralTab);

        case MessageType::usersEmailIsEmpty:
        {
            const auto users = getSortedResourceList(message);
            if (!NX_ASSERT(!users.empty()))
                return {};

            return action::Parameters(users.front())
                // TODO: Support `FocusElementRole` in the new dialog.
                .withArgument(Qn::FocusElementRole, lit("email"))
                .withArgument(Qn::FocusTabRole, UserSettingsDialog::GeneralTab);
        }

        case MessageType::noInternetForTimeSync:
            return action::Parameters()
                .withArgument(Qn::FocusElementRole, lit("syncWithInternet"))
                .withArgument(Qn::FocusTabRole,
                    int(QnSystemAdministrationDialog::TimeServerSelection));

        case MessageType::storagesNotConfigured:
        case MessageType::backupStoragesNotConfigured:
        case MessageType::metadataStorageNotSet:
        case MessageType::metadataOnSystemStorage:
        {
            const auto servers = getSortedResourceList(message);
            if (!NX_ASSERT(!servers.empty()))
                return {};

            return action::Parameters(servers.front())
                .withArgument(Qn::FocusTabRole, QnServerSettingsDialog::StorageManagmentPage);
        }

        case MessageType::archiveRebuildFinished:
        case MessageType::archiveRebuildCanceled:
        {
            return action::Parameters(m_items[index].resource)
                .withArgument(Qn::FocusTabRole, QnServerSettingsDialog::StorageManagmentPage);
        }

        case MessageType::cameraRecordingScheduleIsInvalid:
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

common::system_health::MessageType SystemHealthListModel::Private::messageType(int index) const
{
    const auto& item = m_items[index];
    return item.message;
}

void SystemHealthListModel::Private::addItem(
    MessageType message, const QVariant& params)
{
    doAddItem(message, params, false /*initial*/);
}

QSet<QnResourcePtr> SystemHealthListModel::Private::getResourceSet(
    MessageType message) const
{
    const auto systemHealthState = context()->instance<SystemHealthState>();
    return systemHealthState->data(message).value<QSet<QnResourcePtr>>();
}

QnResourceList SystemHealthListModel::Private::getSortedResourceList(
    MessageType message) const
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
    MessageType message, const QVariant& params, bool initial)
{
    NX_VERBOSE(this, "Adding a system health message %1", message);

    if (!messageIsSupported(message))
        return; //< Message type is processed in a separate model.

    if (!common::system_health::isMessageVisible(message))
    {
        NX_VERBOSE(this, "The message %1 is not visible", message);
        return;
    }

    if (common::system_health::isMessageVisibleInSettings(message))
    {
        if (!appContext()->localSettings()->popupSystemHealth().contains(message))
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

    auto position = std::lower_bound(m_items.begin(), m_items.end(), item);
    const auto index = std::distance(m_items.begin(), position);

    if (position != m_items.end() && *position == item)
        return; //< Item already exists.

    {
        // New item.
        ScopedInsertRows insertRows(q, index, index, !initial);
        m_items.insert(position, item);
    }
}

void SystemHealthListModel::Private::removeItem(
    MessageType message,
    const QVariant& params)
{
    if (!messageIsSupported(message))
        return; //< Message type is processed in a separate model.

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
    MessageType message,
    const QnResourcePtr& resource)
{
    NX_VERBOSE(this, "Removing a system health message %1", toString(message));

    const Item item(message, resource);
    const auto position = std::lower_bound(m_items.cbegin(), m_items.cend(), item);
    if (position != m_items.cend() && *position == item)
        q->removeRows(std::distance(m_items.cbegin(), position), 1);
}

void SystemHealthListModel::Private::toggleItem(MessageType message, bool isOn)
{
    if (isOn)
        addItem(message, {});
    else
        removeItem(message, {});
}

void SystemHealthListModel::Private::updateItem(MessageType message)
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

int SystemHealthListModel::Private::priority(MessageType message)
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
        case MessageType::cloudPromo:
            return priorityValue(kCloudPromoPriority);

        case MessageType::defaultCameraPasswords:
            return priorityValue(kDefaultCameraPasswordsPriority);

        case MessageType::cameraRecordingScheduleIsInvalid:
            return priorityValue(kInvalidRecordingSchedulePriority);

        default:
            return int(QnNotificationLevel::valueOf(message));
    }
}

QString SystemHealthListModel::Private::decorationPath(MessageType message)
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
        case MessageType::smtpIsNotSet:
        case MessageType::emailIsEmpty:
        case MessageType::usersEmailIsEmpty:
        case MessageType::emailSendError:
            return "events/email_20.svg";

        case MessageType::noLicenses:
            return "events/license_20.svg";

        case MessageType::backupStoragesNotConfigured:
        case MessageType::storagesNotConfigured:
        case MessageType::archiveRebuildFinished:
        case MessageType::archiveRebuildCanceled:
        case MessageType::metadataOnSystemStorage: //< TODO: #vbreus Probably should be changed to the 'analytics' icon.
            return "events/storage_20.svg";

        case MessageType::cloudPromo:
            return "cloud/cloud_20.png";

        default:
            return {};
    }
}

} // namespace nx::vms::client::desktop
