#include "native_metadata_source.h"

#include <nx/utils/general_macros.h>

#include <QtCore/QStringList>

#include "exception.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::utils;
using namespace nx::network;

cf::future<cf::unit> NativeMetadataSource::open(const Url& url, NativeMetadataTypes types)
{
    return enableDetailMetadata(url)
        .then_unwrap(
            [this, url = url, types](auto&&) mutable
            {
                url.setPath("/ws/vca");

                QStringList typeNames;
                if (types & ObjectNativeMetadataType)
                    typeNames.append("meta");
                if (types & EventNativeMetadataType)
                    typeNames.append("event");
                url.setQuery("data=" + typeNames.join(","));

                return m_webSocket.open(std::move(url));
            })
        .then(addExceptionContextAndRethrow("Failed to open native metadata source"));
}

cf::future<JsonValue> NativeMetadataSource::read()
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
    m_vcaParameterApi.emplace(std::move(url));
    return m_vcaParameterApi->fetch("Config/AE")
        .then_unwrap(
            [this](const JsonValue& parametersValue)
            {
                auto parameters = parametersValue.to<JsonObject>();
                auto webSocket = parameters["WebSocket"].to<JsonObject>();

                if (webSocket["DetailMetadata"].to<bool>())
                    return cf::make_ready_future(cf::unit());

                webSocket["DetailMetadata"] = true;
                parameters["WebSocket"] = webSocket;

                return m_vcaParameterApi->store("Config/AE", parameters)
                    .then_unwrap(
                        [this](auto&&) {
                            return m_vcaParameterApi->reloadConfig();
                        });
            })
        .then(addExceptionContextAndRethrow("Failed to enable detail metadata"));
}

} // namespace nx::vms_server_plugins::analytics::vivotek
