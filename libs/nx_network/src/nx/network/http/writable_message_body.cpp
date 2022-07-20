// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "writable_message_body.h"

#include <nx/network/aio/aio_service.h>
#include <nx/network/socket_global.h>
#include <nx/utils/byte_stream/custom_output_stream.h>

namespace nx::network::http {

MessageBodyWriter::State::State(WritableMessageBody* body):
    body(body),
    mutex(nx::Mutex::Recursive)
{
}

MessageBodyWriter::MessageBodyWriter(WritableMessageBody* body):
    m_sharedState(std::make_shared<State>(body))
{
    m_sharedState->body->setOnBeforeDestructionHandler(
        [this]()
        {
            NX_MUTEX_LOCKER lock(&m_sharedState->mutex);
            m_sharedState->body = nullptr;
        });
}

void MessageBodyWriter::writeBodyData(nx::Buffer data)
{
    NX_MUTEX_LOCKER lock(&m_sharedState->mutex);
    auto sharedState = m_sharedState;

    if (sharedState->body)
        sharedState->body->writeBodyData(std::move(data));

    lock.unlock();
}

void MessageBodyWriter::writeEof(SystemError::ErrorCode resultCode)
{
    // Using sharedState since body->writeEof can cause body destruction
    // which in turn can cause this destruction.

    NX_MUTEX_LOCKER lock(&m_sharedState->mutex);
    auto sharedState = m_sharedState;

    if (sharedState->body)
        sharedState->body->writeEof(resultCode);

    lock.unlock();
}

//-------------------------------------------------------------------------------------------------

WritableMessageBody::WritableMessageBody(
    const std::string& mimeType,
    std::optional<uint64_t> contentLength)
    :
    m_mimeType(mimeType),
    m_contentLength(contentLength)
{
}

WritableMessageBody::~WritableMessageBody()
{
    if (m_onBeforeDestructionHandler)
        nx::utils::swapAndCall(m_onBeforeDestructionHandler);
}

void WritableMessageBody::stopWhileInAioThread()
{
    if (m_onBeforeDestructionHandler)
        nx::utils::swapAndCall(m_onBeforeDestructionHandler);
}

std::string WritableMessageBody::mimeType() const
{
    return m_mimeType;
}

std::optional<uint64_t> WritableMessageBody::contentLength() const
{
    return m_contentLength;
}

void WritableMessageBody::readAsync(CompletionHandler completionHandler)
{
    // NOTE: Minimizing the number of posts here, but still ensuring that the completion handler
    // is invoked through post.

    dispatch(
        [this, completionHandler = std::move(completionHandler),
            callerAioThread = SocketGlobals::instance().aioService().getCurrentAioThread()]() mutable
        {
            NX_ASSERT(!m_readCompletionHandler);

            auto dataToDeliver = peek();
            if (!dataToDeliver)
            {
                m_readCompletionHandler = std::move(completionHandler);
                return;
            }

            deliverData(
                std::move(completionHandler),
                std::move(*dataToDeliver),
                callerAioThread != SocketGlobals::instance().aioService().getCurrentAioThread());
        });
}

std::optional<std::tuple<SystemError::ErrorCode, nx::Buffer>> WritableMessageBody::peek()
{
    if (m_dataBuffer.empty() && m_eof)
        return std::make_tuple(*m_eof, nx::Buffer());
    else if (!m_dataBuffer.empty())
        return std::make_tuple(SystemError::noError, std::exchange(m_dataBuffer, {}));
    else
        return std::nullopt;
}

void WritableMessageBody::cancelRead()
{
    executeInAioThreadSync(
        [this]()
        {
            m_readCompletionHandler = nullptr;
        });
}

void WritableMessageBody::setOnBeforeDestructionHandler(
    nx::utils::MoveOnlyFunc<void()> handler)
{
    m_onBeforeDestructionHandler = std::move(handler);
}

void WritableMessageBody::writeBodyData(nx::Buffer data)
{
    dispatch(
        [this, data = std::move(data)]() mutable
        {
            if (!m_eof && data.empty())
                m_eof = SystemError::noError;

            if (m_readCompletionHandler)
            {
                nx::utils::swapAndCall(
                    m_readCompletionHandler,
                    m_eof ? *m_eof : SystemError::noError, std::move(data));
            }
            else
            {
                m_dataBuffer += std::move(data);
            }
        });
}

void WritableMessageBody::writeEof(SystemError::ErrorCode resultCode)
{
    dispatch(
        [this, resultCode]()
        {
            m_eof = resultCode;

            if (m_readCompletionHandler)
            {
                NX_ASSERT(m_dataBuffer.empty());
                nx::utils::swapAndCall(m_readCompletionHandler, *m_eof, nx::Buffer());
            }
        });
}

std::shared_ptr<MessageBodyWriter> WritableMessageBody::writer()
{
    if (!m_writer)
        m_writer = std::make_shared<MessageBodyWriter>(this);

    return m_writer;
}

void WritableMessageBody::deliverData(
    CompletionHandler completionHandler,
    std::tuple<SystemError::ErrorCode, nx::Buffer> data,
    bool deliverDirectly)
{
    if (deliverDirectly)
    {
        // post has been implicitly executed by dispatch.
        completionHandler(
            std::get<0>(data), std::move(std::get<1>(data)));
    }
    else
    {
        post(
            [this, completionHandler = std::move(completionHandler),
                data = std::move(data)]() mutable
            {
                deliverData(
                    std::move(completionHandler),
                    std::move(data),
                    /*deliverDirectly*/ true);
            });
    }
}

} // namespace nx::network::http
