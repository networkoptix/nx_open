#include "camera_vca_parameter_api.h"

#include <nx/utils/general_macros.h>
#include <nx/utils/log/log_message.h>

#include "json_utils.h"
#include "exception.h"
#include "utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::utils;

CameraVcaParameterApi::CameraVcaParameterApi(Url url, const QString& scope):
    m_url(std::move(url))
{
    m_url.setPath(NX_FMT("/VCA/%1", scope));
}

cf::future<QJsonValue> CameraVcaParameterApi::fetch()
{
    return m_httpClient.get(m_url)
        .then_unwrap(NX_WRAP_FUNC_TO_LAMBDA(parseJson))
        .then(addExceptionContextAndRethrow(
            "Failed to fetch VCA parameters from %1", withoutUserInfo(m_url)));
}

cf::future<cf::unit> CameraVcaParameterApi::store(const QJsonValue& parameters)
{
    return m_httpClient.post(m_url, "application/json", unparseJson(parameters))
        .then_unwrap(
            [this](auto&&)
            {
                NX_ASSERT(m_url.path().startsWith("/VCA/Config/"));

                auto url = m_url;
                url.setPath("/VCA/Config/Reload");
                return m_httpClient.get(url);
            })
        .then_unwrap([](auto&&) { return cf::unit(); })
        .then(addExceptionContextAndRethrow(
            "Failed to store VCA parameters to %1", withoutUserInfo(m_url)));
}

} // namespace nx::vms_server_plugins::analytics::vivotek
