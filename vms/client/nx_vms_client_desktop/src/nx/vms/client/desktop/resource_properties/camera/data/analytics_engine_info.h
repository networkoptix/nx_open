// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QVariantMap>

#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

struct AnalyticsEngineInfo
{
    QnUuid id;
    QString name;
    QString description;
    QString version;
    QString vendor;
    QJsonObject settingsModel;
    bool isDeviceDependent = false;

    bool operator==(const AnalyticsEngineInfo& other) const = default;

    // TODO: Use fusion to serialize to QJsonObject instead.
    QVariantMap toVariantMap() const
    {
        return {
            {"id", QVariant::fromValue(id)},
            {"name", name},
            {"description", description},
            {"version", version},
            {"vendor", vendor},
            {"settingsModel", settingsModel},
            {"isDeviceDependent", isDeviceDependent}
        };
    }
};

// TODO: #sivanov Add settingsModel when QJsonObject is supported in the nx_reflect.
NX_REFLECTION_INSTRUMENT(AnalyticsEngineInfo,
    (id)(name)(description)(version)(vendor)(isDeviceDependent))

} // namespace nx::vms::client::desktop
