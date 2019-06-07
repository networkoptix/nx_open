#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QVariantMap>

#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

struct AnalyticsEngineInfo
{
    QnUuid id;
    QString name;
    QJsonObject settingsModel;
    bool isDeviceDependent = false;

    // TODO: Looks like we need a fusion method or any other simple way to proxy struct to QML.
    QVariantMap toVariantMap() const
    {
        return {
            {"id", QVariant::fromValue(id)},
            {"name", name},
            {"settingsModel", settingsModel},
            {"isDeviceDependent", isDeviceDependent}
        };
    }
};

} // namespace nx::vms::client::desktop
