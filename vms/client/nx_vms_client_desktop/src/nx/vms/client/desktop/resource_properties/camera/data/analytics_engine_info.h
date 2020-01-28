#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QVariantMap>

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

} // namespace nx::vms::client::desktop
