// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_source/abstract_resource_source.h>

class QnResourcePool;

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

class LayoutResourceIndex;

class UserLayoutsSource: public AbstractResourceSource
{
    Q_OBJECT
    using base_type = AbstractResourceSource;

public:
    UserLayoutsSource(
        const LayoutResourceIndex* layoutResourceIndex,
        const QnUserResourcePtr& parentUser);

    virtual QVector<QnResourcePtr> getResources() override;

private:
    void onLayoutAdded(const QnResourcePtr& layout, const QnUuid& parentId);
    void onLayoutRemoved(const QnResourcePtr& layout, const QnUuid& parentId);
    void holdLocalLayout(const QnResourcePtr& layout);
    void processLayout(const QnResourcePtr& layout, std::function<void()> successHandler);

private:
    const LayoutResourceIndex* m_layoutResourceIndex;
    QSet<QnResourcePtr> m_localLayouts;
    QnUserResourcePtr m_parentUser;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
