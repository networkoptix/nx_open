#pragma once

#include "abstract_stream_consumer.h"

#include <condition_variable>
#include <mutex>
#include <atomic>
#include <chrono>
#include <vector>
#include <deque>
#include <map>

namespace nx::usb_cam {

//-------------------------------------------------------------------------------------------------
// TimeStampedBuffer

template<typename Item>
class TimeStampedBuffer
{
public:
    TimeStampedBuffer() = default;

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

    void clear()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_buffer.size() > 0)
            m_buffer.clear();
    }

    virtual void insert(uint64_t key, const Item& item)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_buffer.emplace(key, item);
        m_wait.notify_all();
    }

    /**
     * Peek at the oldest item in the buffer
     */
    Item peekOldest(const std::chrono::milliseconds& timeout)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!wait(lock, 0 /*minimumBufferSize*/, timeout))
            return Item();
        return m_buffer.begin()->second;
    }

    /**
     * Pop the oldest item off the buffer
     */
    virtual Item popOldest(const std::chrono::milliseconds& timeout)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!wait(lock, 0 /*minimumBufferSize*/, timeout))
            return Item();
        auto it = m_buffer.begin();
        auto item = it->second;
        m_buffer.erase(it);
        return item;
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

    std::vector<uint64_t> timestamps() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<uint64_t> timestamps;
        timestamps.reserve(m_buffer.size());
        for (const auto & item : m_buffer)
            timestamps.push_back(item.first);
        return timestamps;
    }

    /**
     * Wait for the difference in timestamps between the oldest and newest items in buffer 
     *    to be >= timespan. Terminates early if interrupt() is called, returning false.
     * 
     * @param[in] timespan - The amount of items in terms of their timestamps that the buffer 
     *    should contain.
     * @param[in] timeout - The maximum amount of time to wait for the time span condition before
     *    terminating early
     * @return - true if the wait terminated due to satisfying the timespan condition, false if
     *    the wait terminated due to calling interrupt().
     */
    bool waitForTimespan(
        const std::chrono::milliseconds& timespan,
        const std::chrono::milliseconds& timeout)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_wait.wait_for(
            lock,
            timeout,
            [&]() { return m_interrupted || timespanInternal() >= timespan; });
        if (interrupted() || timespanInternal() < timespan)
            return false;
        return true;// < Timespan condition was satisfied
    }

    std::chrono::milliseconds timespan() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return timespanInternal();
    }

protected:
    std::condition_variable m_wait;
    mutable std::mutex m_mutex;
    std::map<uint64_t, Item> m_buffer;
    bool m_interrupted = false;

protected:
    std::chrono::milliseconds timespanInternal() const
    {
        return m_buffer.empty()
            ? std::chrono::milliseconds(0)
            : std::chrono::milliseconds(m_buffer.rbegin()->first - m_buffer.begin()->first);
            // Largest key minus smallest key, or oldest - newest.
    }

    /**
     * Wait for at most timeout for the number of items in the buffer to be > minimumBufferSize.
     *    Terminates early if interrupt() is called, returning false. Also returns false if the
     *    wait times out and the buffer size is not strictly larger than minimumBufferSize.
     * 
     * @params[in] lock - an std::unique lock that the condition_variable should wait on.
     * @param[in] minimumBufferSize - the minimum number of items that the buffer should contain 
     *    for the wait condition to return true. Uses a strictly greater than comparison.
     * @params[in] timeout - the maximum amount of time to wait (in milliseconds)
     *     before terminating early. If 0 is passed, waits indefinitely or until interrupt()
     *     is called.
     * @return - true if the buffer size grows larger than minimum buffer size, false if 
     *    interrupted or time out.
     */
    bool wait(
        std::unique_lock<std::mutex>& lock,
        size_t minimumBufferSize,
        const std::chrono::milliseconds& timeout)
    {
        if (timeout.count() > 0)
        {
            m_wait.wait_for(
                lock,
                timeout,
                [&](){ return m_interrupted || m_buffer.size() > minimumBufferSize; });
        }
        else
        {
            m_wait.wait(
                lock,
                [&](){ return m_interrupted || m_buffer.size() > minimumBufferSize; });
        }
        if (interrupted())
            return false;
        return m_buffer.size() > minimumBufferSize;
    }

    bool interrupted()
    {
        if (m_interrupted)
        {
            m_interrupted = false;
            return true;
        }
        return false;
    }
};

//-------------------------------------------------------------------------------------------------
// BufferedPacketConsumer

class BufferedPacketConsumer: public AbstractPacketConsumer
{
public:
    void givePacket(const std::shared_ptr<ffmpeg::Packet>& packet);
    void flush();

    std::shared_ptr<ffmpeg::Packet> popOldest(
        const std::chrono::milliseconds& timeout = std::chrono::milliseconds(0));

    std::shared_ptr<ffmpeg::Packet> peekOldest(
        const std::chrono::milliseconds& timeout = std::chrono::milliseconds(0));

    void interrupt();

    bool waitForTimespan(
        const std::chrono::milliseconds& timespan,
        const std::chrono::milliseconds& timeout);
    std::chrono::milliseconds timespan() const;

    size_t size() const;
    bool empty() const;

    std::vector<uint64_t> timestamps() const;

private:
    TimeStampedBuffer<std::shared_ptr<ffmpeg::Packet>> m_buffer;
    std::atomic_bool m_dropUntilNextVideoKeyPacket = true;
};

//-------------------------------------------------------------------------------------------------
// BufferedVideoFrameConsumer

class BufferedVideoFrameConsumer: public AbstractFrameConsumer
{
public:

    virtual void flush() override;
    virtual void giveFrame(const std::shared_ptr<ffmpeg::Frame>& frame) override;

    std::shared_ptr<ffmpeg::Frame> popOldest(
        const std::chrono::milliseconds& timeout = std::chrono::milliseconds(0));

    std::shared_ptr<ffmpeg::Frame> peekOldest(
        const std::chrono::milliseconds& timeout = std::chrono::milliseconds(0));

    void interrupt();

    size_t size() const;
    bool empty() const;

    std::vector<uint64_t> timestamps() const;

private:
    TimeStampedBuffer<std::shared_ptr<ffmpeg::Frame>> m_buffer;
};

} // namespace nx::usb_cam
