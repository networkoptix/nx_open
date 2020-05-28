#pragma once

#include <optional>

#include <nx/utils/thread/cf/cfuture.h>
#include <nx/utils/url.h>

#include <QtCore/QFlags>
#include <QtCore/QJsonValue>
#include <QtCore/QSet>
#include <QtCore/QString>

#include "camera_vca_parameter_api.h"
#include "websocket.h"

namespace nx::vms_server_plugins::analytics::vivotek {

enum NativeMetadataType
{
    NoNativeMetadataTypes = 0b00,
    ObjectNativeMetadataType = 0b01,
    EventNativeMetadataType = 0b10,
};
Q_DECLARE_FLAGS(NativeMetadataTypes, NativeMetadataType);
Q_DECLARE_OPERATORS_FOR_FLAGS(NativeMetadataTypes);

class NativeMetadataSource
{
public:
    cf::future<cf::unit> open(const nx::utils::Url& url, NativeMetadataTypes types);
    cf::future<QJsonValue> read();
    void close();

private:
    cf::future<cf::unit> enableDetailMetadata(nx::utils::Url url);

private:
    std::optional<CameraVcaParameterApi> m_vcaParameterApi;
    WebSocket m_webSocket;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
