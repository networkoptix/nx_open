#pragma once

#include "abstract_stream_consumer.h"

#include <condition_variable>
#include <mutex>
#include <atomic>
#include <chrono>
#include <vector>
#include <deque>

namespace nx::usb_cam {

class BufferedPacketConsumer: public AbstractPacketConsumer
{
public:
    // AbstractPacketConsumer
    void givePacket(const std::shared_ptr<ffmpeg::Packet>& packet);
    void flush();

public:
    bool empty() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_buffer.empty();
    }

    size_t size() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_buffer.size();
    }

    size_t sizeBytes() const
    {
        return m_bufferSizeBytes;
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_buffer.size() > 0)
            m_buffer.clear();
        m_bufferSizeBytes = 0;
    }

    virtual void push(const std::shared_ptr<ffmpeg::Packet>& packet)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_buffer.emplace_back(packet);
        m_bufferSizeBytes += packet->size();
        m_wait.notify_all();
    }

    virtual std::shared_ptr<ffmpeg::Packet> pop()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_wait.wait(lock, [this]() { return m_interrupted || !m_buffer.empty(); });
        if (m_buffer.empty())
            return nullptr;

        std::shared_ptr<ffmpeg::Packet> packet = m_buffer.front();
        m_buffer.pop_front();
        m_bufferSizeBytes -= packet->size();
        return packet;
    }

    /**
     * Interrupts the buffer if waiting for input.
     */
    void interrupt()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_interrupted = true;
        m_wait.notify_all();
    }

private:
    std::atomic_bool m_dropUntilNextVideoKeyPacket = true;
    std::atomic_int m_bufferSizeBytes = 0;
    std::condition_variable m_wait;
    mutable std::mutex m_mutex;
    std::deque<std::shared_ptr<ffmpeg::Packet>> m_buffer;
    bool m_interrupted = false;

};

} // namespace nx::usb_cam
