#pragma once

#include <set>
#include <map>

namespace nx::vms::server::analytics::wrappers {

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

} // namespace nx::vms::server::analytics::wrappers
