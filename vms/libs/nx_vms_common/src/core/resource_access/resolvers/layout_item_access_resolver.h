// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>

#include "abstract_resource_access_resolver.h"

class QnResourcePool;

namespace nx::core::access {

/**
 * This class resolves an indirect access granted by shared layouts and videowall layouts
 * to their cameras, web pages and health monitors.
 */
class NX_VMS_COMMON_API LayoutItemAccessResolver: public AbstractResourceAccessResolver
{
    using base_type = AbstractResourceAccessResolver;

public:
    explicit LayoutItemAccessResolver(
        AbstractResourceAccessResolver* baseResolver,
        QnResourcePool* resourcePool,
        bool subjectEditingMode = false,
        QObject* parent = nullptr);

    virtual ~LayoutItemAccessResolver() override;

    virtual nx::vms::api::AccessRights accessRights(const nx::Uuid& subjectId,
        const QnResourcePtr& resource) const override;

    virtual ResourceAccessMap resourceAccessMap(const nx::Uuid& subjectId) const override;

    virtual nx::vms::api::GlobalPermissions globalPermissions(const nx::Uuid& subjectId) const override;

    virtual ResourceAccessDetails accessDetails(
        const nx::Uuid& subjectId,
        const QnResourcePtr& resource,
        nx::vms::api::AccessRight accessRight) const override;

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::core::access
