#include "message_body_converter.h"

#include <nx/network/http/http_content_type.h>
#include <nx/utils/std/cpp14.h>

#include "m3u_playlist_converter.h"

namespace nx_http {
namespace server {
namespace proxy {

namespace {

class DefaultUrlRewriter:
    public AbstractUrlRewriter
{
public:
    virtual nx::utils::Url originalResourceUrlToProxyUrl(
        const nx::utils::Url& originalResourceUrl,
        const SocketAddress& /*proxyEndpoint*/,
        const nx::String& /*targetHost*/) const override
    {
        return originalResourceUrl;
    }
};

} // namespace

using namespace std::placeholders;

MessageBodyConverterFactory::MessageBodyConverterFactory():
    base_type(std::bind(&MessageBodyConverterFactory::defaultFactoryFunction, this,
        _1, _2, _3)),
    m_urlConverter(std::make_unique<DefaultUrlRewriter>())
{
}

void MessageBodyConverterFactory::setUrlConverter(
    std::unique_ptr<AbstractUrlRewriter> urlConverter)
{
    m_urlConverter = std::move(urlConverter);
}

MessageBodyConverterFactory& MessageBodyConverterFactory::instance()
{
    static MessageBodyConverterFactory instance;
    return instance;
}

std::unique_ptr<AbstractMessageBodyConverter>
    MessageBodyConverterFactory::defaultFactoryFunction(
        const nx::String& proxyHost,
        const nx::String& targetHost,
        const nx::String& contentType)
{
    if (contentType == nx_http::kApplicationMpegUrlMimeType ||
        contentType == nx_http::kAudioMpegUrlMimeType)
    {
        return std::make_unique<M3uPlaylistConverter>(
            *m_urlConverter, proxyHost, targetHost);
    }

    return nullptr;
}

} // namespace proxy
} // namespace server
} // namespace nx_http
