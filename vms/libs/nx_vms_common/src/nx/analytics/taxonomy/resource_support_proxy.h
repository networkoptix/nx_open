// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/analytics/taxonomy/abstract_resource_support_proxy.h>
#include <nx/utils/impl_ptr.h>

class QnResourcePool;

namespace nx::vms::common { class SystemContext; }

namespace nx::analytics::taxonomy {

class NX_VMS_COMMON_API ResourceSupportProxy: public AbstractResourceSupportProxy
{
public:
    ResourceSupportProxy(nx::vms::common::SystemContext* systemContext);

    ~ResourceSupportProxy();

    virtual bool isEntityTypeSupported(
        EntityType entityType,
        const QString& entityTypeId,
        nx::Uuid deviceId,
        nx::Uuid engineId) const override;

    virtual bool isEntityTypeAttributeSupported(
        EntityType entityType,
        const QString& entityTypeId,
        const QString& fullAttributeName,
        nx::Uuid deviceId,
        nx::Uuid engineId) const override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::analytics::taxonomy
