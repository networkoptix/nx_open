// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_resource_source.h"

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

/**
 * Resource source which provides set of corresponding parent server resources deduced from set of
 * resources provided by encapsulated resource source.
 */
class ParentServersProxySource: public AbstractResourceSource
{
    Q_OBJECT
    using base_type = AbstractResourceSource;

public:
    ParentServersProxySource(AbstractResourceSourcePtr resourceSource);

    virtual QVector<QnResourcePtr> getResources() override;

private:
    void mapResource(const QnResourcePtr& resource);
    void unmapResource(const QnResourcePtr& resource, const QnResourcePtr& parentResource);
    void onResourceParentIdChanged(const QnResourcePtr& resource, const nx::Uuid& previousParentId);

private:
    std::unique_ptr<AbstractResourceSource> m_resourceSource;
    QHash<QnMediaServerResourcePtr, QSet<QnResourcePtr>> m_resourcesByServer;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
