#pragma once

#include <QtCore/QJsonObject>

#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

struct AnalyticsEngineInfo
{
    QnUuid id;
    QString name;
    QJsonObject settingsModel;
};

} // namespace nx::vms::client::desktop
