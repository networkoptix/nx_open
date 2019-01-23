#pragma once

#include <string>

#include "abstract_msg_body_source.h"

namespace nx::network::http {

/**
 * Message body source that sends data upon availibility.
 * To make body data available, call WritableMessageBody::writeBodyData.
 * 
 */
class NX_NETWORK_API WritableMessageBody:
    public AbstractMsgBodySource
{
public:
    WritableMessageBody(
        const std::string& mimeType,
        boost::optional<uint64_t> contentLength = boost::none);

    virtual void stopWhileInAioThread() override;

    virtual StringType mimeType() const override;
    virtual boost::optional<uint64_t> contentLength() const override;
    virtual void readAsync(
        nx::utils::MoveOnlyFunc<
        void(SystemError::ErrorCode, BufferType)
        > completionHandler) override;

    void setOnBeforeDestructionHandler(nx::utils::MoveOnlyFunc<void()> handler);

    /**
     * @param data Empty buffer signals EOF.
     */
    void writeBodyData(const QnByteArrayConstRef& data);

private:
    const std::string m_mimeType;
    boost::optional<uint64_t> m_contentLength;
    nx::utils::MoveOnlyFunc<
        void(SystemError::ErrorCode, BufferType)
    > m_readCompletionHandler;
    BufferType m_dataBuffer;
    nx::utils::MoveOnlyFunc<void()> m_onBeforeDestructionHandler;
    bool m_eof = false;
};

} // namespace nx::network::http
