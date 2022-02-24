// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_source/abstract_resource_source.h>

#include <core/resource_access/resource_access_subject.h>
#include <nx/core/access/access_types.h>

class QnResourcePool;
class QnGlobalPermissionsManager;
namespace nx::core::access { class ResourceAccessProvider; }

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

class UserAccessibleDevicesSource: public AbstractResourceSource
{
    Q_OBJECT
    using base_type = AbstractResourceSource;

public:
    UserAccessibleDevicesSource(
        const QnResourcePool* resourcePool,
        const QnGlobalPermissionsManager* globalPermissionsManager,
        const nx::core::access::ResourceAccessProvider* resourceAccessProvider,
        const QnResourceAccessSubject& accessSubject);

    virtual QVector<QnResourcePtr> getResources() override;

private:
    bool filter(const QnResourcePtr& resource) const;

    void onResourceAccessChanged(
        const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource,
        nx::core::access::Source source);

private:
    const QnResourcePool* m_resourcePool = nullptr;
    const QnGlobalPermissionsManager* m_globalPermissionsManager = nullptr;
    const nx::core::access::ResourceAccessProvider* m_accessProvider = nullptr;

    QMetaObject::Connection m_accessProviderConnection;
    QnResourceAccessSubject m_accessSubject;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
