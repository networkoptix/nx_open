// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_health_state.h"

#include <array>

#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtWidgets/QAction>

#include <api/runtime_info_manager.h>
#include <client/client_message_processor.h>
#include <client/client_show_once_settings.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <licensing/license.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_health/default_password_cameras_watcher.h>
#include <nx/vms/client/desktop/system_health/invalid_recording_schedule_watcher.h>
#include <nx/vms/client/desktop/system_health/system_internet_access_watcher.h>
#include <nx/vms/client/desktop/system_health/user_emails_watcher.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/common/system_settings.h>
#include <server/server_storage_manager.h>
#include <ui/workbench/handlers/workbench_notifications_handler.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <utils/email/email.h>

using namespace nx::vms::common;

namespace nx::vms::client::desktop {

namespace {

static const QString kCloudPromoShowOnceKey(lit("CloudPromoNotification"));

} // namespace

// ------------------------------------------------------------------------------------------------
// SystemHealthState::Private declaration.

class SystemHealthState::Private
{
    SystemHealthState* const q;

public:
    Private(SystemHealthState* q);

    bool state(SystemHealthIndex index) const;
    QVariant data(SystemHealthIndex index) const;

private:
    bool hasResourcesForMessageType(SystemHealthIndex message) const;
    void setResourcesForMessageType(SystemHealthIndex message, QSet<QnResourcePtr> resources);

    bool setState(SystemHealthIndex index, bool value);
    bool calculateState(SystemHealthIndex index) const;

    bool update(SystemHealthIndex index) { return setState(index, calculateState(index)); }

    SystemInternetAccessWatcher* internetAccessWatcher() const;
    DefaultPasswordCamerasWatcher* defaultPasswordWatcher() const;
    UserEmailsWatcher* userEmailsWatcher() const;
    bool isAdmin() const;

