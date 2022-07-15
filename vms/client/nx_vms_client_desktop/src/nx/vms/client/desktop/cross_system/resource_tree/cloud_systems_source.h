// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/unique_key_source.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

class CloudSystemsSource: public nx::vms::client::desktop::entity_item_model::UniqueStringSource
{
public:
    CloudSystemsSource();

private:
    nx::utils::ScopedConnections m_connectionsGuard;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
