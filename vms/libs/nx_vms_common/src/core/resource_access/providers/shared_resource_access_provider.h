// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource_access/providers/base_resource_access_provider.h>

namespace nx::core::access
{

/**
 * Provides access to directly shared resources.
 */
class NX_VMS_COMMON_API SharedResourceAccessProvider: public BaseResourceAccessProvider
{
    using base_type = BaseResourceAccessProvider;

public:
    SharedResourceAccessProvider(
        Mode mode,
        nx::vms::common::SystemContext* context,
        QObject* parent = nullptr);
    virtual ~SharedResourceAccessProvider();

protected:
    virtual Source baseSource() const override;

    virtual bool calculateAccess(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource,
        nx::vms::api::GlobalPermissions globalPermissions) const override;

    virtual void handleResourceAdded(const QnResourcePtr& resource) override;

private:
    void handleSharedResourcesChanged(
        const QnResourceAccessSubject& subject,
        const std::map<QnUuid, nx::vms::api::AccessRights>& oldValues,
        const std::map<QnUuid, nx::vms::api::AccessRights>& newValues);
};

} // namespace nx::core::access
