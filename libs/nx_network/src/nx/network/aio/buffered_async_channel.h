// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "async_channel_adapter.h"

namespace nx::network::aio {

class NX_NETWORK_API BufferedAsyncChannel:
    public AsyncChannelAdapter<std::unique_ptr<AbstractAsyncChannel>>
{
    using base_type = AsyncChannelAdapter<std::unique_ptr<AbstractAsyncChannel>>;

public:
    BufferedAsyncChannel(
        std::unique_ptr<AbstractAsyncChannel> source,
        nx::Buffer buffer);

    virtual void readSomeAsync(
        nx::Buffer* const buffer,
        IoCompletionHandler handler) override;

protected:
    virtual void cancelIoInAioThread(nx::network::aio::EventType eventType) override;

private:
    nx::Buffer m_internalRecvBuffer;
    std::unique_ptr<AbstractAsyncChannel> m_source;
};

} // namespace nx::network::aio
