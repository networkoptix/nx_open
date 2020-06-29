#pragma once

#include <QtCore/QMap>
#include <QtCore/QJsonObject>

namespace nx::vms::api::analytics {

using SettingsModel = QJsonObject;
using SettingsValues = QJsonObject;
using SettingsErrors = QMap<QString, QString>;

} // namespace nx::vms::api::analytics
