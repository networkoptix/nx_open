// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>

#include "abstract_resource_access_resolver.h"

namespace nx::core::access {

class AccessRightsManager;
class SubjectHierarchy;

/**
 * This class resolves a resource access inherited from parent groups.
 */
class NX_VMS_COMMON_API InheritedResourceAccessResolver: public AbstractResourceAccessResolver
{
    using base_type = AbstractResourceAccessResolver;

public:
    explicit InheritedResourceAccessResolver(
        AbstractResourceAccessResolver* baseResolver,
        SubjectHierarchy* subjectHierarchy,
        QObject* parent = nullptr);

    virtual ~InheritedResourceAccessResolver() override;

    virtual ResourceAccessMap resourceAccessMap(const nx::Uuid& subjectId) const override;

    virtual nx::vms::api::GlobalPermissions globalPermissions(const nx::Uuid& subjectId) const override;

    nx::vms::api::AccessRights availableAccessRights(const nx::Uuid& subjectId) const;

    /**
     * Returns all ways in which the specified subject gains specified access right to
     * the specified resource, directly and indirectly.
     * In case of a sequential inheritance, returns the actual source of the access right,
     * not the last inherited group with the access right.
     */
    virtual ResourceAccessDetails accessDetails(
        const nx::Uuid& subjectId,
        const QnResourcePtr& resource,
        nx::vms::api::AccessRight accessRight) const override;

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::core::access
