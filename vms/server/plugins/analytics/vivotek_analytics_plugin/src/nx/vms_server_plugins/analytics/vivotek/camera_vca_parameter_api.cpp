#include "camera_vca_parameter_api.h"

#include <stdexcept>

#include <nx/utils/general_macros.h>

#include "json_utils.h"
#include "exception_utils.h"
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
        .then_ok(NX_WRAP_FUNC_TO_LAMBDA(parseJson))
        .then(addExceptionContext(
            "Failed to fetch VCA parameters from camera at %1", withoutUserInfo(m_url)));
}

cf::future<cf::unit> CameraVcaParameterApi::store(const QJsonValue& parameters)
{
    auto url = m_url;
    url.setPath("/VCA/Config/AE");
    return m_httpClient.post(url, "application/json", unparseJson(parameters))
        .then_ok(
            [this](auto&&)
            {
                auto url = m_url;
                url.setPath("/VCA/Config/Reload");
                return m_httpClient.get(url);
            })
        .then_ok([](auto&&) { return cf::unit(); })
        .then(addExceptionContext(
            "Failed to store VCA parameters to camera at %1", withoutUserInfo(m_url)));
}

} // namespace nx::vms_server_plugins::analytics::vivotek
