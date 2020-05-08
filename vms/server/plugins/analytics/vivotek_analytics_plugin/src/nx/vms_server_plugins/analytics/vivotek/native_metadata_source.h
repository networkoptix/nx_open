#pragma once

#include <optional>

#include <nx/utils/thread/cf/cfuture.h>
#include <nx/utils/url.h>

#include <QtCore/QJsonValue>

#include "camera_vca_parameter_api.h"
#include "websocket.h"

namespace nx::vms_server_plugins::analytics::vivotek {

class NativeMetadataSource
{
public:
    cf::future<cf::unit> open(const nx::utils::Url& url);
    cf::future<QJsonValue> read();
    void close();

private:
    cf::future<cf::unit> enableDetailMetadata(nx::utils::Url url);

private:
    std::optional<CameraVcaParameterApi> m_vcaParameterApi;
    WebSocket m_webSocket;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
