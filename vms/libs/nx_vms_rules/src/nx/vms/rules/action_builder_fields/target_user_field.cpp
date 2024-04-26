// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "target_user_field.h"

#include "../field_types.h"
#include "../utils/resource.h"

namespace nx::vms::rules {

TargetUserField::TargetUserField(
    common::SystemContext* context,
    const FieldDescriptor* descriptor)
    :
    ResourceFilterActionField{descriptor},
    common::SystemContextAware{context}
{
}

QnUserResourceSet TargetUserField::users() const
{
    return utils::users(UuidSelection{ids(), acceptAll()}, systemContext(), /*activeOnly*/ true);
}

} // namespace nx::vms::rules
