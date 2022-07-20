// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <tuple>

#include <nx/utils/thread/mutex.h>

#include "abstract_msg_body_source.h"

namespace nx::network::http {

class WritableMessageBody;

/**
 * Writers to an associated WritableMessageBody.
 */
class NX_NETWORK_API MessageBodyWriter
{
public:
    MessageBodyWriter(WritableMessageBody* body);
    virtual ~MessageBodyWriter() = default;

    /**
     * See WritableMessageBody::writeBodyData().
     */
    void writeBodyData(nx::Buffer data);

    /**
     * See WritableMessageBody::writeEof().
     */
    void writeEof(SystemError::ErrorCode resultCode = SystemError::noError);

private:
    struct State
    {
        WritableMessageBody* body = nullptr;
        nx::Mutex mutex;

        State(WritableMessageBody* body);
    };

    std::shared_ptr<State> m_sharedState;
};

/**
 * Message body source that sends data upon availibility.
 * To make body data available, call WritableMessageBody::writeBodyData.
 * Another way is to use WritableMessageBody::writer().
 * This allows to make life-times or writer and WritableMessageBody itself
 * totally independent.
 */
class NX_NETWORK_API WritableMessageBody:
    public AbstractMsgBodySourceWithCache
{
public:
    WritableMessageBody(
        const std::string& mimeType,
        std::optional<uint64_t> contentLength = std::nullopt);

    ~WritableMessageBody();

    virtual void stopWhileInAioThread() override;

    virtual std::string mimeType() const override;
    virtual std::optional<uint64_t> contentLength() const override;

    virtual void readAsync(CompletionHandler completionHandler) override;

    virtual std::optional<std::tuple<SystemError::ErrorCode, nx::Buffer>> peek() override;

    virtual void cancelRead() override;

    void setOnBeforeDestructionHandler(nx::utils::MoveOnlyFunc<void()> handler);

    /**
     * @param data Empty buffer signals EOF.
     */
    void writeBodyData(nx::Buffer data);

    /**
     * Convenience method that invokes WritableMessageBody::writeBodyData.
     */
    void writeEof(SystemError::ErrorCode resultCode = SystemError::noError);

    std::shared_ptr<MessageBodyWriter> writer();

private:
    const std::string m_mimeType;
    std::optional<uint64_t> m_contentLength;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, nx::Buffer)> m_readCompletionHandler;
    nx::Buffer m_dataBuffer;
    nx::utils::MoveOnlyFunc<void()> m_onBeforeDestructionHandler;
    std::optional<SystemError::ErrorCode> m_eof;
    std::shared_ptr<MessageBodyWriter> m_writer;

    void deliverData(
        CompletionHandler completionHandler,
        std::tuple<SystemError::ErrorCode, nx::Buffer> data,
        bool deliverDirectly);
};

} // namespace nx::network::http
