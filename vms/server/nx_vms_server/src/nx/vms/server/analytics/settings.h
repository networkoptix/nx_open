#pragma once

#include <nx/utils/uuid.h>
#include <nx/vms/api/analytics/settings.h>
#include <nx/vms/api/analytics/device_agent_settings_response.h>

namespace nx::vms::server::analytics {

struct SetSettingsRequest
{
    QnUuid modelId;
    api::analytics::SettingsValues values;
};

struct SettingsResponse
{
    struct Error
    {
        enum class Code
        {
            ok,
            sdkError,
            sdkObjectIsNotAccessible,
            wrongSettingsModel,
            unableToObtainSettingsResponse,
        };

        Error() = default;
        Error(Code code, QString message = QString()):
            code(code),
            message(std::move(message))
        {
        }

        bool isOk() const { return code == Code::ok; }

        Code code = Code::ok;
        QString message;
    };

    SettingsResponse() = default;
    SettingsResponse(Error error): error(std::move(error)) {}
    SettingsResponse(Error::Code errorCode, QString errorMessage):
        error(Error(errorCode, std::move(errorMessage)))
    {
    }

    Error error;

    QnUuid modelId;
    api::analytics::SettingsModel model;
    api::analytics::SettingsValues values;
    api::analytics::SettingsErrors errors;
    api::analytics::DeviceAgentSettingsSession session;
};

struct SettingsContext
{
    QnUuid modelId;
    api::analytics::SettingsModel model;
    api::analytics::SettingsValues values;
    bool saveSettingsValuesToProperty = true;
};

QnUuid calculateModelId(const api::analytics::SettingsModel& settingsModel);

QJsonObject toJsonObject(const api::analytics::SettingsErrors& settingsErrors);

} // namespace nx::vms::server::analytics
