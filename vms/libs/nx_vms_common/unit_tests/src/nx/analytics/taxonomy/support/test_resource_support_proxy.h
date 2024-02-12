// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/analytics/taxonomy/abstract_resource_support_proxy.h>

namespace nx::analytics::taxonomy {

class TestResourceSupportProxy: public AbstractResourceSupportProxy
{
public:
    TestResourceSupportProxy();

    virtual bool isEntityTypeSupported(
        EntityType entityType,
        const QString& entityTypeId,
        nx::Uuid deviceId,
        nx::Uuid engineId) const override;

    virtual bool isEntityTypeAttributeSupported(
        EntityType entityType,
        const QString& entityTypeId,
        const QString& fullAttributeName,
        nx::Uuid deviceId,
        nx::Uuid engineId) const override;
};

} // namespace nx::analytics::taxonomy
