// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_health_list_model_p.h"

#include <chrono>
#include <limits>

#include <QtCore/QCollator>
#include <QtGui/QDesktopServices>

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <health/system_health_strings_helper.h>
#include <nx/utils/metatypes.h>
#include <nx/utils/string.h>
#include <nx/vms/client/core/common/utils/cloud_url_helper.h>
#include <nx/vms/client/core/resource/session_resources_signal_listener.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/utils/progress_state.h>
#include <nx/vms/client/desktop/help/rules_help.h>
#include <nx/vms/client/desktop/menu/action.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/resource_properties/camera/camera_settings_tab.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_health/system_health_state.h>
#include <nx/vms/client/desktop/system_update/update_contents.h>
#include <nx/vms/client/desktop/system_update/workbench_update_watcher.h>
#include <nx/vms/client/desktop/utils/user_notification_settings_manager.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/handlers/notification_action_handler.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/saas/saas_utils.h>
#include <nx/vms/common/system_health/system_health_data_helper.h>
#include <nx/vms/event/helpers.h>
#include <nx/vms/time/formatter.h>
#include <ui/common/notification_levels.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/synctime.h>

// TODO: #vkutin Dialogs are included just for tab identifiers. Needs refactoring to avoid it.
#include <nx/vms/client/desktop/system_administration/dialogs/user_settings_dialog.h>
#include <ui/dialogs/resource_properties/server_settings_dialog.h>
#include <ui/dialogs/system_administration_dialog.h>

namespace nx::vms::client::desktop {

using namespace nx::vms::common::system_health;

namespace {

bool isServiceDisabledMessage(MessageType message)
{
    return message == MessageType::recordingServiceDisabled
        || message == MessageType::cloudServiceDisabled
        || message == MessageType::integrationServiceDisabled;
}

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
    return !isMessageIntercom(message);
}

} // namespace

//-------------------------------------------------------------------------------------------------
// SystemHealthListModel::Private::Item definition.


QnResourcePtr SystemHealthListModel::Private::Item::getResource() const
{
    return !resources.empty() ? resources[0] : QnResourcePtr();
}

bool SystemHealthListModel::Private::Item::operator==(const Item& other) const
{
    return type == other.type && resources == other.resources;
}

bool SystemHealthListModel::Private::Item::operator!=(const Item& other) const
{
    return !(*this == other);
}

bool SystemHealthListModel::Private::Item::operator<(const Item& other) const
{
    return type != other.type
        ? type < other.type
        : resources < other.resources;
}

//-------------------------------------------------------------------------------------------------
// SystemHealthListModel::Private definition.

