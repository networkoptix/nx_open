#include "chunked_body_source.h"

#include "chunked_transfer_encoder.h"

namespace nx::network::http {

ChunkedBodySource::ChunkedBodySource(
    std::unique_ptr<AbstractMsgBodySource> body)
    :
    m_body(std::move(body))
{
    bindToAioThread(m_body->getAioThread());
}

void ChunkedBodySource::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_body)
        m_body->bindToAioThread(aioThread);
}

StringType ChunkedBodySource::mimeType() const
{
    return m_body->mimeType();
}

boost::optional<uint64_t> ChunkedBodySource::contentLength() const
{
    // NOTE: Even if m_body provides Content-Length (what would be weird),
    // not reporting it since it does not make any sense with the chunked encoding.
    return boost::none;
}

void ChunkedBodySource::readAsync(
    nx::utils::MoveOnlyFunc<
        void(SystemError::ErrorCode, nx::Buffer)
    > completionHandler)
{
    m_body->readAsync(
        [this, completionHandler = std::move(completionHandler)](
            SystemError::ErrorCode resultCode, nx::Buffer buffer)
        {
            if (m_eof || resultCode != SystemError::noError)
                return completionHandler(resultCode, nx::Buffer());

            if (resultCode == SystemError::noError && buffer.isEmpty())
                m_eof = true; //< Still have to report the final chunk.

            completionHandler(
                resultCode,
                QnChunkedTransferEncoder::serializeSingleChunk(std::move(buffer)));
        });
}

void ChunkedBodySource::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_body.reset();
}

} // namespace nx::network::http
