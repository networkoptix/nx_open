// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_health_state.h"

#include <array>

#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtGui/QAction>

#include <api/runtime_info_manager.h>
#include <client/client_message_processor.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <licensing/license.h>
#include <nx/vms/api/data/storage_flags.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/desktop/access/caching_access_controller.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/settings/show_once_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_health/default_password_cameras_watcher.h>
#include <nx/vms/client/desktop/system_health/invalid_recording_schedule_watcher.h>
#include <nx/vms/client/desktop/system_health/system_internet_access_watcher.h>
#include <nx/vms/client/desktop/system_health/user_emails_watcher.h>
#include <nx/vms/client/desktop/system_update/workbench_update_watcher.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/saas/saas_utils.h>
#include <nx/vms/common/system_settings.h>
#include <server/server_storage_manager.h>
#include <ui/workbench/workbench_context.h>
#include <utils/email/email.h>

using namespace nx::vms::common;

namespace nx::vms::client::desktop {

// ------------------------------------------------------------------------------------------------
// SystemHealthState::Private declaration.

class SystemHealthState::Private: public SystemContextAware
{
    SystemHealthState* const q;

public:
    Private(SystemHealthState* q);

    bool state(SystemHealthIndex index) const;
    QVariant data(SystemHealthIndex index) const;

private:
    QnUserResourcePtr user() const { return systemContext()->userWatcher()->user(); }
    bool hasResourcesForMessageType(SystemHealthIndex message) const;
    void setResourcesForMessageType(SystemHealthIndex message, QSet<QnResourcePtr> resources);

    bool setState(SystemHealthIndex index, bool value);
    bool calculateState(SystemHealthIndex index) const;

    bool update(SystemHealthIndex index) { return setState(index, calculateState(index)); }

    DefaultPasswordCamerasWatcher* defaultPasswordWatcher() const;
    bool hasPowerUserPermissions() const;

