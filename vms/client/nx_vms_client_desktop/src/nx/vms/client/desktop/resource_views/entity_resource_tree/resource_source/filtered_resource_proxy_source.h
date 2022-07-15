// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_resource_source.h"

#include <functional>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

class FilteredResourceProxySource: public AbstractResourceSource
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
} // namespace nx::vms::client::desktop
