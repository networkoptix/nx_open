#include "native_metadata_source.h"

#include <nx/utils/general_macros.h>

#include "json_utils.h"
#include "exception.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::utils;
using namespace nx::network;

cf::future<cf::unit> NativeMetadataSource::open(const Url& url)
{
    return enableDetailMetadata(url)
        .then_unwrap(
            [this, url = url](auto&&) mutable
            {
                url.setPath("/ws/vca");
                url.setQuery("data=meta");
                return m_webSocket.open(std::move(url));
            })
        .then(addExceptionContextAndRethrow("Failed to open native metadata source"));
}

cf::future<QJsonValue> NativeMetadataSource::read()
{
    return m_webSocket.read()
        .then_unwrap(NX_WRAP_FUNC_TO_LAMBDA(parseJson))
        .then(addExceptionContextAndRethrow("Failed to read native metadata"));
}

void NativeMetadataSource::close()
{
    m_vcaParameterApi.reset();
    m_webSocket.close();
}

cf::future<cf::unit> NativeMetadataSource::enableDetailMetadata(Url url)
{
    m_vcaParameterApi.emplace(std::move(url), "Config/AE");
    return m_vcaParameterApi->fetch()
        .then_unwrap(
            [this](auto parameters)
            {
                if (get<bool>(parameters, "WebSocket", "DetailMetadata"))
                    return cf::make_ready_future(cf::unit());

                set(&parameters, "WebSocket", "DetailMetadata", true);

                return m_vcaParameterApi->store(parameters);
            })
        .then(addExceptionContextAndRethrow("Failed to enable detail metadata"));
}

} // namespace nx::vms_server_plugins::analytics::vivotek
