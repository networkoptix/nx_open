#pragma once

#include <memory>

#include <nx/network/socket_common.h>
#include <nx/utils/basic_factory.h>

#include "../../http_types.h"

namespace nx {
namespace network {
namespace http {
namespace server {
namespace proxy {

class NX_NETWORK_API AbstractMessageBodyConverter
{
public:
    virtual ~AbstractMessageBodyConverter() = default;

    virtual nx::network::http::BufferType convert(nx::network::http::BufferType originalBody) = 0;
};

class NX_NETWORK_API AbstractUrlRewriter
{
public:
    virtual ~AbstractUrlRewriter() = default;

    virtual nx::utils::Url originalResourceUrlToProxyUrl(
        const nx::utils::Url& originalResourceUrl,
        const SocketAddress& proxyEndpoint,
        const nx::String& targetHost) const = 0;
};

//-------------------------------------------------------------------------------------------------

using MessageBodyConverterFactoryFunction =
    std::unique_ptr<AbstractMessageBodyConverter>(
        const nx::String& proxyHost,
        const nx::String& targetHost,
        const nx::String& contentType);

class NX_NETWORK_API MessageBodyConverterFactory:
    public nx::utils::BasicFactory<MessageBodyConverterFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<MessageBodyConverterFactoryFunction>;

public:
    MessageBodyConverterFactory();

    /**
     * NOTE: Default implementation does not do anything.
     */
    void setUrlConverter(std::unique_ptr<AbstractUrlRewriter> urlConverter);

    static MessageBodyConverterFactory& instance();

private:
    std::unique_ptr<AbstractUrlRewriter> m_urlConverter;

    std::unique_ptr<AbstractMessageBodyConverter> defaultFactoryFunction(
        const nx::String& proxyHost,
        const nx::String& targetHost,
        const nx::String& contentType);
};

} // namespace proxy
} // namespace server
} // namespace nx
} // namespace network
} // namespace http
