#pragma once

#include "abstract_msg_body_source.h"

namespace nx {
namespace network {
namespace http {

class NX_NETWORK_API BufferSource:
    public AbstractMsgBodySource
{
public:
    BufferSource(
        StringType mimeType,
        BufferType msgBody);

    virtual StringType mimeType() const override;
    virtual boost::optional<uint64_t> contentLength() const override;
    virtual void readAsync(
        nx::utils::MoveOnlyFunc<
            void(SystemError::ErrorCode, BufferType)
        > completionHandler) override;

private:
    const StringType m_mimeType;
    BufferType m_msgBody;
};

} // namespace nx
} // namespace network
} // namespace http
