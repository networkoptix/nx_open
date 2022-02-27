// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stream_socket_stub.h"

#include <thread>

namespace nx::network::test {

StreamSocketStub::StreamSocketStub():
    base_type(&m_delegate)
{
    setNonBlockingMode(true);

    m_timer.bindToAioThread(getAioThread());
}

StreamSocketStub::~StreamSocketStub()
{
    m_timer.pleaseStopSync();
}

void StreamSocketStub::post(nx::utils::MoveOnlyFunc<void()> func)
{
    if (m_postDelay)
        m_timer.start(*m_postDelay, std::move(func));
    else
        base_type::post(std::move(func));
}

void StreamSocketStub::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    m_timer.bindToAioThread(aioThread);
}

void StreamSocketStub::readSomeAsync(
    nx::Buffer* const buffer,
    IoCompletionHandler handler)
{
    base_type::post(
        [this, buffer, handler = std::move(handler)]() mutable
        {
            m_readBuffer = buffer;
            m_readHandler = std::move(handler);

            if (m_connectionClosed)
                reportConnectionClosure();
        });
}

void StreamSocketStub::sendAsync(
    const nx::Buffer* buffer,
    IoCompletionHandler handler)
{
    auto func =
        [this, buffer, handler = std::move(handler)]()
        {
            if (!m_sendCompletesAfterConnectionClosure && m_connectionClosed)
                return;

            m_reflectingPipeline.write(buffer->data(), buffer->size());
            handler(SystemError::noError, buffer->size());
        };

    if (m_postDelay)
        m_timer.start(*m_postDelay, std::move(func));
    else
        base_type::post(std::move(func));
}

SocketAddress StreamSocketStub::getForeignAddress() const
{
    return m_foreignAddress;
}

bool StreamSocketStub::setKeepAlive(std::optional<KeepAliveOptions> info)
{
    m_keepAliveOptions = info;
    return true;
}

bool StreamSocketStub::getKeepAlive(std::optional<KeepAliveOptions>* result) const
{
    *result = m_keepAliveOptions;
    return true;
}

void StreamSocketStub::cancelIoInAioThread(nx::network::aio::EventType eventType)
{
    if (eventType & nx::network::aio::EventType::etRead)
        m_readBuffer = nullptr;
    base_type::cancelIoInAioThread(eventType);
}

nx::Buffer StreamSocketStub::read()
{
    while (m_reflectingPipeline.totalBytesThrough() == 0)
        std::this_thread::yield();
    return m_reflectingPipeline.readAll();
}

void StreamSocketStub::setConnectionToClosedState()
{
    base_type::post(
        [this]()
        {
            m_connectionClosed = true;

            if (m_readHandler) //< We have a pending read.
                reportConnectionClosure();
        });
}

void StreamSocketStub::setForeignAddress(const SocketAddress& endpoint)
{
    m_foreignAddress = endpoint;
}

void StreamSocketStub::setPostDelay(std::optional<std::chrono::milliseconds> postDelay)
{
    m_postDelay = postDelay;
}

void StreamSocketStub::setSendCompletesAfterConnectionClosure(bool value)
{
    m_sendCompletesAfterConnectionClosure = value;
}

void StreamSocketStub::reportConnectionClosure()
{
    m_readBuffer->append(
        "Just checking that buffer is always alive when completion handler is triggered");
    nx::utils::swapAndCall(m_readHandler, SystemError::noError, 0);
}

} // namespace nx::network::test
