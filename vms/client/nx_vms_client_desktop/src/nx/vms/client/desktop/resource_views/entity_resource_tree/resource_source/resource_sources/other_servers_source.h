// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/unique_key_source.h>

namespace nx::vms::client::desktop {

class OtherServersManager;

namespace entity_resource_tree {

/**
 * OtherServersSource is a source for "Other server" resource tree nodes.
 */

class NX_VMS_CLIENT_DESKTOP_API OtherServersSource:
    public QObject,
    public entity_item_model::UniqueKeySource<QnUuid>
{
    Q_OBJECT

public:
    OtherServersSource(const OtherServersManager* otherServersManager, QObject* parent = nullptr);
    virtual ~OtherServersSource() override;

private:
    const OtherServersManager* m_otherServerManager;
    nx::utils::ScopedConnections m_connectionsGuard;
};

using UniqueUuidSourcePtr = std::shared_ptr<entity_item_model::UniqueKeySource<QnUuid>>;

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
