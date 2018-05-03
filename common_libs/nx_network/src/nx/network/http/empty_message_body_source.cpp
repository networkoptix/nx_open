#include "empty_message_body_source.h"

namespace nx {
namespace network {
namespace http {

EmptyMessageBodySource::EmptyMessageBodySource(
    StringType contentType,
    const boost::optional<uint64_t>& contentLength)
    :
    m_contentType(std::move(contentType)),
    m_contentLength(contentLength)
{
}

EmptyMessageBodySource::~EmptyMessageBodySource()
{
    stopWhileInAioThread();
}

void EmptyMessageBodySource::stopWhileInAioThread()
{
}

StringType EmptyMessageBodySource::mimeType() const
{
    return m_contentType;
}

boost::optional<uint64_t> EmptyMessageBodySource::contentLength() const
{
    return m_contentLength;
}

void EmptyMessageBodySource::readAsync(
    nx::utils::MoveOnlyFunc<
        void(SystemError::ErrorCode, BufferType)> completionHandler)
{
    post(
        [completionHandler = std::move(completionHandler)]()
        {
            completionHandler(SystemError::noError, BufferType());
        });
}

} // namespace nx
} // namespace network
} // namespace http
