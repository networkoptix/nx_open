#include "camera_vca_parameter_api.h"

#include <cmath>

#include <nx/utils/general_macros.h>
#include <nx/utils/log/log_message.h>

#include "exception.h"
#include "utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::sdk::analytics;
using namespace nx::utils;

namespace {

// Coordinates are normalized to [0; 10000]x[0; 10000].
constexpr double kCoordinateDomain = 10000;

} // namespace

CameraVcaParameterApi::CameraVcaParameterApi(Url url):
    m_url(std::move(url))
{
}

cf::future<JsonValue> CameraVcaParameterApi::fetch(const QString& scope)
{
    auto url = m_url;
    url.setPath(NX_FMT("/VCA/%1", scope));
    return m_httpClient.get(url)
        .then_unwrap(NX_WRAP_FUNC_TO_LAMBDA(parseJson))
        .then(addExceptionContextAndRethrow(
            "Failed to fetch %1 VCA parameters from %2", scope, withoutUserInfo(m_url)));
}

cf::future<cf::unit> CameraVcaParameterApi::store(
    const QString& scope, const JsonValue& parameters)
{
    auto url = m_url;
    url.setPath(NX_FMT("/VCA/%1", scope));
    return m_httpClient.post(url, "application/json", serializeJson(parameters))
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

float CameraVcaParameterApi::parseCoordinate(const JsonValue& json)
{
    return std::clamp(json.to<double>() / kCoordinateDomain, 0.0, 1.0);
}

JsonValue CameraVcaParameterApi::serializeCoordinate(float value)
{
    return (int) std::round(value * kCoordinateDomain);
}

Point CameraVcaParameterApi::parsePoint(const JsonValue& json)
{
    return Point{parseCoordinate(json["x"]), parseCoordinate(json["y"])};
}

JsonObject CameraVcaParameterApi::serialize(const Point& point)
{
    return JsonObject{
        {"x", serializeCoordinate(point.x)},
        {"y", serializeCoordinate(point.y)},
    };
}

} // namespace nx::vms_server_plugins::analytics::vivotek
