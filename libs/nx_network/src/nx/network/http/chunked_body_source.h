#pragma once

#include "abstract_msg_body_source.h"

namespace nx::network::http {

/**
 * Reads message body data from another AbstractMsgBodySource instance
 * and encodes it to the chunked encoding [rfc7230, 4.1].
 */
class NX_NETWORK_API ChunkedBodySource:
    public AbstractMsgBodySource
{
    using base_type = AbstractMsgBodySource;

public:
    ChunkedBodySource(std::unique_ptr<AbstractMsgBodySource> body);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual StringType mimeType() const override;

    virtual boost::optional<uint64_t> contentLength() const override;

    virtual void readAsync(
        nx::utils::MoveOnlyFunc<
        void(SystemError::ErrorCode, nx::Buffer)
        > completionHandler) override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    std::unique_ptr<AbstractMsgBodySource> m_body;
    bool m_eof = false;
};

} // namespace nx::network::http
