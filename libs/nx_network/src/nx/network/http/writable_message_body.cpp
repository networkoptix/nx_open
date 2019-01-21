#include "writable_message_body.h"

#include <nx/utils/byte_stream/custom_output_stream.h>

namespace nx::network::http {

WritableMessageBody::WritableMessageBody(
    const std::string& mimeType,
    boost::optional<uint64_t> contentLength)
    :
    m_mimeType(mimeType),
    m_contentLength(contentLength)
{
}

void WritableMessageBody::stopWhileInAioThread()
{
    if (m_onBeforeDestructionHandler)
        nx::utils::swapAndCall(m_onBeforeDestructionHandler);
}

StringType WritableMessageBody::mimeType() const
{
    return m_mimeType.c_str();
}

boost::optional<uint64_t> WritableMessageBody::contentLength() const
{
    return m_contentLength;
}

void WritableMessageBody::readAsync(
    nx::utils::MoveOnlyFunc<
    void(SystemError::ErrorCode, BufferType)
    > completionHandler)
{
    post(
        [this, completionHandler = std::move(completionHandler)]() mutable
    {
        NX_ASSERT(!m_readCompletionHandler);
        if (m_dataBuffer.isEmpty())
        {
            if (m_eof)
                completionHandler(SystemError::noError, BufferType());
            else
                m_readCompletionHandler = std::move(completionHandler);
            return;
        }

        auto dataBuffer = std::move(m_dataBuffer);
        m_dataBuffer.clear();
        completionHandler(SystemError::noError, std::move(dataBuffer));
    });
}

void WritableMessageBody::setOnBeforeDestructionHandler(
    nx::utils::MoveOnlyFunc<void()> handler)
{
    m_onBeforeDestructionHandler = std::move(handler);
}

void WritableMessageBody::writeBodyData(
    const QnByteArrayConstRef& data)
{
    post(
        [this, dataBuf = (QByteArray)data]() mutable
        {
            m_eof = dataBuf.isEmpty();
            if (m_readCompletionHandler)
            {
                auto hander = std::exchange(m_readCompletionHandler, nullptr);
                hander(SystemError::noError, std::move(dataBuf));
            }
            else
            {
                m_dataBuffer += std::move(dataBuf);
            }
        });
}

} // namespace nx::network::http
