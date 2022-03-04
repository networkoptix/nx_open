// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource_access/providers/base_resource_access_provider.h>

namespace nx::core::access {

/**
 * Handles access via global permissions.
 */
class NX_VMS_COMMON_API PermissionsResourceAccessProvider: public BaseResourceAccessProvider
{
    using base_type = BaseResourceAccessProvider;

public:
    PermissionsResourceAccessProvider(
        Mode mode,
        nx::vms::common::ResourceContext* context,
        QObject* parent = nullptr);
    virtual ~PermissionsResourceAccessProvider();

protected:
    virtual Source baseSource() const override;

    virtual bool calculateAccess(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource, nx::vms::api::GlobalPermissions globalPermissions) const override;

    virtual void handleResourceAdded(const QnResourcePtr& resource) override;

private:
    bool hasAccessToDesktopCamera(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const;
};

} // namespace nx::core::access
