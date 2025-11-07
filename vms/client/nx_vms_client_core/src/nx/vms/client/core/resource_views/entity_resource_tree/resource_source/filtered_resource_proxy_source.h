// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <nx/vms/client/core/resource_views/entity_resource_tree/resource_source/abstract_resource_source.h>

namespace nx::vms::client::core {
namespace entity_resource_tree {

class NX_VMS_CLIENT_CORE_API FilteredResourceProxySource: public AbstractResourceSource
{
    Q_OBJECT
    using base_type = AbstractResourceSource;

public:
    FilteredResourceProxySource(
        const std::function<bool(const QnResourcePtr&)>& resourceFilter,
        const std::unique_ptr<AbstractResourceSource> baseResourceSource);

    virtual ~FilteredResourceProxySource() override;
    virtual QVector<QnResourcePtr> getResources() override;

private:
    const std::function<bool(const QnResourcePtr&)> m_resourceFilter;
    const std::unique_ptr<AbstractResourceSource> m_baseResourceSource;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::core
