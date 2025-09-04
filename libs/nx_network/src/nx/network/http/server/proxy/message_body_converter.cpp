// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "message_body_converter.h"

#include <nx/network/http/http_content_type.h>

#include "m3u_playlist_converter.h"

namespace nx::network::http::server::proxy {

namespace {

class DefaultUrlRewriter:
    public AbstractUrlRewriter
{
public:
    virtual nx::Url originalResourceUrlToProxyUrl(
        const nx::Url& originalResourceUrl,
        const nx::Url& /*proxyHostUrl*/,
        const std::string& /*targetHost*/) const override
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
        const nx::Url& proxyHostUrl,
        const std::string& targetHost,
        const std::string& contentType)
{
    if (contentType == nx::network::http::kApplicationMpegUrlMimeType ||
        contentType == nx::network::http::kAudioMpegUrlMimeType)
    {
        return std::make_unique<M3uPlaylistConverter>(
            *m_urlConverter, proxyHostUrl, targetHost);
    }

    return nullptr;
}

} // namespace nx::network::http::server::proxy
