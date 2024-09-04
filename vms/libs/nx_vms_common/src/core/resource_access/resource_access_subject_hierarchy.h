// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>

#include "resource_access_subject.h"
#include "subject_hierarchy.h"

class QnResourcePool;

namespace nx::vms::common { class UserGroupManager; }

namespace nx::core::access {

/**
 * User and group hierarchy tracker.
 */
class NX_VMS_COMMON_API ResourceAccessSubjectHierarchy: public SubjectHierarchy
{
    using base_type = SubjectHierarchy;

public:
    explicit ResourceAccessSubjectHierarchy(
        QnResourcePool* resourcePool,
        nx::vms::common::UserGroupManager* userGroupManager,
        QObject* parent = nullptr);

    virtual ~ResourceAccessSubjectHierarchy() override;

    /**
     * Creates a resource access subject by trying to look up a user in the system resource pool
     * or a user group in the system user groups manager.
     */
    QnResourceAccessSubject subjectById(const nx::Uuid& id) const;

    /**
     * List of users belonging to a given set of groups or any of their child groups, recursively.
     */
    QnUserResourceList usersInGroups(const QSet<nx::Uuid>& groupIds, bool withHidden) const;

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::core::access
