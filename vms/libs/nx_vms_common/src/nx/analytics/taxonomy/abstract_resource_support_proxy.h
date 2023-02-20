// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>
#include <nx/utils/uuid.h>
#include <nx/analytics/taxonomy/entity_type.h>

namespace nx::analytics::taxonomy {

class NX_VMS_COMMON_API AbstractResourceSupportProxy
{
public:
    virtual ~AbstractResourceSupportProxy() = default;

    virtual bool isEntityTypeSupported(
        EntityType entityType,
        const QString& entityTypeId,
        QnUuid deviceId,
        QnUuid engineId) const = 0;

    virtual bool isEntityTypeAttributeSupported(
        EntityType entityType,
        const QString& entityTypeId,
        const QString& fullAttributeName,
        QnUuid deviceId,
        QnUuid engineId) const = 0;
};

} // namespace nx::analytics::taxonomy
