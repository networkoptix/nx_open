// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <core/resource/resource_fwd.h>
#include <core/resource_access/resource_access_subject.h>
#include <nx/vms/client/desktop/system_context_aware.h>

#include "abstract_resource_source.h"

namespace nx::core::access { class ResourceAccessProvider; }

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

class AccessibleResourceProxySource: public AbstractResourceSource, public SystemContextAware
{
    Q_OBJECT
    using base_type = AbstractResourceSource;

public:
    AccessibleResourceProxySource(
        SystemContext* systemContext,
        const nx::core::access::ResourceAccessProvider* accessProvider,
        const QnResourceAccessSubject& accessSubject,
        const std::unique_ptr<AbstractResourceSource> baseResourceSource);

    virtual ~AccessibleResourceProxySource() override;
    virtual QVector<QnResourcePtr> getResources() override;

private:
    const nx::core::access::ResourceAccessProvider* m_accessProvider;
    QnResourceAccessSubject m_accessSubject;
    std::unique_ptr<AbstractResourceSource> m_baseResourceSource;
    QSet<QnResourcePtr> m_acceptedResources;
    QSet<QnResourcePtr> m_deniedResources;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
