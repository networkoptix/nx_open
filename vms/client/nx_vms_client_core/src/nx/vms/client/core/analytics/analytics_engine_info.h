// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QVariantMap>

#include <nx/reflect/instrument.h>
#include <nx/reflect/to_string.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/analytics_integration_model.h>

namespace nx::vms::client::core {

struct AnalyticsEngineInfo
{
    QnUuid id;
    QString name;
    QString description;
    QString version;
    QString vendor;
    bool isLicenseRequired = false;
    QJsonObject settingsModel;
    bool isDeviceDependent = false;
    nx::vms::api::analytics::IntegrationType type;

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
            {"isLicenseRequired", isLicenseRequired},
            {"settingsModel", settingsModel},
            {"isDeviceDependent", isDeviceDependent},
            {"type", QString::fromStdString(nx::reflect::toString(type))}
        };
    }
};

// TODO: #sivanov Add settingsModel when QJsonObject is supported in the nx_reflect.
NX_REFLECTION_INSTRUMENT(AnalyticsEngineInfo,
    (id)(name)(description)(version)(vendor)(isLicenseRequired)(isDeviceDependent))

} // namespace nx::vms::client::core
