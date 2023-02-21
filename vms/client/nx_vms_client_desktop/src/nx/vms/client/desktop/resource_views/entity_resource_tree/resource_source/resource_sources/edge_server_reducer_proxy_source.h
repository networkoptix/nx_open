// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <core/resource_access/resource_access_subject.h>
#include <nx/vms/client/desktop/system_context_aware.h>

#include "../abstract_resource_source.h"

namespace nx::vms::client::desktop {

class SystemContext;

namespace entity_resource_tree {

/**
 * A proxy source that replaces Edge servers with their cameras if they meet certain criteria:
 *    - the server has exactly one non-virtual camera with the same host address;
 *    - that camera is accessible to given access subject;
 *    - redundancy is turned off.
 */
class EdgeServerReducerProxySource:
    public AbstractResourceSource,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = AbstractResourceSource;

public:
    explicit EdgeServerReducerProxySource(
        SystemContext* systemContext,
        const QnResourceAccessSubject& subject,
        std::unique_ptr<AbstractResourceSource> serversSource);

    virtual QVector<QnResourcePtr> getResources() override;

private:
    void setupEdgeServerStateTracking(const QnMediaServerResourcePtr& server) const;
    void updateEdgeServerReducedState(const QnResourcePtr& serverResource);
    QnVirtualCameraResourcePtr cameraToReduceTo(const QnMediaServerResourcePtr& server) const;
    bool hasAccess(const QnResourcePtr& resource) const;

private:
    const std::unique_ptr<AbstractResourceSource> m_serversSource;
    const QnResourceAccessSubject m_subject;
    QHash<QnMediaServerResourcePtr, QnVirtualCameraResourcePtr> m_reducedServerCameras;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
