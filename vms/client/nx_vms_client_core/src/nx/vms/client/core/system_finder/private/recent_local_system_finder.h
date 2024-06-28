// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "local_system_finder.h"

namespace nx::vms::client::core {

/**
 * Handles recent connections, stored in the coreSettings()->recentLocalConnections property.
 */
class RecentLocalSystemFinder: public LocalSystemFinder
{
    Q_OBJECT
    using base_type = LocalSystemFinder;

public:
    RecentLocalSystemFinder(QObject* parent = nullptr);
    virtual ~RecentLocalSystemFinder() = default;

private:
    virtual void updateSystems();

};

} // namespace nx::vms::client::core
