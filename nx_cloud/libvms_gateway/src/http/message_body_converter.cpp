#include "message_body_converter.h"

#include <memory>

#include <nx/network/http/http_content_type.h>
#include <nx/utils/std/cpp14.h>

#include "m3u_playlist_converter.h"

namespace nx {
namespace cloud {
namespace gateway {

using namespace std::placeholders;

MessageBodyConverterFactory::MessageBodyConverterFactory():
    base_type(std::bind(&MessageBodyConverterFactory::defaultFactoryFunction, this,
        _1, _2, _3))
{
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
        return std::make_unique<M3uPlaylistConverter>(proxyHost, targetHost);
    }

    return nullptr;
}

} // namespace gateway
} // namespace cloud
} // namespace nx
