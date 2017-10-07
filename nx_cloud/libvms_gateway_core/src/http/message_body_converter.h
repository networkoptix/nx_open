#pragma once

#include <nx/network/http/http_types.h>
#include <nx/network/socket_common.h>
#include <nx/utils/basic_factory.h>

#include "../settings.h"

namespace nx {
namespace cloud {
namespace gateway {

class AbstractMessageBodyConverter
{
public:
    virtual ~AbstractMessageBodyConverter() = default;

    virtual nx_http::BufferType convert(nx_http::BufferType originalBody) = 0;
};

//-------------------------------------------------------------------------------------------------

struct TargetHost
{
    nx_http::StatusCode::Value status = nx_http::StatusCode::notImplemented;
    SocketAddress target;
    conf::SslMode sslMode = conf::SslMode::followIncomingConnection;

    TargetHost() {}

    TargetHost(nx_http::StatusCode::Value status, SocketAddress target = {}):
        status(status),
        target(std::move(target))
    {
    }
};

//-------------------------------------------------------------------------------------------------

using MessageBodyConverterFactoryFunction = 
    std::unique_ptr<AbstractMessageBodyConverter>(
        const nx::String& proxyHost,
        const nx::String& targetHost,
        const nx::String& contentType);

class MessageBodyConverterFactory:
    public utils::BasicFactory<MessageBodyConverterFactoryFunction>
{
    using base_type = utils::BasicFactory<MessageBodyConverterFactoryFunction>;

public:
    MessageBodyConverterFactory();

    static MessageBodyConverterFactory& instance();

private:
    std::unique_ptr<AbstractMessageBodyConverter> defaultFactoryFunction(
        const nx::String& proxyHost,
        const nx::String& targetHost,
        const nx::String& contentType);
};

} // namespace gateway
} // namespace cloud
} // namespace nx
