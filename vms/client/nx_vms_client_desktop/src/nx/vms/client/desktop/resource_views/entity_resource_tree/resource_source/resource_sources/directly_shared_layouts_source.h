// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_source/abstract_resource_source.h>
#include <core/resource_access/resource_access_subject.h>

class QnGlobalPermissionsManager;
class QnSharedResourcesManager;

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

class DirectlySharedLayoutsSource: public AbstractResourceSource
{
    Q_OBJECT
    using base_type = AbstractResourceSource;

public:
    DirectlySharedLayoutsSource(
        const QnResourcePool* resourcePool,
        const QnGlobalPermissionsManager* globalPermissionsManager,
        const QnSharedResourcesManager* sharedResourcesManager,
        const QnResourceAccessSubject& accessSubject);

    virtual QVector<QnResourcePtr> getResources() override;

private:
    const QnResourcePool* m_resourcePool = nullptr;
    const QnGlobalPermissionsManager* m_globalPermissionsManager = nullptr;
    const QnSharedResourcesManager* m_sharedResourcesManager = nullptr;
    QnResourceAccessSubject m_accessSubject;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
