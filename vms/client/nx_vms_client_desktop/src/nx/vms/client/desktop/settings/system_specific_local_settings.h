// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <set>

#include <QtCore/QString>

#include <nx/utils/property_storage/storage.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

/**
 * Settings which are stored locally on the client side but specific to the given System.
 */
class SystemSpecificLocalSettings:
    public nx::utils::property_storage::Storage,
    public SystemContextAware
{
    Q_OBJECT

public:
    SystemSpecificLocalSettings(SystemContext* systemContext);

    /** List of muted layout item ids.
     *
     * Note that the fact that we're only saving item ids here means that we need to GC them
     * somehow. We do this on save just by checking the ids that we have agains all items in
     * the resource pool. This works because even live viewers have access to layout resources
     * for all other users (even though they don't see them in the UI). */
    Property<std::set<QnUuid>> mutedItemIds{this, "mutedItemIds"};
};

} // namespace nx::vms::client::desktop
