#pragma once

#include <set>

#include <QtCore/QString>
#include <QtCore/QMap>
#include <QtCore/QVariantMap>

namespace nx::vms::server::sdk_support {

using ErrorMap = QMap<QString, QString>;
using SettingMap = QVariantMap;

struct SettingsResponse
{
    SettingMap settingValues;
    ErrorMap errors;
};

struct MetadataTypes
{
    std::set<QString> eventTypeIds;
    std::set<QString> objectTypeIds;
};

} // namespace nx::vms::server::sdk_support
