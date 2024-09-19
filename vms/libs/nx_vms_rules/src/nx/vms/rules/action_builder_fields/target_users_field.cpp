// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "target_users_field.h"

#include "../field_types.h"
#include "../utils/resource.h"

namespace nx::vms::rules {

TargetUsersField::TargetUsersField(
    common::SystemContext* context,
    const FieldDescriptor* descriptor)
    :
    ResourceFilterActionField{descriptor},
    common::SystemContextAware{context}
{
}

QnUserResourceSet TargetUsersField::users() const
{
    return utils::users(UuidSelection{ids(), acceptAll()}, systemContext(), /*activeOnly*/ true);
}

QJsonObject TargetUsersField::openApiDescriptor(const QVariantMap&)
{
    auto descriptor = utils::constructOpenApiDescriptor<TargetUsersField>();
    descriptor[utils::kDescriptionProperty] = "Specifies the target users for the action.";
    return descriptor;
}

} // namespace nx::vms::rules
