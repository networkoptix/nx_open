// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_resource_support_proxy.h"

namespace nx::analytics::taxonomy {

TestResourceSupportProxy::TestResourceSupportProxy()
{
}

bool TestResourceSupportProxy::isEntityTypeSupported(
    EntityType entityType,
    const QString& entityTypeId,
    nx::Uuid deviceId,
    nx::Uuid engineId) const
{
    return false;
}

bool TestResourceSupportProxy::isEntityTypeAttributeSupported(
    EntityType entityType,
    const QString& entityTypeId,
    const QString& fullAttributeName,
    nx::Uuid deviceId,
    nx::Uuid engineId) const
{
    return false;
}

} // namespace nx::analytics::taxonomy
