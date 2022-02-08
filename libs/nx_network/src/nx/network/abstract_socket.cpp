// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_socket.h"

#include <future>

#include <nx/utils/string.h>
#include <nx/utils/thread/mutex_lock_analyzer.h>

#include "aio/aio_thread.h"
#include "aio/pollable.h"

namespace nx::network {

AbstractStreamSocket::~AbstractStreamSocket()
{
    if (m_beforeDestroyCallback)
        m_beforeDestroyCallback();
}

void AbstractStreamSocket::setBeforeDestroyCallback(nx::utils::MoveOnlyFunc<void()> callback)
{
    m_beforeDestroyCallback = std::move(callback);
}

//-------------------------------------------------------------------------------------------------
// class AbstractSocket.

bool AbstractSocket::bind(const std::string& localAddress, unsigned short localPort)
{
    return bind(SocketAddress(localAddress, localPort));
}

bool AbstractSocket::isInSelfAioThread() const
{
    // AbstractSocket does not provide const pollable() just to simplify implementation, so it is
    // safe to perform a const_cast.
    return const_cast<AbstractSocket*>(this)->pollable()->isInSelfAioThread();
}

//-------------------------------------------------------------------------------------------------
// class AbstractCommunicatingSocket.

bool AbstractCommunicatingSocket::connect(
    const std::string& foreignAddress,
    unsigned short foreignPort,
    std::chrono::milliseconds timeout)
{
    return connect(SocketAddress(foreignAddress, foreignPort), timeout);
}

int AbstractCommunicatingSocket::send(const nx::Buffer& data)
{
    return send(data.data(), data.size());
}

std::string AbstractCommunicatingSocket::getForeignHostName() const
{
    return getForeignAddress().address.toString();
}

void AbstractCommunicatingSocket::registerTimer(
    unsigned int timeout,
    nx::utils::MoveOnlyFunc<void()> handler)
{
    return registerTimer(
        std::chrono::milliseconds(timeout),
        std::move(handler));
}

void AbstractCommunicatingSocket::readAsyncAtLeast(
    nx::Buffer* const buffer, std::size_t minimalSize,
    IoCompletionHandler handler)
{
    NX_CRITICAL(
        buffer->capacity() >= buffer->size() + static_cast<int>(minimalSize),
        "Not enough space in the buffer!");

    if (minimalSize == 0)
    {
        handler(SystemError::noError, 0);
        return;
    }

    readAsyncAtLeastImpl(
        buffer, minimalSize, std::move(handler),
        static_cast<size_t>(buffer->size()));
}

void AbstractCommunicatingSocket::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    post(
        [this, handler = std::move(handler)]()
        {
            pleaseStopSync();
            handler();
        });
}

void AbstractCommunicatingSocket::pleaseStopSync()
{
    if (isInSelfAioThread())
    {
        cancelIOSync(aio::EventType::etNone);
        // TODO: #akolesnikov Refactor after inheriting AbstractSocket from BasicPollable.
        if (pollable())
            pollable()->getAioThread()->cancelPostedCalls(pollable());
    }
    else
    {
        std::promise<void> stopped;
        pleaseStop([&stopped]() { stopped.set_value(); });
        stopped.get_future().wait();
    }
}

void AbstractCommunicatingSocket::cancelIOAsync(
    nx::network::aio::EventType eventType,
    nx::utils::MoveOnlyFunc<void()> handler)
{
    this->post(
        [this, eventType, handler = std::move(handler)]()
        {
            cancelIoInAioThread(eventType);
            handler();
        });
}

void AbstractCommunicatingSocket::cancelIOSync(nx::network::aio::EventType eventType)
{
    if (isInSelfAioThread())
    {
        cancelIoInAioThread(eventType);
    }
    else
    {
        std::promise<void> promise;
        post(
            [this, eventType, &promise]()
            {
                cancelIoInAioThread(eventType);
                promise.set_value();
            });
        promise.get_future().wait();
    }
}

void AbstractCommunicatingSocket::readAsyncAtLeastImpl(
    nx::Buffer* const buffer, std::size_t minimalSize,
    IoCompletionHandler handler,
    std::size_t initBufSize)
{
    readSomeAsync(
        buffer,
        [this, buffer, minimalSize, handler = std::move(handler), initBufSize](
            SystemError::ErrorCode code, std::size_t size) mutable
        {
            if (code != SystemError::noError || size == 0 ||
                static_cast<std::size_t>(buffer->size()) >= initBufSize + minimalSize)
            {
                return handler(
                    code, static_cast<std::size_t>(buffer->size()) - initBufSize);
            }

            readAsyncAtLeastImpl(
                buffer, minimalSize, std::move(handler), initBufSize);
        });
}

//-------------------------------------------------------------------------------------------------

void AbstractStreamServerSocket::cancelIOAsync(
    nx::utils::MoveOnlyFunc<void()> handler)
{
    post(
        [this, handler = std::move(handler)]()
        {
            cancelIoInAioThread();
            handler();
        });
}

void AbstractStreamServerSocket::cancelIOSync()
{
    if (isInSelfAioThread())
    {
        cancelIoInAioThread();
    }
    else
    {
        std::promise<void> promise;
        post(
            [this, &promise]()
            {
                cancelIoInAioThread();
                promise.set_value();
            });
        promise.get_future().wait();
    }
}

//-------------------------------------------------------------------------------------------------

bool AbstractDatagramSocket::setDestAddr(
    const std::string& foreignAddress,
    unsigned short foreignPort)
{
    return setDestAddr(SocketAddress(foreignAddress, foreignPort));
}

bool AbstractDatagramSocket::sendTo(
    const void* buffer,
    std::size_t bufferLen,
    const std::string& foreignAddress,
    unsigned short foreignPort)
{
    return sendTo(
        buffer,
        bufferLen,
        SocketAddress(foreignAddress, foreignPort));
}

bool AbstractDatagramSocket::sendTo(
    const nx::Buffer& buf,
    const SocketAddress& foreignAddress)
{
    return sendTo(buf.data(), buf.size(), foreignAddress);
}

} // namespace nx::network
