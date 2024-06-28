// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "local_system_finder.h"

namespace nx::vms::client::core {

/**
 * Finder creates tiles for the systems, which were added to Favorites - or hidden - but are not
 * available anymore in any way. Tiles are displayed in the corresponding mode, so user can remove
 * the system from the list.
 */
class ScopeLocalSystemFinder: public LocalSystemFinder
{
    Q_OBJECT
    using base_type = LocalSystemFinder;

public:
    ScopeLocalSystemFinder(QObject* parent = nullptr);
    virtual ~ScopeLocalSystemFinder() = default;

private:
    virtual void updateSystems() override;
};

} // namespace nx::vms::client::core
