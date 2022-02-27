// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_source/abstract_resource_source.h>

class QnResourcePool;

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

class LayoutResourceSource: public AbstractResourceSource
{
    Q_OBJECT
    using base_type = AbstractResourceSource;

public:
    LayoutResourceSource(
        const QnResourcePool* resourcePool,
        const QnUserResourcePtr& parentUser,
        bool includeShared);

    virtual QVector<QnResourcePtr> getResources() override;

private:
    void onResourceAdded(const QnResourcePtr& resource);
    void onResourceRemoved(const QnResourcePtr& resource);
    void onLayoutParentIdChanged(const QnResourcePtr& resource, const QnUuid& previousParentId);
    void holdLocalLayout(const QnResourcePtr& resource);
    void processResource(const QnResourcePtr& resource, std::function<void()> successHandler);

private:
    const QnResourcePool* m_resourcePool;
    QSet<QnResourcePtr> m_localLayouts;
    QnUserResourcePtr m_parentUser;
    bool m_includeShared;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
