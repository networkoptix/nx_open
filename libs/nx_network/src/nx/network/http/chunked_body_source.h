// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_msg_body_source.h"

namespace nx::network::http {

/**
 * Reads message body data from another AbstractMsgBodySource instance
 * and encodes it to the chunked encoding [rfc7230, 4.1].
 * Each buffer read from the source stream is encoded as a single chunk.
 */
class NX_NETWORK_API ChunkedBodySource:
    public AbstractMsgBodySource
{
    using base_type = AbstractMsgBodySource;

public:
    ChunkedBodySource(std::unique_ptr<AbstractMsgBodySource> body);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual std::string mimeType() const override;

    virtual std::optional<uint64_t> contentLength() const override;

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
