#pragma once

#include <condition_variable>
#include <mutex>

#include <nx/network/aio/abstract_async_channel.h>

namespace nx::vms::server::http_audio {

class SyncReader
{
public:
    SyncReader(nx::network::aio::AsyncChannelPtr socket);
    int read(uint8_t* buffer, int size);
    void cancel();

private:
    nx::network::aio::AsyncChannelPtr m_socket;

    std::mutex m_mutex;
    std::condition_variable m_condition;
    bool m_readComplete = true;

    int m_bufferSize = 0;
    int m_bufferOffset = 0;
    nx::Buffer m_buffer;
};

} // namespace nx::vms::server::http_audio
