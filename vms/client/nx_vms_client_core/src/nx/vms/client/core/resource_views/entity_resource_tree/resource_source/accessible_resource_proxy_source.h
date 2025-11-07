// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <core/resource/resource_fwd.h>
#include <core/resource_access/resource_access_subject.h>
#include <nx/vms/client/core/resource_views/entity_resource_tree/resource_source/abstract_resource_source.h>
#include <nx/vms/client/core/system_context_aware.h>

namespace nx::vms::client::core {
namespace entity_resource_tree {

class NX_VMS_CLIENT_CORE_API AccessibleResourceProxySource:
    public AbstractResourceSource,
    public core::SystemContextAware
{
    Q_OBJECT
    using base_type = AbstractResourceSource;

public:
    AccessibleResourceProxySource(
        core::SystemContext* systemContext,
        const QnResourceAccessSubject& accessSubject,
        const std::unique_ptr<AbstractResourceSource> baseResourceSource);

    virtual ~AccessibleResourceProxySource() override;
    virtual QVector<QnResourcePtr> getResources() override;

private:
    bool hasAccess(const QnResourcePtr& resource) const;

private:
    QnResourceAccessSubject m_accessSubject;
    std::unique_ptr<AbstractResourceSource> m_baseResourceSource;
    QSet<QnResourcePtr> m_acceptedResources;
    QSet<QnResourcePtr> m_deniedResources;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::core
