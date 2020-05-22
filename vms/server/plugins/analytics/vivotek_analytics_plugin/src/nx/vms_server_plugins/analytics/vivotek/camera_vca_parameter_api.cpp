#include "camera_vca_parameter_api.h"

#include <nx/utils/general_macros.h>
#include <nx/utils/log/log_message.h>

#include "json_utils.h"
#include "exception.h"
#include "utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::sdk::analytics;
using namespace nx::utils;

CameraVcaParameterApi::CameraVcaParameterApi(Url url):
    m_url(std::move(url))
{
}

cf::future<QJsonValue> CameraVcaParameterApi::fetch(const QString& scope)
{
    auto url = m_url;
    url.setPath(NX_FMT("/VCA/%1", scope));
    return m_httpClient.get(url)
        .then_unwrap(NX_WRAP_FUNC_TO_LAMBDA(parseJson))
        .then(addExceptionContextAndRethrow(
            "Failed to fetch %1 VCA parameters from %2", scope, withoutUserInfo(m_url)));
}

cf::future<cf::unit> CameraVcaParameterApi::store(
    const QString& scope, const QJsonValue& parameters)
{
    auto url = m_url;
    url.setPath(NX_FMT("/VCA/%1", scope));
    return m_httpClient.post(url, "application/json", unparseJson(parameters))
        .then(cf::discard_value)
        .then(addExceptionContextAndRethrow(
            "Failed to store %1 VCA parameters to %2", scope, withoutUserInfo(m_url)));
}

cf::future<cf::unit> CameraVcaParameterApi::reloadConfig()
{
    auto url = m_url;
    url.setPath("/VCA/Config/Reload");
    return m_httpClient.get(url)
        .then(cf::discard_value)
        .then(addExceptionContextAndRethrow(
            "Failed to reload VCA config on %1", withoutUserInfo(m_url)));
}

Point CameraVcaParameterApi::parsePoint(const QJsonValue& json, const QString& path)
{
    auto parseCoord =
        [&](const QString& key)
        {
            // coordinates are normalized to [0; 10000]x[0; 10000]
            constexpr double kDomain = 10000;

            const auto value = get<double>(path, json, key);
            if (value < 0 || value > kDomain)
            {
                throw Exception("%1.%2 = %3 is outside of expected range of [0; 10000]",
                    path, key, value);
            }

            return value / kDomain;
        };

    return Point(parseCoord("x"), parseCoord("y"));
}

} // namespace nx::vms_server_plugins::analytics::vivotek
