// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_settings_backup_storages_watcher.h"

#include <QtCore/QPointer>

#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <nx/vms/api/data/storage_flags.h>
#include <nx/vms/api/data/storage_space_data.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/resource_properties/server/flux/server_settings_dialog_store.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/saas/saas_utils.h>
#include <server/server_storage_manager.h>

namespace {

bool hasActiveBackupStorage(const QnMediaServerResourcePtr& server)
{
    if (server.isNull())
        return false;

    const auto serverStorages = server->getStorages();
    return std::any_of(std::cbegin(serverStorages), std::cend(serverStorages),
        [](const auto& storageResource)
        {
            const auto statusFlags = storageResource->runtimeStatusFlags();
            return !statusFlags.testFlag(nx::vms::api::StorageRuntimeFlag::disabled)
                && storageResource->isBackup()
                && storageResource->isUsedForWriting();
        });
}

bool usesCloudBackupStorage(const QnMediaServerResourcePtr& server)
{
    if (server.isNull())
        return false;

    const bool emulateCloudStorage = nx::vms::common::saas::saasInitialized(server->systemContext())
        && nx::vms::client::desktop::ini().emulateCloudBackupSettingsOnNonCloudStorage;

    const auto serverStorages = server->getStorages();
    return std::any_of(std::cbegin(serverStorages), std::cend(serverStorages),
        [emulateCloudStorage](const auto& storageResource)
        {
            const auto statusFlags = storageResource->runtimeStatusFlags();
            return !statusFlags.testFlag(nx::vms::api::StorageRuntimeFlag::disabled)
                && storageResource->isBackup()
                && storageResource->isUsedForWriting()
                && ((storageResource->getStorageType() == nx::vms::api::kCloudStorageType)
                    || emulateCloudStorage);
        });
}

int enabledNonCloudStoragesCount(const QnMediaServerResourcePtr& server)
{
    if (server.isNull())
        return 0;

    const auto serverStorages = server->getStorages();
    return std::count_if(std::cbegin(serverStorages), std::cend(serverStorages),
        [](const auto& storageResource)
        {
            const auto statusFlags = storageResource->runtimeStatusFlags();
            return !statusFlags.testFlag(nx::vms::api::StorageRuntimeFlag::disabled)
                && storageResource->isUsedForWriting()
                && storageResource->getStorageType() != nx::vms::api::kCloudStorageType;
        });
}

} // namespace

namespace nx::vms::client::desktop {

// ------------------------------------------------------------------------------------------------
// ServerSettingsBackupStoragesWatcher::Private declaration

struct ServerSettingsBackupStoragesWatcher::Private
{
    ServerSettingsBackupStoragesWatcher* const q;
    QPointer<ServerSettingsDialogStore> store;
    QnMediaServerResourcePtr server;
    QPointer<QnServerStorageManager> storageManager;

    void updateStoragesState();
};

// ------------------------------------------------------------------------------------------------
// ServerSettingsBackupStoragesWatcher::Private definition

void ServerSettingsBackupStoragesWatcher::Private::updateStoragesState()
{
    if (!store)
        return;

    ServerSettingsDialogState::BackupStoragesStatus storagesStatus{
        hasActiveBackupStorage(server),
        usesCloudBackupStorage(server),
        enabledNonCloudStoragesCount(server)};

    store->setBackupStoragesStatus(storagesStatus);
}

// ------------------------------------------------------------------------------------------------
// ServerSettingsBackupStoragesWatcher::Private definition

ServerSettingsBackupStoragesWatcher::ServerSettingsBackupStoragesWatcher(
    ServerSettingsDialogStore* store,
    QObject* parent)
    :
    base_type(parent),
    d(new Private({this, store}))
{
}

ServerSettingsBackupStoragesWatcher::~ServerSettingsBackupStoragesWatcher()
{
}

void ServerSettingsBackupStoragesWatcher::setServer(const QnMediaServerResourcePtr& server)
{
    if (d->server == server)
        return;

    d->server = server;

    if (!d->server)
    {
        if (d->storageManager)
            d->storageManager->disconnect(this);

        d->storageManager.clear();
    }
    else if (!d->storageManager)
    {
        d->storageManager = SystemContext::fromResource(d->server)->serverStorageManager();

        connect(d->storageManager, &QnServerStorageManager::storageAdded, this,
            [this]() { d->updateStoragesState(); });

        connect(d->storageManager, &QnServerStorageManager::storageChanged, this,
            [this]() { d->updateStoragesState(); });

        connect(d->storageManager, &QnServerStorageManager::storageRemoved, this,
            [this]() { d->updateStoragesState(); });

        d->updateStoragesState();
    }
}

} // nx::vms::client::desktop
