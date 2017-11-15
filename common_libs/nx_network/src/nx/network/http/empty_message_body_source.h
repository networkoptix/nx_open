#pragma once

#include "abstract_msg_body_source.h"

namespace nx_http {

/**
 * Should be used when HTTP request handler wishes to set Content-Length
 * to some specific value (or just omit it) and at the same time does not provide body at all.
 */
class NX_NETWORK_API EmptyMessageBodySource:
    public AbstractMsgBodySource
{
public:
    EmptyMessageBodySource(
        StringType contentType,
        const boost::optional<uint64_t>& contentLength);
    ~EmptyMessageBodySource();

    virtual void stopWhileInAioThread() override;

    virtual StringType mimeType() const override;
    virtual boost::optional<uint64_t> contentLength() const override;
    /**
     * Always reports end-of-stream
     */
    virtual void readAsync(
        nx::utils::MoveOnlyFunc<
            void(SystemError::ErrorCode, BufferType)> completionHandler) override;

private:
    StringType m_contentType;
    boost::optional<uint64_t> m_contentLength;
};

} // namespace nx_http
