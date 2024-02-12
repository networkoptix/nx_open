// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "running_instances_manager.h"

#include <QtCore/QPointer>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/settings/local_settings.h>

#include "shared_memory_manager.h"

namespace nx::vms::client::desktop {

struct RunningInstancesManager::Private
{
    Private(LocalSettings* settings, SharedMemoryManager* memoryManager):
        memory(memoryManager)
    {
        initPcUuid(settings);
    }

    void initPcUuid(LocalSettings* settings)
    {
        if (!NX_ASSERT(settings))
            return;

        pcUuid = settings->pcUuid();
        if (pcUuid.isNull())
        {
            pcUuid = nx::Uuid::createUuid();
            settings->pcUuid = pcUuid;
        }
    }

    nx::Uuid instanceGuidForIndex(int index) const
    {
        return nx::Uuid::createUuidFromPool(pcUuid.getQUuid(), static_cast<uint>(index));
    }

    nx::Uuid pcUuid = nx::Uuid::createUuid();
    QPointer<SharedMemoryManager> memory;
};

RunningInstancesManager::RunningInstancesManager(
    LocalSettings* settings,
    SharedMemoryManager* memory)
    :
    d(new Private(settings, memory))
{
}

RunningInstancesManager::~RunningInstancesManager()
{
}

nx::Uuid RunningInstancesManager::currentInstanceGuid() const
{
    if (NX_ASSERT(d->memory))
        return d->instanceGuidForIndex(d->memory->currentInstanceIndex());
    return d->pcUuid;
}

QList<nx::Uuid> RunningInstancesManager::runningInstancesGuids() const
{
    if (!NX_ASSERT(d->memory))
        return QList<nx::Uuid>{d->pcUuid};

    QList<int> runningInstancesIndices = d->memory->runningInstancesIndices();
    QList<nx::Uuid> result;
    for (auto index: runningInstancesIndices)
        result.push_back(d->instanceGuidForIndex(index));
    return result;
}

} // namespace nx::vms::client::desktop
