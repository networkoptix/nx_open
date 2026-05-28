// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <QtCore/QObject>

#include <nx/analytics/taxonomy/entity_type.h>
#include <nx/utils/uuid.h>

namespace nx::analytics::taxonomy {

class NX_VMS_COMMON_API AbstractResourceSupportProxy: public QObject
{
    Q_OBJECT
public:
    virtual ~AbstractResourceSupportProxy() = default;

    virtual bool isEntityTypeSupported(
        EntityType entityType,
        const std::string& entityTypeId,
        nx::Uuid deviceId,
        nx::Uuid engineId) const = 0;

    virtual bool isEntityTypeAttributeSupported(
        EntityType entityType,
        const std::string& entityTypeId,
        const std::string& fullAttributeName,
        nx::Uuid deviceId,
        nx::Uuid engineId) const = 0;

signals:
    void manifestsMaybeUpdated();
};

} // namespace nx::analytics::taxonomy
