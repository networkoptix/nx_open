// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>

class QnClientSettings;

namespace nx::vms::client::desktop {

class SharedMemoryManager;

/**
 * Generates runtime GUID for the current client instance. Provides information about another
 * running client instances on the same PC.
 */
class RunningInstancesManager
{
public:
    explicit RunningInstancesManager(QnClientSettings* settings, SharedMemoryManager* memory);
    virtual ~RunningInstancesManager();

    QnUuid currentInstanceGuid() const;
    QList<QnUuid> runningInstancesGuids() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