    void updateCamerasWithDefaultPassword();
    void updateCamerasWithInvalidSchedule();
    void updateUsersWithInvalidEmail();
    void updateServersWithoutStorages();
    void updateServersWithMetadataStorageIssues();
    void updateSaasIssues();

private:
    std::array<bool, (int) SystemHealthIndex::count> m_state;
    QHash<SystemHealthIndex, QSet<QnResourcePtr>> m_resourcesForMessageType;
    std::unique_ptr<InvalidRecordingScheduleWatcher> m_invalidRecordingScheduleWatcher;
    std::unique_ptr<SystemInternetAccessWatcher> m_systemInternetAccessWatcher;
    std::unique_ptr<UserEmailsWatcher> m_userEmailsWatcher;
};

// ------------------------------------------------------------------------------------------------
// SystemHealthState::Private definition.

SystemHealthState::Private::Private(SystemHealthState* q):
    SystemContextAware(q->systemContext()),
    q(q),
    m_state(),
    m_invalidRecordingScheduleWatcher(
        std::make_unique<InvalidRecordingScheduleWatcher>(systemContext())),
    m_systemInternetAccessWatcher(std::make_unique<SystemInternetAccessWatcher>(systemContext())),
    m_userEmailsWatcher(std::make_unique<UserEmailsWatcher>(systemContext()))
{
#define updateSlot(index) [this]() { return update(SystemHealthIndex::index); }

    connect(systemContext()->userWatcher(), &core::UserWatcher::userChanged, q,
        [this]()
        {
            update(SystemHealthIndex::noLicenses);
            update(SystemHealthIndex::smtpIsNotSet);
            update(SystemHealthIndex::emailIsEmpty);
            update(SystemHealthIndex::cloudPromo);
            update(SystemHealthIndex::metadataStorageNotSet);
            update(SystemHealthIndex::metadataOnSystemStorage);
            updateCamerasWithDefaultPassword();
            updateCamerasWithInvalidSchedule();
            updateUsersWithInvalidEmail();
            updateSaasIssues();
        });

    // NoLicenses.

    connect(systemContext()->licensePool(),
        &QnLicensePool::licensesChanged,
        q,
        updateSlot(noLicenses));

    update(SystemHealthIndex::noLicenses);

    // SmtpIsNotSet.
    connect(systemSettings(), &SystemSettings::emailSettingsChanged,
        q, updateSlot(smtpIsNotSet));
    update(SystemHealthIndex::smtpIsNotSet);

    // NoInternetForTimeSync.
    m_systemInternetAccessWatcher->start();
    connect(m_systemInternetAccessWatcher.get(),
        &SystemInternetAccessWatcher::internetAccessChanged,
        q, updateSlot(noInternetForTimeSync));

    connect(systemSettings(), &SystemSettings::timeSynchronizationSettingsChanged,
        q, updateSlot(noInternetForTimeSync));

    const auto messageProcessor = clientMessageProcessor();
    connect(messageProcessor, &QnCommonMessageProcessor::initialResourcesReceived, q,
        [this]()
        {
            update(SystemHealthIndex::noInternetForTimeSync);
            updateServersWithoutStorages();
        });

    connect(messageProcessor, &QnCommonMessageProcessor::connectionClosed,
        q, updateSlot(noInternetForTimeSync));

    update(SystemHealthIndex::noInternetForTimeSync);

    // DefaultCameraPasswords.

    NX_ASSERT(defaultPasswordWatcher(), "Default password cameras watcher is not initialized");

    connect(defaultPasswordWatcher(), &DefaultPasswordCamerasWatcher::cameraSetChanged, q,
        [this]() { updateCamerasWithDefaultPassword(); });

    updateCamerasWithDefaultPassword();

    // InvalidRecordingSchedule.

    connect(m_invalidRecordingScheduleWatcher.get(),
        &InvalidRecordingScheduleWatcher::camerasChanged,
        q,
        [this]() { updateCamerasWithInvalidSchedule(); });

    updateCamerasWithInvalidSchedule();

    // EmailIsEmpty.

    connect(m_userEmailsWatcher.get(), &UserEmailsWatcher::userSetChanged,
        q, updateSlot(emailIsEmpty));
    update(SystemHealthIndex::emailIsEmpty);

    // UsersEmailIsEmpty.

    connect(m_userEmailsWatcher.get(), &UserEmailsWatcher::userSetChanged, q,
        [this]() { updateUsersWithInvalidEmail(); });

    updateUsersWithInvalidEmail();

    // StoragesNotConfigured.
    connect(runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoAdded, q,
        [this]() { updateServersWithoutStorages(); });

    connect(runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoRemoved, q,
        [this]() { updateServersWithoutStorages(); });

    connect(runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoChanged, q,
        [this]() { updateServersWithoutStorages(); });

    updateServersWithoutStorages();

    // CloudPromo.

    connect(&showOnceSettings()->cloudPromo, &ShowOnceSettings::BaseProperty::changed,
        q,
        [this](nx::utils::property_storage::BaseProperty* /*property*/)
        {
            update(SystemHealthIndex::cloudPromo);
        });

    update(SystemHealthIndex::cloudPromo);

    // Metadata storage issues.

    connect(qnServerStorageManager, &QnServerStorageManager::storageChanged,
        [this]() { updateServersWithMetadataStorageIssues(); });

    connect(qnServerStorageManager, &QnServerStorageManager::activeMetadataStorageChanged,
        [this]() { updateServersWithMetadataStorageIssues(); });

    update(SystemHealthIndex::metadataStorageNotSet);
    update(SystemHealthIndex::metadataOnSystemStorage);

    // SaaS related issues.

    connect(systemContext()->saasServiceManager(), &saas::ServiceManager::dataChanged, q,
        [this] { updateSaasIssues(); } );
    updateSaasIssues();

#undef updateSlot
}

void SystemHealthState::Private::updateCamerasWithDefaultPassword()
{
    const auto camerasWithDefaultPassword = defaultPasswordWatcher()->camerasWithDefaultPassword();

    QSet<QnResourcePtr> resourceSet;
    for (const auto& camera: camerasWithDefaultPassword)
        resourceSet.insert(camera);

    setResourcesForMessageType(SystemHealthIndex::defaultCameraPasswords, resourceSet);
}

void SystemHealthState::Private::updateCamerasWithInvalidSchedule()
{
    const auto camerasWithInvalidSchedule =
        m_invalidRecordingScheduleWatcher->camerasWithInvalidSchedule();

    QSet<QnResourcePtr> resourceSet;
    for (const auto& camera: camerasWithInvalidSchedule)
    {
        if (accessController()->hasPermissions(camera, Qn::ReadWriteSavePermission))
            resourceSet.insert(camera);
    }

    setResourcesForMessageType(SystemHealthIndex::cameraRecordingScheduleIsInvalid, resourceSet);
}

void SystemHealthState::Private::updateUsersWithInvalidEmail()
{
    const auto usersWithInvalidEmail = m_userEmailsWatcher->usersWithInvalidEmail();

    QSet<QnResourcePtr> resourceSet;
    for (const auto& user: usersWithInvalidEmail)
    {
        if (user != this->user()
            && accessController()->hasPermissions(user, Qn::WriteEmailPermission))
        {
            resourceSet.insert(user);
        }
    }

    setResourcesForMessageType(SystemHealthIndex::usersEmailIsEmpty, resourceSet);
}

void SystemHealthState::Private::updateServersWithoutStorages()
{
    const auto calculateServersWithoutStorages =
        [this](auto flag) -> QSet<QnResourcePtr>
        {
            QSet<QnUuid> serverIds;
            const auto items = runtimeInfoManager()->items()->getItems();
            for (const auto& item: items)
            {
                if (item.data.flags.testFlag(flag))
                    serverIds.insert(item.uuid);
            }

            const auto servers =
                resourcePool()->getResourcesByIds<QnMediaServerResource>(serverIds);

            return QSet<QnResourcePtr>(servers.cbegin(), servers.cend());
        };

    setResourcesForMessageType(SystemHealthIndex::storagesNotConfigured,
        calculateServersWithoutStorages(nx::vms::api::RuntimeFlag::noStorages));

    setResourcesForMessageType(SystemHealthIndex::backupStoragesNotConfigured,
        calculateServersWithoutStorages(nx::vms::api::RuntimeFlag::noBackupStorages));
}

void SystemHealthState::Private::updateServersWithMetadataStorageIssues()
{
    QSet<QnResourcePtr> serversWithoutMetadataStorage;
    QSet<QnResourcePtr> serversWithMetadataOnSystemStorage;

    const auto servers = resourcePool()->servers();
    for (const auto& server: servers)
    {
        const auto serverMetadataStorageId = server->metadataStorageId();
        if (serverMetadataStorageId.isNull())
        {
            serversWithoutMetadataStorage.insert(server);
            continue;
        }

        const auto serverStorages = server->getStorages();
        for (const auto& storage: serverStorages)
        {
            if (storage->persistentStatusFlags().testFlag(nx::vms::api::StoragePersistentFlag::system)
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

void SystemHealthState::Private::updateSaasIssues()
{
    update(SystemHealthIndex::saasLocalRecordingServicesOverused);
    update(SystemHealthIndex::saasCloudStorageServicesOverused);
    update(SystemHealthIndex::saasIntegrationServicesOverused);
    update(SystemHealthIndex::saasInSuspendedState);
    update(SystemHealthIndex::saasInShutdownState);
}

bool SystemHealthState::Private::state(SystemHealthIndex index) const
{
    return m_state[(int) index];
}

bool SystemHealthState::Private::setState(SystemHealthIndex index, bool value)
{
    if (state(index) == value)
        return false;

    m_state[(int) index] = value;
    emit q->stateChanged(index, value, SystemHealthState::QPrivateSignal());

    return true;
}

bool SystemHealthState::Private::calculateState(SystemHealthIndex index) const
{
    if (!user())
        return false;

    switch (index)
    {
        case SystemHealthIndex::noInternetForTimeSync:
            return hasPowerUserPermissions()
                && systemSettings()->isTimeSynchronizationEnabled()
                && systemSettings()->primaryTimeServer().isNull()
                && !m_systemInternetAccessWatcher->systemHasInternetAccess();

        case SystemHealthIndex::noLicenses:
            return hasPowerUserPermissions() && systemContext()->licensePool()->isEmpty();

        case SystemHealthIndex::smtpIsNotSet:
            return hasPowerUserPermissions()
                && !(systemSettings()->emailSettings().isValid()
                    || systemSettings()->emailSettings().useCloudServiceToSendEmail);

        case SystemHealthIndex::defaultCameraPasswords:
            return hasPowerUserPermissions() && hasResourcesForMessageType(index);

        case SystemHealthIndex::emailIsEmpty:
        {
            const auto user = this->user();
            return user && m_userEmailsWatcher->usersWithInvalidEmail().contains(user)
                && accessController()->hasPermissions(user, Qn::WriteEmailPermission);
        }

        case SystemHealthIndex::usersEmailIsEmpty:
            return hasResourcesForMessageType(index);

        case SystemHealthIndex::cloudPromo:
            return user()
                && user()->isAdministrator()
                && systemSettings()->cloudSystemId().isEmpty()
                && !showOnceSettings()->cloudPromo();

        case SystemHealthIndex::storagesNotConfigured:
            return hasPowerUserPermissions() && hasResourcesForMessageType(index);

        case SystemHealthIndex::backupStoragesNotConfigured:
            return hasPowerUserPermissions() && hasResourcesForMessageType(index);

        case SystemHealthIndex::cameraRecordingScheduleIsInvalid:
            return hasResourcesForMessageType(index);

        case SystemHealthIndex::metadataStorageNotSet:
            return hasPowerUserPermissions() && hasResourcesForMessageType(index);

        case SystemHealthIndex::metadataOnSystemStorage:
            return hasPowerUserPermissions() && hasResourcesForMessageType(index);

        case SystemHealthIndex::saasLocalRecordingServicesOverused:
            return hasPowerUserPermissions()
                && saas::saasInitialized(systemContext())
                && saas::localRecordingServicesOverused(systemContext());

        case SystemHealthIndex::saasCloudStorageServicesOverused:
            return hasPowerUserPermissions()
                && saas::saasInitialized(systemContext())
                && saas::cloudStorageServicesOverused(systemContext());

        case SystemHealthIndex::saasIntegrationServicesOverused:
            return hasPowerUserPermissions()
                && saas::saasInitialized(systemContext())
                && saas::integrationServicesOverused(systemContext());

        case SystemHealthIndex::saasInSuspendedState:
            return hasPowerUserPermissions()
                && systemContext()->saasServiceManager()->saasState()
                    == nx::vms::api::SaasState::suspend;

        case SystemHealthIndex::saasInShutdownState:
            return hasPowerUserPermissions()
                && systemContext()->saasServiceManager()->saasShutDown();

        default:
            NX_ASSERT(false, "This system health index is not handled by SystemHealthState");
            return false;
    }
}

QVariant SystemHealthState::Private::data(SystemHealthIndex index) const
{
    if (!m_state[(int) index])
        return {};

    switch (index)
    {
        case SystemHealthIndex::usersEmailIsEmpty:
        case SystemHealthIndex::storagesNotConfigured:
        case SystemHealthIndex::defaultCameraPasswords:
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
            emit q->dataChanged(message, SystemHealthState::QPrivateSignal());
    }
}

bool SystemHealthState::Private::hasPowerUserPermissions() const
{
    return accessController()->hasPowerUserPermissions();
}

DefaultPasswordCamerasWatcher* SystemHealthState::Private::defaultPasswordWatcher() const
{
    return systemContext()->defaultPasswordCamerasWatcher();
}

// ------------------------------------------------------------------------------------------------
// SystemHealthState definition.

SystemHealthState::SystemHealthState(SystemContext* systemContext, QObject* parent):
    QObject(parent),
    SystemContextAware(systemContext),
    d(new Private(this))
{
    NX_CRITICAL(messageProcessor());
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
