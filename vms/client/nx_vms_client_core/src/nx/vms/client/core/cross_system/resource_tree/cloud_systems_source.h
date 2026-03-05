// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/resource_views/entity_item_model/entity/unique_key_source.h>

namespace nx::vms::client::core {
namespace entity_resource_tree {

class NX_VMS_CLIENT_CORE_API CloudSystemsSource: public core::entity_item_model::UniqueStringSource
{
public:
    CloudSystemsSource(nx::Uuid organizationId = {});

private:
    nx::utils::ScopedConnections m_connectionsGuard;
    nx::Uuid m_organizationId;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
