#include "buffer_source.h"

namespace nx {
namespace network {
namespace http {

BufferSource::BufferSource(StringType mimeType, BufferType msgBody):
    m_mimeType(std::move(mimeType)),
    m_msgBody(std::move(msgBody))
{
}

StringType BufferSource::mimeType() const
{
    return m_mimeType;
}

boost::optional<uint64_t> BufferSource::contentLength() const
{
    return boost::optional<uint64_t>(m_msgBody.size());
}

void BufferSource::readAsync(
    nx::utils::MoveOnlyFunc<
        void(SystemError::ErrorCode, BufferType)
    > completionHandler)
{
    BufferType outMsgBody;
    outMsgBody.swap(m_msgBody);
    completionHandler(SystemError::noError, std::move(outMsgBody));
}

} // namespace nx
} // namespace network
} // namespace http
