// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "source_user_field.h"

#include <QtCore/QCoreApplication>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/common/user_management/user_management_helpers.h>

namespace nx::vms::rules {

SourceUserField::SourceUserField(
    common::SystemContext* context,
    const FieldDescriptor* descriptor)
    :
    ResourceFilterEventField{descriptor},
    SystemContextAware{context}
{
}

bool SourceUserField::match(const QVariant& value) const
{
    if (acceptAll())
        return true;

    const auto userId = value.value<nx::Uuid>();
    if (ids().contains(userId))
        return true;

    const auto user = resourcePool()->getResourceById<QnUserResource>(userId);
    if (!user)
        return false;

    return nx::vms::common::userGroupsWithParents(user).intersects(ids());
}

QJsonObject SourceUserField::openApiDescriptor(const QVariantMap& properties)
{
    auto descriptor = ResourceFilterEventField::openApiDescriptor(properties);
    descriptor[utils::kDescriptionProperty] =
        "Defines the source users to be used for event filtering.";
    return descriptor;
}

} // namespace nx::vms::rules
