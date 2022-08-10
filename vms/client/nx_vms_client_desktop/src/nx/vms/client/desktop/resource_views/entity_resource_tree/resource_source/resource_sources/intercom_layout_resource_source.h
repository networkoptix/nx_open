// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_source/abstract_resource_source.h>

class QnResourcePool;

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

class IntercomLayoutResourceSource: public AbstractResourceSource
{
    Q_OBJECT
    using base_type = AbstractResourceSource;

public:
    IntercomLayoutResourceSource(const QnResourcePool* resourcePool);

    virtual QVector<QnResourcePtr> getResources() override;

private:
    void onResourcesAdded(const QnResourceList& resources);
    void onResourcesRemoved(const QnResourceList& resources);
    bool processResource(const QnResourcePtr& resource);

private:
    const QnResourcePool* m_resourcePool;
    QSet<QnResourcePtr> m_intercomLocalLayouts;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
