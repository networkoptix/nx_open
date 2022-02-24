// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "separate_up_down_channel_delegate.h"

namespace nx::network::http::tunneling::detail {

SeparateUpDownChannelDelegate::SeparateUpDownChannelDelegate(
    std::unique_ptr<AbstractStreamSocket> receiveChannel,
    std::unique_ptr<AbstractStreamSocket> sendChannel,
    Purpose purpose)
    :
    base_type(purpose == Purpose::client ? receiveChannel.get() : sendChannel.get()),
    m_receiveChannel(std::move(receiveChannel)),
    m_sendChannel(std::move(sendChannel))
{
    bindToAioThread(m_receiveChannel->getAioThread());
}

void SeparateUpDownChannelDelegate::bindToAioThread(
    aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_receiveChannel->bindToAioThread(aioThread);
    m_sendChannel->bindToAioThread(aioThread);
}

bool SeparateUpDownChannelDelegate::setNonBlockingMode(bool val)
{
    return m_receiveChannel->setNonBlockingMode(val)
        && m_sendChannel->setNonBlockingMode(val);
}

void SeparateUpDownChannelDelegate::readSomeAsync(
    nx::Buffer* const buffer,
    IoCompletionHandler handler)
{
    return m_receiveChannel->readSomeAsync(buffer, std::move(handler));
}

void SeparateUpDownChannelDelegate::sendAsync(
    const nx::Buffer* buffer,
    IoCompletionHandler handler)
{
    return m_sendChannel->sendAsync(buffer, std::move(handler));
}

int SeparateUpDownChannelDelegate::recv(void* buffer, std::size_t bufferLen, int flags)
{
    return m_receiveChannel->recv(buffer, bufferLen, flags);
}

int SeparateUpDownChannelDelegate::send(const void* buffer, std::size_t bufferLen)
{
    return m_sendChannel->send(buffer, bufferLen);
}

void SeparateUpDownChannelDelegate::cancelIoInAioThread(
    nx::network::aio::EventType eventType)
{
    m_receiveChannel->cancelIOSync(eventType);
    m_sendChannel->cancelIOSync(eventType);
}

} // namespace nx::network::http::tunneling::detail
