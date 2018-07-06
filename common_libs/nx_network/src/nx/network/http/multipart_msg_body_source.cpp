#include "multipart_msg_body_source.h"

#include <nx/utils/byte_stream/custom_output_stream.h>

namespace nx {
namespace network {
namespace http {

MultipartMessageBodySource::MultipartMessageBodySource(StringType boundary):
    m_multipartBodySerializer(
        std::move(boundary),
        nx::utils::bstream::makeCustomOutputStream(
            std::bind(
                &MultipartMessageBodySource::onSomeDataAvailable, this,
                std::placeholders::_1))),
    m_eof(false)
{
}

MultipartMessageBodySource::~MultipartMessageBodySource()
{
    stopWhileInAioThread();
}

void MultipartMessageBodySource::stopWhileInAioThread()
{
    auto onBeforeDestructionHandler = std::move(m_onBeforeDestructionHandler);
    m_onBeforeDestructionHandler = nullptr;
    if (onBeforeDestructionHandler)
        onBeforeDestructionHandler();
}

StringType MultipartMessageBodySource::mimeType() const
{
    return m_multipartBodySerializer.contentType();
}

boost::optional<uint64_t> MultipartMessageBodySource::contentLength() const
{
    return boost::none;
}

void MultipartMessageBodySource::readAsync(
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

void MultipartMessageBodySource::setOnBeforeDestructionHandler(
    nx::utils::MoveOnlyFunc<void()> handler)
{
    m_onBeforeDestructionHandler = std::move(handler);
}

MultipartBodySerializer* MultipartMessageBodySource::serializer()
{
    return &m_multipartBodySerializer;
}

void MultipartMessageBodySource::onSomeDataAvailable(
    const QnByteArrayConstRef& data)
{
    NX_ASSERT(!data.isEmpty());

    const bool eof = m_multipartBodySerializer.eof();
    post(
        [this, dataBuf = (QByteArray)data, eof]() mutable
        {
            m_eof = eof;
            if (m_readCompletionHandler)
            {
                auto hander = std::move(m_readCompletionHandler);
                m_readCompletionHandler = nullptr;
                hander(SystemError::noError, std::move(dataBuf));
            }
            else
            {
                m_dataBuffer += std::move(dataBuf);
            }
        });
}

} // namespace nx
} // namespace network
} // namespace http
