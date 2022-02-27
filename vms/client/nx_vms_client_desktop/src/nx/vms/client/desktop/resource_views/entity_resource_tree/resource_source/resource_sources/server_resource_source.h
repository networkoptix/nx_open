// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_source/abstract_resource_source.h>
#include <core/resource/resource_fwd.h>

class QnResourcePool;

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

class ServerResourceSource: public AbstractResourceSource
{
    Q_OBJECT
    using base_type = AbstractResourceSource;

public:
    ServerResourceSource(const QnResourcePool* resourcePool, bool displayReducedEdgeServers);

    virtual QVector<QnResourcePtr> getResources() override;

private:
    void setupEdgeServerStateTracking(const QnMediaServerResourcePtr& server) const;
    void updateEdgeServerReducedState(const QnResourcePtr& serverResource);

private:
    const QnResourcePool* m_resourcePool;
    const bool m_displayReducedEdgeServers;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
