#pragma once

#include <set>

#include <QtCore/QString>
#include <QtCore/QMap>
#include <QtCore/QJsonObject>

#include <nx/utils/uuid.h>
#include <nx/vms/server/sdk_support/error.h>

namespace nx::vms::server::sdk_support {

using SettingsErrors = QMap<QString, QString>;
using SettingsValues = QJsonObject;
using SettingsModel = QJsonObject;
using SettingsModelId = QnUuid;

struct SdkSettingsResponse
{
    std::optional<SettingsValues> values = std::nullopt;
    SettingsErrors errors;
    std::optional<SettingsModel> model = std::nullopt;
    Error sdkError;
};

struct MetadataTypes
{
    std::set<QString> eventTypeIds;
    std::set<QString> objectTypeIds;
};

inline bool operator==(const MetadataTypes& lhs, const MetadataTypes& rhs)
{
    return lhs.eventTypeIds == rhs.eventTypeIds && lhs.objectTypeIds == rhs.objectTypeIds;
}

} // namespace nx::vms::server::sdk_support
