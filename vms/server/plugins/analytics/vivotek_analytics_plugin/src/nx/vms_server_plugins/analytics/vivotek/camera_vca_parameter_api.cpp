#include "camera_vca_parameter_api.h"

#include <nx/utils/general_macros.h>

#include "json_utils.h"
#include "exception.h"
#include "utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::utils;

CameraVcaParameterApi::CameraVcaParameterApi(Url url):
    m_url(std::move(url))
{
}

cf::future<QJsonValue> CameraVcaParameterApi::fetch()
{
    auto url = m_url;
    url.setPath("/VCA/Config/AE");
    return m_httpClient.get(std::move(url))
        .then_unwrap(NX_WRAP_FUNC_TO_LAMBDA(parseJson))
        .then(addExceptionContextAndRethrow(
            "Failed to fetch VCA parameters from camera at %1", withoutUserInfo(m_url)));
}

cf::future<cf::unit> CameraVcaParameterApi::store(const QJsonValue& parameters)
{
    auto url = m_url;
    url.setPath("/VCA/Config/AE");
    return m_httpClient.post(url, "application/json", unparseJson(parameters))
        .then_unwrap(
            [this](auto&&)
            {
                auto url = m_url;
                url.setPath("/VCA/Config/Reload");
                return m_httpClient.get(url);
            })
        .then_unwrap([](auto&&) { return cf::unit(); })
        .then(addExceptionContextAndRethrow(
            "Failed to store VCA parameters to camera at %1", withoutUserInfo(m_url)));
}

} // namespace nx::vms_server_plugins::analytics::vivotek
