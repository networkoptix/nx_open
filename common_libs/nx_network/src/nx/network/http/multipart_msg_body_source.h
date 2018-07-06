#pragma once

#include "abstract_msg_body_source.h"

#include "multipart_body_serializer.h"

namespace nx {
namespace network {
namespace http {

/**
 * Used to generate and stream HTTP multipart message body.
 */
class NX_NETWORK_API MultipartMessageBodySource:
    public AbstractMsgBodySource
{
public:
    MultipartMessageBodySource(StringType boundary);
    virtual ~MultipartMessageBodySource();

    virtual void stopWhileInAioThread() override;

    virtual StringType mimeType() const override;
    virtual boost::optional<uint64_t> contentLength() const override;
    virtual void readAsync(
        nx::utils::MoveOnlyFunc<
            void(SystemError::ErrorCode, BufferType)
        > completionHandler) override;

    void setOnBeforeDestructionHandler(nx::utils::MoveOnlyFunc<void()> handler);

    MultipartBodySerializer* serializer();

private:
    MultipartBodySerializer m_multipartBodySerializer;
    nx::utils::MoveOnlyFunc<
        void(SystemError::ErrorCode, BufferType)
    > m_readCompletionHandler;
    BufferType m_dataBuffer;
    nx::utils::MoveOnlyFunc<void()> m_onBeforeDestructionHandler;
    bool m_eof;

    void onSomeDataAvailable(const QnByteArrayConstRef& data);
};

} // namespace nx
} // namespace network
} // namespace http
