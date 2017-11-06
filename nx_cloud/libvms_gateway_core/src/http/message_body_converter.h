#pragma once

#include <nx/network/http/http_types.h>
#include <nx/network/socket_common.h>
#include <nx/utils/basic_factory.h>

namespace nx {
namespace cloud {
namespace gateway {

class AbstractMessageBodyConverter
{
public:
    virtual ~AbstractMessageBodyConverter() = default;

    virtual nx_http::BufferType convert(nx_http::BufferType originalBody) = 0;
};

class AbstractUrlRewriter
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

class MessageBodyConverterFactory:
    public utils::BasicFactory<MessageBodyConverterFactoryFunction>
{
    using base_type = utils::BasicFactory<MessageBodyConverterFactoryFunction>;

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

} // namespace gateway
} // namespace cloud
} // namespace nx
