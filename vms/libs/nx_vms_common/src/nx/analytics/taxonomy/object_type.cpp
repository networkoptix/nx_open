// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_type.h"

using namespace nx::vms::api::analytics;

namespace nx::analytics::taxonomy {

ObjectType::ObjectType(
    ObjectTypeDescriptor objectTypeDescriptor,
    AbstractResourceSupportProxy* resourceSupportProxy)
    :
    base_type(
        EntityType::objectType,
        objectTypeDescriptor,
        kObjectTypeDescriptorTypeName,
        resourceSupportProxy)
{
}

bool ObjectType::isNonIndexable() const
{
    return m_descriptor.flags.testFlag(ObjectTypeFlag::nonIndexable)
        || m_descriptor.flags.testFlag(ObjectTypeFlag::liveOnly);
}

bool ObjectType::isLiveOnly() const
{
    return m_descriptor.flags.testFlag(ObjectTypeFlag::liveOnly);
}

} // namespace nx::analytics::taxonomy
