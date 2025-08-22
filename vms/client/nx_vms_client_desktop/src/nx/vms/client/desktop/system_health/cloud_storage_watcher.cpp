// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_storage_watcher.h"

#include <core/resource/storage_resource.h>
#include <nx/vms/client/core/resource/session_resources_signal_listener.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop {

CloudStorageWatcher::CloudStorageWatcher(SystemContext* systemContext, QObject* parent):
    QObject(parent),
    SystemContextAware(systemContext)
{
    auto storageChangesListener = new core::SessionResourcesSignalListener<QnStorageResource>(
        systemContext,
        this);

    const auto check =
        [this](const QnStorageResourcePtr& storage)
        {
            if (!storage->isCloudStorage())
                return;

            auto enabledCount = m_enabledCloudStorageCount;
            if (storage->isUsedForWriting())
                enabledCount++;
            else
                enabledCount--;

            update(m_availableCloudStorageCount, enabledCount);
        };
    storageChangesListener->addOnSignalHandler(&QnStorageResource::isUsedForWritingChanged, check);
    storageChangesListener->setOnAddedHandler(
        [this](const QnStorageResourceList& storages)
        {
            auto availableCount = m_availableCloudStorageCount;
            auto enabledCount = m_enabledCloudStorageCount;
            for (const auto& storage: storages)
            {
                if (!storage->isCloudStorage())
                    continue;

                availableCount++;
                if(storage->isUsedForWriting())
                    enabledCount++;
            }

            update(availableCount, enabledCount);
        });

    storageChangesListener->setOnRemovedHandler(
        [this](const QnStorageResourceList& storages)
        {
            auto availableCount = m_availableCloudStorageCount;
            auto enabledCount = m_enabledCloudStorageCount;
            for (const auto& storage: storages)
            {
                if (!storage->isCloudStorage())
                    continue;

                availableCount--;
                if(storage->isUsedForWriting())
                    enabledCount--;
            }

            update(availableCount, enabledCount);
        });

    storageChangesListener->start();

    auto serversChangesListener = new core::SessionResourcesSignalListener<QnMediaServerResource>(
        systemContext,
        this);

    serversChangesListener->setOnAddedHandler(
        [this]([[maybe_unused]] const QnMediaServerResourceList& servers)
        { emit cloudStorageStateChanged(); });

    serversChangesListener->setOnRemovedHandler(
        [this]([[maybe_unused]] const QnMediaServerResourceList& servers)
        { emit cloudStorageStateChanged(); });
}

CloudStorageWatcher::~CloudStorageWatcher()
{
}

void CloudStorageWatcher::update(int availableCount, int enabledCount)
{
    if (m_availableCloudStorageCount != availableCount || m_enabledCloudStorageCount != enabledCount)
    {
        m_availableCloudStorageCount = availableCount;
        m_enabledCloudStorageCount = enabledCount;
        emit cloudStorageStateChanged();
    }
}

int CloudStorageWatcher::enabledCloudStorageCount() const
{
    return m_enabledCloudStorageCount;
}

int CloudStorageWatcher::availableCloudStorageCount() const
{
    return m_availableCloudStorageCount;
}

} // namespace nx::vms::client::desktop
