// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/socket_delegate.h>

namespace nx::network::http::tunneling::detail {

/**
 * Socket that delegates reads and writes to different sockets.
 */
class NX_NETWORK_API SeparateUpDownChannelDelegate:
    public StreamSocketDelegate
{
    using base_type = StreamSocketDelegate;

public:
    enum class Purpose
    {
        client,
        server,
    };

    SeparateUpDownChannelDelegate(
        std::unique_ptr<AbstractStreamSocket> receiveChannel,
        std::unique_ptr<AbstractStreamSocket> sendChannel,
        Purpose purpose);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual bool setNonBlockingMode(bool val) override;

    virtual void readSomeAsync(
        nx::Buffer* const buffer,
        IoCompletionHandler handler) override;

    virtual void sendAsync(
        const nx::Buffer* buffer,
        IoCompletionHandler handler) override;

    virtual int recv(void* buffer, std::size_t bufferLen, int flags) override;
    virtual int send(const void* buffer, std::size_t bufferLen) override;

protected:
    virtual void cancelIoInAioThread(nx::network::aio::EventType eventType) override;

private:
    std::unique_ptr<AbstractStreamSocket> m_receiveChannel;
    std::unique_ptr<AbstractStreamSocket> m_sendChannel;
};

} // namespace nx::network::http::tunneling::detail