SystemHealthListModel::Private::Private(SystemHealthListModel* q):
    base_type(),
    WindowContextAware(q),
    q(q),
    m_popupSystemHealthFilter(system()->userNotificationSettingsManager()->watchedMessages())
{
    if (!NX_ASSERT(system()))
        return;

    // Handle system health state.

    const auto systemHealthState = system()->systemHealthState();
    connect(systemHealthState, &SystemHealthState::stateChanged, this, &Private::toggleItem);
    connect(systemHealthState, &SystemHealthState::dataChanged, this, &Private::updateItem);

    for (const auto index: common::system_health::allMessageTypes({
        common::system_health::isMessageVisibleInSettings}))
    {
        if (systemHealthState->state(index))
            doAddItem(Message{.type = index}, true /*initial*/);
    }

    auto usernameChangesListener = new core::SessionResourcesSignalListener<QnUserResource>(
        system(),
        this);
    usernameChangesListener->addOnSignalHandler(
        &QnUserResource::nameChanged,
        [this](const QnUserResourcePtr& user)
        {
            if (getResourceSet(MessageType::usersEmailIsEmpty).contains(user))
                updateItem(MessageType::usersEmailIsEmpty);
        });
    usernameChangesListener->start();

    const auto cameraNameChangesListener =
        new core::SessionResourcesSignalListener<QnVirtualCameraResource>(system(), this);
    cameraNameChangesListener->addOnSignalHandler(
        &QnVirtualCameraResource::nameChanged,
        [this](const QnVirtualCameraResourcePtr& camera)
        {
            if (getResourceSet(MessageType::cameraRecordingScheduleIsInvalid).contains(camera))
                updateItem(MessageType::cameraRecordingScheduleIsInvalid);
        });
    cameraNameChangesListener->start();

    // Handle system health notifications.

    const auto notificationHandler = windowContext()->notificationActionHandler();
    connect(notificationHandler, &NotificationActionHandler::cleared, this, &Private::clear);

    connect(notificationHandler, &NotificationActionHandler::systemHealthEventAdded,
        this, &Private::addItem);

    connect(notificationHandler, &NotificationActionHandler::systemHealthEventRemoved,
        this, &Private::removeItem);

    connect(
        system()->userNotificationSettingsManager(),
        &UserNotificationSettingsManager::settingsChanged,
        this,
        [this]
        {
            auto userNotificationSettingsManager = system()->userNotificationSettingsManager();
            auto& filter = userNotificationSettingsManager->watchedMessages();
            if (m_popupSystemHealthFilter == filter)
                return;

            for (auto messageType: userNotificationSettingsManager->supportedMessageTypes())
            {
                const bool wasVisible = m_popupSystemHealthFilter.contains(messageType);
                const bool isVisible = filter.contains(messageType);
                if (wasVisible != isVisible)
                {
                    toggleItem(
                        messageType,
                        isVisible && system()->systemHealthState()->state(messageType));
                }
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
    return m_items[index].type;
}

QnResourcePtr SystemHealthListModel::Private::resource(int index) const
{
    return m_items[index].getResource();
}

 QnResourceList SystemHealthListModel::Private::resources(int index) const
{
    return m_items[index].resources;
}

QString SystemHealthListModel::Private::description(int index) const
{
    return QnSystemHealthStringsHelper::messageDescription(m_items[index].type);
}

QString SystemHealthListModel::Private::title(int index) const
{
    const auto& item = m_items[index];

    switch (item.type)
    {
        case MessageType::replacedDeviceDiscovered:
        {
            using namespace nx::vms::common;

            static const auto kDeviceNameColor = core::ColorTheme::instance()->color("light10");

            const auto caption = tr("Replaced camera discovered");

            auto result = html::paragraph(caption);
            if (const auto discoveredDeviceModel =
                system_health::getDeviceModelFromAttributes(item.attributes))
            {
                result += html::paragraph(html::colored(*discoveredDeviceModel, kDeviceNameColor));
            }

            return result;
        }

        default:
        {
            NX_ASSERT(system());
            return QnSystemHealthStringsHelper::messageNotificationTitle(
                system(), item.type, getResourceSet(item.type));
        }
    }
}

QnResourceList SystemHealthListModel::Private::displayedResourceList(int index) const
{
    const auto message = messageType(index);
    switch (message)
    {
        case MessageType::archiveRebuildFinished:
        case MessageType::archiveRebuildCanceled:
            return parameters(index).resources();

        case MessageType::remoteArchiveSyncError:
        case MessageType::archiveIntegrityFailed:
            m_items[index].resources;

        case MessageType::defaultCameraPasswords:
        case MessageType::usersEmailIsEmpty:
        case MessageType::storagesNotConfigured:
        case MessageType::backupStoragesNotConfigured:
        case MessageType::cameraRecordingScheduleIsInvalid:
        case MessageType::metadataStorageNotSet:
        case MessageType::metadataOnSystemStorage:
        case MessageType::cloudStorageIsEnabled:
            return getSortedResourceList(message);

        default:
            return {};
    }
}

QString SystemHealthListModel::Private::toolTip(int index) const
{
    const auto& item = m_items[index];

    // TODO: move all text values to QnSystemHealthStringsHelper::messageTooltip.
    if (item.type == MessageType::replacedDeviceDiscovered)
    {
        using namespace nx::vms::common;

        const auto camera = item.getResource().dynamicCast<QnVirtualCameraResource>();
        if (!NX_ASSERT(camera, "Invalid notification parameter"))
            return {};

        const auto& attributes = item.attributes;
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

    if (const auto serviceType = overusedSaasServiceType(item.type))
    {
        if (!NX_ASSERT(system()))
            return {};

        const auto serviceManager = system()->saasServiceManager();
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

    if (item.type == MessageType::saasInSuspendedState
        || item.type == MessageType::saasInShutdownState)
    {
        using namespace nx::vms::common;

        const auto serviceManager = system()->saasServiceManager();

        QStringList tooltipLines;

        tooltipLines.append(tr("Some features may not be available."));
        tooltipLines.append(saas::StringsHelper::recommendedAction(serviceManager->saasState()));

        return tooltipLines.join("<br>");
    }

    if (item.type == MessageType::saasTierIssue)
    {
        return (tr("The Site exceeds its Organization's limits and may become "
            "non-functional soon. Please adjust your usage to avoid service disruption."));
    }

    if (isServiceDisabledMessage(item.type))
    {
        using namespace nx::vms::api;
        EventReason eventReason;
        switch (item.type)
        {
            case MessageType::recordingServiceDisabled:
                eventReason = EventReason::notEnoughLocalRecordingServices;
                break;
            case MessageType::cloudServiceDisabled:
                eventReason = EventReason::notEnoughCloudRecordingServices;
                break;
            case MessageType::integrationServiceDisabled:
                eventReason = EventReason::notEnoughIntegrationServices;
                break;
            default:
                NX_ASSERT(false, "Unexpected message type %1", item.type);
                return QString();
        }

        QStringList tooltipLines;
        tooltipLines << servicesDisabledReason(eventReason, item.resources.size());
        tooltipLines << QString();
        const auto resourceInfoLevel = appContext()->localSettings()->resourceInfoLevel();
        for (const auto& resource: item.resources)
            tooltipLines << QnResourceDisplayInfo(resource).toString(resourceInfoLevel);
        return tooltipLines.join("<br>");
    }

    NX_ASSERT(system());
    return QnSystemHealthStringsHelper::messageTooltip(
        system(), item.type, getResourceSet(item.type));
}

QString SystemHealthListModel::Private::decorationPath(int index) const
{
    NX_ASSERT(system());
    return decorationPath(system(), m_items[index].type);
}

QString SystemHealthListModel::Private::servicesDisabledReason(
    nx::vms::api::EventReason reasonCode,
    int channelCount)
{
    using namespace nx::vms::api;

    switch (reasonCode)
    {
        case EventReason::notEnoughLocalRecordingServices:
            return tr("Recording on %n channels was stopped due to service overuse.",
                "", channelCount);

        case EventReason::notEnoughCloudRecordingServices:
            return tr("Cloud storage backup on %n channels was stopped due to service overuse.",
                "", channelCount);

        case EventReason::notEnoughIntegrationServices:
            return tr("Paid integration service usage on %n channels was stopped due to service "
                "overuse.", "", channelCount);
            break;

        default:
            NX_ASSERT(false, "Unexpected reason code");
            return {};
    }
}

QColor SystemHealthListModel::Private::color(int index) const
{
    NX_ASSERT(system());
    return QnNotificationLevel::notificationTextColor(
        QnNotificationLevel::valueOf(system(), m_items[index].type));
}

QVariant SystemHealthListModel::Private::timestamp(int index) const
{
    const auto& item = m_items[index];
    switch (item.type)
    {
        case MessageType::remoteArchiveSyncError:
            return QVariant::fromValue(item.timestampUs);

        default:
            return {};
    }
}

int SystemHealthListModel::Private::helpId(int index) const
{
    return rules::healthHelpId(m_items[index].type);
}

int SystemHealthListModel::Private::priority(int index) const
{
    NX_ASSERT(system());
    return priority(system(), m_items[index].type);
}

bool SystemHealthListModel::Private::locked(int index) const
{
    return common::system_health::isMessageLocked(m_items[index].type);
}

bool SystemHealthListModel::Private::isCloseable(int index) const
{
    return m_items[index].type != MessageType::defaultCameraPasswords;
}

CommandActionPtr SystemHealthListModel::Private::commandAction(int index) const
{
    const auto& item = m_items[index];

    switch (item.type)
    {
        case MessageType::defaultCameraPasswords:
        {
            const auto action = CommandActionPtr(new CommandAction(tr("Set Passwords")));
            connect(action.data(), &CommandAction::triggered, this,
                [this]
                {
                    const auto parameters = menu::Parameters(
                        getSortedResourceList(MessageType::defaultCameraPasswords))
                            .withArgument(Qn::ForceShowCamerasList, true);

                    menu()->triggerIfPossible(
                        menu::ChangeDefaultCameraPasswordAction, parameters);
                });

            return action;
        }

        case MessageType::replacedDeviceDiscovered:
        {
            const auto action = CommandActionPtr(new CommandAction(tr("Undo Replace")));
            connect(action.data(), &CommandAction::triggered, this,
                [this, resource = item.getResource()]
                {
                    const auto parameters = menu::Parameters(resource);
                    menu()->triggerIfPossible(menu::UndoReplaceCameraAction, parameters);
                });

            return action;
        }

        case MessageType::notificationLanguageDiffers:
        {
            const auto action = CommandActionPtr(new CommandAction(tr("Open Settings")));
            connect(action.data(), &CommandAction::triggered, this,
                [this]
                {
                    auto user = workbenchContext()->user();
                    if (!NX_ASSERT(user))
                        return;

                    if (user->isCloud())
                    {
                        core::CloudUrlHelper urlHelper(
                            utils::SystemUri::ReferralSource::DesktopClient,
                            utils::SystemUri::ReferralContext::None);
                        const auto url = urlHelper.accountManagementUrl();
                        QDesktopServices::openUrl(url);
                    }
                    else
                    {
                        menu()->trigger(menu::UserSettingsAction, {user});
                    }
                });

            return action;
        }

        case MessageType::saasTierIssue:
        {
            const auto action = CommandActionPtr(new CommandAction(tr("Open Services")));
            connect(action.data(),
                &CommandAction::triggered,
                this,
                [this]
                {
                    menu()->trigger(menu::PreferencesServicesTabAction);
                });

            return action;
        }

        default:
            return {};
    }
}

menu::IDType SystemHealthListModel::Private::action(int index) const
{
    const auto message = messageType(index);
    switch (message)
    {
        case MessageType::emailIsEmpty:
        case MessageType::usersEmailIsEmpty:
            return menu::UserSettingsAction;

        case MessageType::noLicenses:
            return menu::PreferencesLicensesTabAction;

        case MessageType::smtpIsNotSet:
        case MessageType::emailSendError:
            return menu::PreferencesSmtpTabAction;

        case MessageType::backupStoragesNotConfigured:
        case MessageType::storagesNotConfigured:
        case MessageType::archiveRebuildFinished:
        case MessageType::archiveRebuildCanceled:
        case MessageType::metadataStorageNotSet:
        case MessageType::metadataOnSystemStorage:
        case MessageType::cloudStorageIsAvailable:
        case MessageType::cloudStorageIsEnabled:
            return menu::ServerSettingsAction;

        case MessageType::noInternetForTimeSync:
            return menu::SystemAdministrationAction;

        case MessageType::cloudPromo:
            return menu::PreferencesCloudTabAction;

        case MessageType::cameraRecordingScheduleIsInvalid:
            return menu::CameraSettingsAction;

        case MessageType::remoteArchiveSyncError:
            return menu::OpenImportFromDevices;

        default:
            return menu::NoAction;
    }
}

menu::Parameters SystemHealthListModel::Private::parameters(int index) const
{
    const auto message = messageType(index);
    switch (message)
    {
        case MessageType::emailIsEmpty:
            return menu::Parameters(workbenchContext()->user())
                // TODO: Support `FocusElementRole` in the new dialog.
                .withArgument(Qn::FocusElementRole, lit("email"))
                .withArgument(Qn::FocusTabRole, UserSettingsDialog::GeneralTab);

        case MessageType::usersEmailIsEmpty:
        {
            const auto users = getSortedResourceList(message);
            if (!NX_ASSERT(!users.empty()))
                return {};

            return menu::Parameters(users.front())
                // TODO: Support `FocusElementRole` in the new dialog.
                .withArgument(Qn::FocusElementRole, lit("email"))
                .withArgument(Qn::FocusTabRole, UserSettingsDialog::GeneralTab);
        }

        case MessageType::noInternetForTimeSync:
            return menu::Parameters()
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

            return menu::Parameters(servers.front())
                .withArgument(Qn::FocusTabRole, QnServerSettingsDialog::StorageManagementPage);
        }
        case MessageType::cloudStorageIsAvailable:
        case MessageType::cloudStorageIsEnabled:
        {
            return menu::Parameters(system()->currentServer())
                .withArgument(Qn::FocusTabRole, QnServerSettingsDialog::StorageManagementPage);
        }

        case MessageType::archiveRebuildFinished:
        case MessageType::archiveRebuildCanceled:
        {
            return menu::Parameters(m_items[index].getResource())
                .withArgument(Qn::FocusTabRole, QnServerSettingsDialog::StorageManagementPage);
        }

        case MessageType::cameraRecordingScheduleIsInvalid:
        {
            const auto cameras = getSortedResourceList(message);
            if (!NX_ASSERT(!cameras.empty()))
                return {};

            return menu::Parameters(cameras.front())
                .withArgument(Qn::FocusTabRole, CameraSettingsTab::recording);
        }

        default:
            return menu::Parameters();
    }
}

common::system_health::MessageType SystemHealthListModel::Private::messageType(int index) const
{
    return m_items[index].type;
}

void SystemHealthListModel::Private::addItem(const Message& message)
{
    doAddItem(message, /*initial*/ false);
}

QSet<QnResourcePtr> SystemHealthListModel::Private::getResourceSet(
    MessageType message) const
{
    if (!NX_ASSERT(system()))
        return {};

    return system()->systemHealthState()->data(message).value<QSet<QnResourcePtr>>();
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
            if (!NX_ASSERT(lhs))
                return false;
            if (!NX_ASSERT(rhs))
                return true;
            return collator.compare(lhs->getName(), rhs->getName()) < 0;
        };

    const auto resourceSet = getResourceSet(message);
    QnResourceList result(resourceSet.cbegin(), resourceSet.cend());
    std::sort(result.begin(), result.end(), resourceNameLessThan);
    return result;
}

void SystemHealthListModel::Private::doAddItem(const Message& message, bool initial)
{
    NX_VERBOSE(this, "Adding a system health message %1", message.type);

    if (!NX_ASSERT(system()))
        return;

    if (!messageIsSupported(message.type))
        return; //< Message type is processed in a separate model.

    if (!common::system_health::isMessageVisible(message.type))
    {
        NX_VERBOSE(this, "The message %1 is not visible", message.type);
        return;
    }

    if (common::system_health::isMessageVisibleInSettings(message.type))
    {
        if (!system()->userNotificationSettingsManager()->watchedMessages().contains(message.type))
        {
            NX_VERBOSE(this, "The message %1 is not allowed by the filter", message.type);
            return; //< Not allowed by filter.
        }
    }

    auto item = Item{message};
    item.resources = system()->resourcePool()->getResourcesByIds(message.resourceIds);

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

void SystemHealthListModel::Private::removeItem(const Message& message)
{
    if (!NX_ASSERT(system()))
        return;

    if (!messageIsSupported(message.type))
        return; //< Message type is processed in a separate model.

    if (message.type == MessageType::replacedDeviceDiscovered)
    {
        if (const auto resource = system()->resourcePool()->getResourceById(message.resourceId()))
            removeItemForResource(message.type, resource);
    }
    else
    {
        NX_VERBOSE(this, "Removing a system health message %1", toString(message.type));

        const auto range = std::equal_range(m_items.cbegin(), m_items.cend(), message.type);
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

    Item item;
    item.resources = {resource};
    static_cast<Message&>(item) = Message{.type = message, .resourceIds = {resource->getId()}};

    const auto position = std::lower_bound(m_items.cbegin(), m_items.cend(), item);
    if (position != m_items.cend() && *position == item)
        q->removeRows(std::distance(m_items.cbegin(), position), 1);
}

void SystemHealthListModel::Private::toggleItem(MessageType type, bool isOn)
{
    auto message = Message{.type = type, .active = isOn};

    if (isOn)
        addItem(message);
    else
        removeItem(message);
}

void SystemHealthListModel::Private::updateItem(MessageType message)
{
    Item item;
    item.type = message;
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

int SystemHealthListModel::Private::priority(SystemContext* systemContext, MessageType message)
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
            return QnNotificationLevel::priority(systemContext, message);
    }
}

QString SystemHealthListModel::Private::decorationPath(
    SystemContext* systemContext,
    MessageType message)
{
    switch (message)
    {
        case MessageType::showIntercomInformer:
        case MessageType::showMissedCallInformer:
            return "20x20/Outline/call.svg";

        case MessageType::cameraRecordingScheduleIsInvalid:
            return "20x20/Outline/device.svg";

        case MessageType::smtpIsNotSet:
            return "20x20/Outline/server.svg";

        case MessageType::archiveRebuildCanceled:
            return "20x20/Outline/error.svg";

        case MessageType::emailIsEmpty:
        case MessageType::usersEmailIsEmpty:
            return "20x20/Outline/email.svg";

        case MessageType::noLicenses:
            return "20x20/Outline/key.svg";

        case MessageType::archiveRebuildFinished:
            return "20x20/Outline/success.svg";

        case MessageType::backupStoragesNotConfigured:
        case MessageType::storagesNotConfigured:
        case MessageType::metadataOnSystemStorage:
            return "20x20/Outline/storage.svg";

        case MessageType::cloudPromo:
            return "20x20/Outline/cloud.svg";

        // There is no specific icon for message, the icon will be provided according it's level.
        default:
            break;
    }

    using namespace nx::vms::event;
    switch (QnNotificationLevel::valueOf(systemContext, message))
    {
        case Level::critical:
            return "20x20/Outline/error.svg";

        case Level::important:
            return "20x20/Outline/warning.svg";

        case Level::success:
            return "20x20/Outline/success.svg";

        default:
            break;
    }
    return "";

}

} // namespace nx::vms::client::desktop
