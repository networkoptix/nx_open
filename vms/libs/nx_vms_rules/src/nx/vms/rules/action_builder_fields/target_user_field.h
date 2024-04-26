// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/common/system_context_aware.h>

#include "../base_fields/resource_filter_field.h"

namespace nx::vms::rules {

constexpr auto kBookmarkManagementValidationPolicy = "bookmarkManagement";
constexpr auto kUserWithEmailValidationPolicy = "userWithEmail";
constexpr auto kCloudUserValidationPolicy = "cloudUser";
constexpr auto kLayoutAccessValidationPolicy = "layoutAccess";

class NX_VMS_RULES_API TargetUserField:
    public ResourceFilterActionField,
    public common::SystemContextAware
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.users")

public:
    TargetUserField(common::SystemContext* context, const FieldDescriptor* descriptor);

    QnUserResourceSet users() const;
};

} // namespace nx::vms::rules
