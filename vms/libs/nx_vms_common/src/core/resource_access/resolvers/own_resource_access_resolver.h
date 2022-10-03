// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>

#include "abstract_resource_access_resolver.h"

namespace nx::core::access {

class AbstractAccessRightsManager;
class AbstractGlobalPermissionsWatcher;

/**
 * A simple resolver that translates access rights changes from `AccessRightsManager`.
 */
class NX_VMS_COMMON_API OwnResourceAccessResolver: public AbstractResourceAccessResolver
{
    Q_OBJECT
    using base_type = AbstractResourceAccessResolver;

public:
    explicit OwnResourceAccessResolver(
        AbstractAccessRightsManager* accessRightsManager,
        AbstractGlobalPermissionsWatcher* globalPermissionWatcher,
        QObject* parent = nullptr);

    virtual ~OwnResourceAccessResolver() override;

    virtual ResourceAccessMap resourceAccessMap(const QnUuid& subjectId) const override;

    virtual AccessRights accessRights(const QnUuid& subjectId,
        const QnResourcePtr& resource) const override;

    virtual nx::vms::api::GlobalPermissions globalPermissions(const QnUuid& subjectId) const override;

    virtual ResourceAccessDetails accessDetails(
        const QnUuid& subjectId,
        const QnResourcePtr& resource,
        nx::vms::api::AccessRight accessRight) const override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::core::access