    void updateCamerasWithDefaultPassword();
    void updateCamerasWithInvalidSchedule();
    void updateUsersWithInvalidEmail();
    void updateServersWithoutStorages();
    void updateServersWithMetadataStorageIssues();

private:
    std::array<bool, SystemHealthIndex::Count> m_state;
    QHash<SystemHealthIndex, QSet<QnResourcePtr>> m_resourcesForMessageType;
    std::unique_ptr<InvalidRecordingScheduleWatcher> m_invalidRecordingScheduleWatcher;
};

// ------------------------------------------------------------------------------------------------
// SystemHealthState::Private definition.

SystemHealthState::Private::Private(SystemHealthState* q):
    q(q),
    m_state(),
    m_invalidRecordingScheduleWatcher(std::make_unique<InvalidRecordingScheduleWatcher>())
{
#define update(index) [this]() { return update(SystemHealthIndex::index); }

    // NoLicenses.

    connect(q->systemContext()->licensePool(),
        &QnLicensePool::licensesChanged,
        q,
        update(NoLicenses));
    connect(q->context(), &QnWorkbenchContext::userChanged, q, update(NoLicenses));

    update(NoLicenses)();

    // SmtpIsNotSet.

    connect(q->systemSettings(), &SystemSettings::emailSettingsChanged, q, update(SmtpIsNotSet));
    connect(q->context(), &QnWorkbenchContext::userChanged, q, update(SmtpIsNotSet));

    update(SmtpIsNotSet)();

    // NoInternetForTimeSync.

    NX_ASSERT(internetAccessWatcher(), "Internet access watcher is not initialized");

    connect(internetAccessWatcher(), &SystemInternetAccessWatcher::internetAccessChanged,
        q, update(NoInternetForTimeSync));

    connect(q->systemSettings(), &SystemSettings::timeSynchronizationSettingsChanged,
        q, update(NoInternetForTimeSync));

    const auto messageProcessor =
        dynamic_cast<QnClientMessageProcessor*>(q->messageProcessor());

    connect(messageProcessor, &QnCommonMessageProcessor::initialResourcesReceived, q,
        [this]()
        {
            update(NoInternetForTimeSync);
            updateServersWithoutStorages();
        });

    connect(messageProcessor, &QnCommonMessageProcessor::connectionClosed,
        q, update(NoInternetForTimeSync));

    update(NoInternetForTimeSync)();

    // replacedDeviceDiscovered.

    connect(messageProcessor, &QnClientMessageProcessor::hardwareIdMappingRemoved, q,
        [this](const QnUuid& id)
        {
            const auto resource = this->q->resourcePool()->getResourceById(id);
            const auto notificationsHandler =
                this->q->context()->instance<QnWorkbenchNotificationsHandler>();

            notificationsHandler->setSystemHealthEventVisible(
                QnSystemHealth::replacedDeviceDiscovered, resource, false);
        });

    // DefaultCameraPasswords.

    NX_ASSERT(defaultPasswordWatcher(), "Default password cameras watcher is not initialized");

    connect(defaultPasswordWatcher(), &DefaultPasswordCamerasWatcher::cameraSetChanged, q,
        [this]() { updateCamerasWithDefaultPassword(); });

    connect(q->context(), &QnWorkbenchContext::userChanged, q,
        [this]() { updateCamerasWithDefaultPassword(); });

    updateCamerasWithDefaultPassword();

    // InvalidRecordingSchedule.

    connect(m_invalidRecordingScheduleWatcher.get(),
        &InvalidRecordingScheduleWatcher::camerasChanged,
        q,
        [this]() { updateCamerasWithInvalidSchedule(); });

    connect(q->context(), &QnWorkbenchContext::userChanged, q,
        [this]() { updateCamerasWithInvalidSchedule(); });

    updateCamerasWithInvalidSchedule();

    // EmailIsEmpty.

    NX_ASSERT(userEmailsWatcher(), "User email watcher is not initialized");

    connect(userEmailsWatcher(), &UserEmailsWatcher::userSetChanged, q, update(EmailIsEmpty));
    connect(q->context(), &QnWorkbenchContext::userChanged, q, update(EmailIsEmpty));
    update(EmailIsEmpty)();

    // UsersEmailIsEmpty.

    connect(userEmailsWatcher(), &UserEmailsWatcher::userSetChanged, q,
        [this]() { updateUsersWithInvalidEmail(); });

    connect(q->context(), &QnWorkbenchContext::userChanged, q,
        [this]() { updateUsersWithInvalidEmail(); });

    updateUsersWithInvalidEmail();

    // StoragesNotConfigured.

    const auto runtimeInfoManager = q->context()->runtimeInfoManager();
    NX_ASSERT(runtimeInfoManager);

    connect(runtimeInfoManager, &QnRuntimeInfoManager::runtimeInfoAdded, q,
        [this]() { updateServersWithoutStorages(); });

    connect(runtimeInfoManager, &QnRuntimeInfoManager::runtimeInfoRemoved, q,
        [this]() { updateServersWithoutStorages(); });

    connect(runtimeInfoManager, &QnRuntimeInfoManager::runtimeInfoChanged, q,
        [this]() { updateServersWithoutStorages(); });

    updateServersWithoutStorages();

    // CloudPromo.

    connect(q->action(ui::action::HideCloudPromoAction), &QAction::triggered, q,
        [this]
        {
            qnClientShowOnce->setFlag(kCloudPromoShowOnceKey);
            update(CloudPromo)();
        });

    connect(q->context(), &QnWorkbenchContext::userChanged, q, update(CloudPromo));
    update(CloudPromo)();

    // Metadata storage issues.

    connect(qnServerStorageManager, &QnServerStorageManager::storageChanged,
        [this]() { updateServersWithMetadataStorageIssues(); });

    connect(qnServerStorageManager, &QnServerStorageManager::activeMetadataStorageChanged,
        [this]() { updateServersWithMetadataStorageIssues(); });

    connect(q->context(), &QnWorkbenchContext::userChanged, q, update(metadataStorageNotSet));
    update(metadataStorageNotSet)();

    connect(q->context(), &QnWorkbenchContext::userChanged, q, update(metadataOnSystemStorage));
    update(metadataOnSystemStorage)();

#undef update
}

void SystemHealthState::Private::updateCamerasWithDefaultPassword()
{
    const auto camerasWithDefaultPassword = defaultPasswordWatcher()->camerasWithDefaultPassword();

    QSet<QnResourcePtr> resourceSet;
    for (const auto& camera: camerasWithDefaultPassword)
        resourceSet.insert(camera);

    setResourcesForMessageType(SystemHealthIndex::DefaultCameraPasswords, resourceSet);
}

void SystemHealthState::Private::updateCamerasWithInvalidSchedule()
{
    const auto camerasWithInvalidSchedule =
        m_invalidRecordingScheduleWatcher->camerasWithInvalidSchedule();

    QSet<QnResourcePtr> resourceSet;
    for (const auto& camera: camerasWithInvalidSchedule)
    {
        if (q->accessController()->hasPermissions(camera, Qn::ReadWriteSavePermission))
            resourceSet.insert(camera);
    }

    setResourcesForMessageType(SystemHealthIndex::cameraRecordingScheduleIsInvalid, resourceSet);
}

void SystemHealthState::Private::updateUsersWithInvalidEmail()
{
    const auto usersWithInvalidEmail = userEmailsWatcher()->usersWithInvalidEmail();

    QSet<QnResourcePtr> resourceSet;
    for (const auto& user: usersWithInvalidEmail)
    {
        if (user != q->context()->user()
            && q->accessController()->hasPermissions(user, Qn::WriteEmailPermission))
        {
            resourceSet.insert(user);
        }
    }

    setResourcesForMessageType(SystemHealthIndex::UsersEmailIsEmpty, resourceSet);
}

void SystemHealthState::Private::updateServersWithoutStorages()
{
    const auto calculateServersWithoutStorages =
        [this](auto flag) -> QSet<QnResourcePtr>
        {
            const auto runtimeInfoManager = q->context()->runtimeInfoManager();
            if (!NX_ASSERT(runtimeInfoManager))
                return {};

            QSet<QnUuid> serverIds;
            const auto items = runtimeInfoManager->items()->getItems();
            for (const auto item: items)
            {
                if (item.data.flags.testFlag(flag))
                    serverIds.insert(item.uuid);
            }

            const auto servers =
                q->resourcePool()->getResourcesByIds<QnMediaServerResource>(serverIds);

            return QSet<QnResourcePtr>(servers.cbegin(), servers.cend());
        };

    setResourcesForMessageType(SystemHealthIndex::StoragesNotConfigured,
        calculateServersWithoutStorages(nx::vms::api::RuntimeFlag::noStorages));

    setResourcesForMessageType(SystemHealthIndex::backupStoragesNotConfigured,
        calculateServersWithoutStorages(nx::vms::api::RuntimeFlag::noBackupStorages));
}

void SystemHealthState::Private::updateServersWithMetadataStorageIssues()
{
    QSet<QnResourcePtr> serversWithoutMetadataStorage;
    QSet<QnResourcePtr> serversWithMetadataOnSystemStorage;

    const auto resourcePool = q->context()->resourcePool();
    const auto servers = resourcePool->servers();
    for (const auto server: servers)
    {
        const auto serverMetadataStorageId = server->metadataStorageId();
        if (serverMetadataStorageId.isNull())
        {
            serversWithoutMetadataStorage.insert(server);
            continue;
        }

        const auto serverStorages = server->getStorages();
        for (const auto storage: serverStorages)
        {
            if (storage->statusFlag().testFlag(nx::vms::api::StorageStatus::system)
                && storage->getId() == serverMetadataStorageId)
            {
                serversWithMetadataOnSystemStorage.insert(server);
            }
        }
    }

    // TODO: #vbreus What if disabled storage or storage with nx::vms::api::StorageStatus::tooSmall
    // flag set as metadata storage?

    setResourcesForMessageType(SystemHealthIndex::metadataStorageNotSet,
        serversWithoutMetadataStorage);

    setResourcesForMessageType(SystemHealthIndex::metadataOnSystemStorage,
        serversWithMetadataOnSystemStorage);
}

bool SystemHealthState::Private::state(SystemHealthIndex index) const
{
    return m_state[index];
}

bool SystemHealthState::Private::setState(SystemHealthIndex index, bool value)
{
    if (state(index) == value)
        return false;

    m_state[index] = value;
    emit q->stateChanged(index, value, {});

    return true;
}

bool SystemHealthState::Private::calculateState(SystemHealthIndex index) const
{
    if (!q->context()->user())
        return false;

    switch (index)
    {
        case SystemHealthIndex::NoInternetForTimeSync:
            return isAdmin()
                && q->systemSettings()->isTimeSynchronizationEnabled()
                && q->systemSettings()->primaryTimeServer().isNull()
                && !internetAccessWatcher()->systemHasInternetAccess();

        case SystemHealthIndex::NoLicenses:
            return isAdmin() && q->systemContext()->licensePool()->isEmpty();

        case SystemHealthIndex::SmtpIsNotSet:
            return isAdmin() && !q->systemSettings()->emailSettings().isValid();

        case SystemHealthIndex::DefaultCameraPasswords:
            return isAdmin() && hasResourcesForMessageType(index);

        case SystemHealthIndex::EmailIsEmpty:
        {
            const auto user = q->context()->user();
            return user && userEmailsWatcher()->usersWithInvalidEmail().contains(user)
                && q->accessController()->hasPermissions(user, Qn::WriteEmailPermission);
        }

        case SystemHealthIndex::UsersEmailIsEmpty:
            return hasResourcesForMessageType(index);

        case SystemHealthIndex::CloudPromo:
            return q->context()->user()
                && q->context()->user()->userRole() == Qn::UserRole::owner
                && q->systemSettings()->cloudSystemId().isEmpty()
                && !qnClientShowOnce->testFlag(kCloudPromoShowOnceKey);

        case SystemHealthIndex::StoragesNotConfigured:
            return isAdmin() && hasResourcesForMessageType(index);

        case SystemHealthIndex::backupStoragesNotConfigured:
            return isAdmin() && hasResourcesForMessageType(index);

        case SystemHealthIndex::cameraRecordingScheduleIsInvalid:
            return hasResourcesForMessageType(index);

        case SystemHealthIndex::metadataStorageNotSet:
            return isAdmin() && hasResourcesForMessageType(index);

        case SystemHealthIndex::metadataOnSystemStorage:
            return isAdmin() && hasResourcesForMessageType(index);

        default:
            NX_ASSERT(false, "This system health index is not handled by SystemHealthState");
            return false;
    }
}

QVariant SystemHealthState::Private::data(SystemHealthIndex index) const
{
    if (!m_state[index])
        return {};

    switch (index)
    {
        case SystemHealthIndex::UsersEmailIsEmpty:
        case SystemHealthIndex::StoragesNotConfigured:
        case SystemHealthIndex::DefaultCameraPasswords:
        case SystemHealthIndex::backupStoragesNotConfigured:
        case SystemHealthIndex::cameraRecordingScheduleIsInvalid:
        case SystemHealthIndex::metadataStorageNotSet:
        case SystemHealthIndex::metadataOnSystemStorage:
            return QVariant::fromValue(
                m_resourcesForMessageType.value(index, QSet<QnResourcePtr>()));

        default:
            return {};
    }
}

bool SystemHealthState::Private::hasResourcesForMessageType(SystemHealthIndex message) const
{
    const auto itr = m_resourcesForMessageType.constFind(message);
    return itr != m_resourcesForMessageType.end() && !itr.value().empty();
}

void SystemHealthState::Private::setResourcesForMessageType(
    SystemHealthIndex message,
    QSet<QnResourcePtr> resources)
{
    if (resources != m_resourcesForMessageType.value(message, QSet<QnResourcePtr>()))
    {
        m_resourcesForMessageType.insert(message, resources);
        if (!update(message))
            emit q->dataChanged(message, {});
    }
}

bool SystemHealthState::Private::isAdmin() const
{
    return q->accessController()->hasGlobalPermission(GlobalPermission::admin);
}

SystemInternetAccessWatcher* SystemHealthState::Private::internetAccessWatcher() const
{
    return q->context()->findInstance<SystemInternetAccessWatcher>();
}

DefaultPasswordCamerasWatcher* SystemHealthState::Private::defaultPasswordWatcher() const
{
    return q->context()->findInstance<DefaultPasswordCamerasWatcher>();
}

UserEmailsWatcher* SystemHealthState::Private::userEmailsWatcher() const
{
    return q->context()->instance<UserEmailsWatcher>();
}

// ------------------------------------------------------------------------------------------------
// SystemHealthState definition.

SystemHealthState::SystemHealthState(QObject* parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    d(new Private(this))
{
}

SystemHealthState::~SystemHealthState()
{
}

bool SystemHealthState::state(SystemHealthIndex index) const
{
    return d->state(index);
}

QVariant SystemHealthState::data(SystemHealthIndex index) const
{
    return d->data(index);
}

} // namespace nx::vms::client::desktop
