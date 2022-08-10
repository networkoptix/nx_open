// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource_access/providers/base_resource_access_provider.h>

namespace nx::core::access {

/**
 * Handles access to intercom layouts.
 * A layout is accessible if parent intercom is accessible.
 */
class NX_VMS_COMMON_API IntercomLayoutAccessProvider: public BaseResourceAccessProvider
{
    using base_type = BaseResourceAccessProvider;

public:
    IntercomLayoutAccessProvider(
        Mode mode,
        nx::vms::common::SystemContext* context,
        QObject* parent = nullptr);
    virtual ~IntercomLayoutAccessProvider();

protected:
    virtual Source baseSource() const override;

    virtual bool calculateAccess(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource,
        nx::vms::api::GlobalPermissions globalPermissions) const override;

    virtual void fillProviders(
        const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource,
        QnResourceList& providers) const override;

    virtual void handleResourceAdded(const QnResourcePtr& resource) override;
    virtual void handleResourceRemoved(const QnResourcePtr& resource) override;
};

} // namespace nx::core::access
