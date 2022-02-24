// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/unique_key_source.h>

namespace nx::vms::client::desktop {

class CloudSystemCamerasWatcher;

namespace entity_resource_tree {

class CloudSystemCamerasSource: public entity_item_model::UniqueStringSource
{
public:
    CloudSystemCamerasSource(const QString& systemId);
    ~CloudSystemCamerasSource();

private:
    std::unique_ptr<CloudSystemCamerasWatcher> m_cloudSystemCamerasWatcher;
    nx::utils::ScopedConnections m_connectionsGuard;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
