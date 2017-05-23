#include "message_body_converter.h"

#include <memory>

#include <nx/network/http/http_content_type.h>

#include "m3u_playlist_converter.h"

namespace nx {
namespace cloud {
namespace gateway {

MessageBodyConverterFactory::MessageBodyConverterFactory():
    base_type(std::bind(&MessageBodyConverterFactory::defaultFactoryFunction, this,
        std::placeholders::_1, std::placeholders::_2))
{
}

MessageBodyConverterFactory& MessageBodyConverterFactory::instance()
{
    static MessageBodyConverterFactory instance;
    return instance;
}

std::unique_ptr<AbstractMessageBodyConverter> 
    MessageBodyConverterFactory::defaultFactoryFunction(
        const TargetHost& targetHost,
        const nx::String& contentType)
{
    if (contentType == nx_http::kApplicationMpegUrlMimeType ||
        contentType == nx_http::kAudioMpegUrlMimeType)
    {
        return std::make_unique<M3uPlaylistConverter>(
            targetHost.target.address.toString().toUtf8());
    }

    return nullptr;
}

} // namespace gateway
} // namespace cloud
} // namespace nx
