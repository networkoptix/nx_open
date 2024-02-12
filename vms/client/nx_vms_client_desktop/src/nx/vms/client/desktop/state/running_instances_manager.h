// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

class LocalSettings;
class SharedMemoryManager;

/**
 * Generates runtime GUID for the current client instance. Provides information about another
 * running client instances on the same PC.
 */
class RunningInstancesManager
{
public:
    explicit RunningInstancesManager(LocalSettings* settings, SharedMemoryManager* memory);
    virtual ~RunningInstancesManager();

    nx::Uuid currentInstanceGuid() const;
    QList<nx::Uuid> runningInstancesGuids() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
