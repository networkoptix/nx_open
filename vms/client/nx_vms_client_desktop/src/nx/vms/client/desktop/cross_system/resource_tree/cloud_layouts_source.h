// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/unique_key_source.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

class CloudLayoutsSource: public entity_item_model::UniqueResourceSource
{
public:
    CloudLayoutsSource();

private:
    nx::utils::ScopedConnections m_connectionsGuard;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
