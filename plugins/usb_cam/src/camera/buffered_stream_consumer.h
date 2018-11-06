#pragma once

#include "abstract_stream_consumer.h"
#include "abstract_video_consumer.h"

#include <condition_variable>
#include <mutex>
#include <atomic>
#include <chrono>
#include <vector>
#include <deque>
#include <map>

namespace nx {
namespace usb_cam {

//--------------------------------------------------------------------------------------------------
// TimeStampedBuffer

template<typename V>
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

    virtual void insert(uint64_t key, const V& item)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_buffer.emplace(key, item);
        m_wait.notify_all();
    }

    /**
     * Peek at the oldest item in the buffer
     */
    V peekOldest(const std::chrono::milliseconds& timeOut)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!wait(lock, 0 /*minimumBufferSize*/, timeOut))
            return V();
        return m_buffer.begin()->second;
    }

    /**
     * Pop the oldest item off the buffer
     */
    virtual V popOldest(const std::chrono::milliseconds& timeOut)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!wait(lock, 0 /*minimumBufferSize*/, timeOut))
            return V();
        auto it = m_buffer.begin();
        auto v = it->second;
        m_buffer.erase(it);
        return v;
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
     *    to be >= timeSpan. Terminates early if interrupt() is called, returning false.
     * 
     * @param[in] timeSpan - The amount of items in terms of their timestamps that the buffer should
     *    contain.
     * @param[in] timeOut - The maximum amount of time to wait for the time span condition before
     *    terminating early
     * @return - true if the wait terminated due to satisfying the timespan condition, false if
     *    the wait terminated due to calling interrupt().
     */
    bool waitForTimeSpan(
        const std::chrono::milliseconds& timeSpan,
        const std::chrono::milliseconds& timeOut)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_wait.wait_for(
            lock,
            timeOut,
            [&]() { return m_interrupted || timeSpanInternal() >= timeSpan; });
        if (interrupted() || timeSpanInternal() < timeSpan)
            return false;
        return true;// < Timespan condition was satisfied
    }
    
    std::chrono::milliseconds timeSpan() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return timeSpanInternal();
    }

protected:
    std::condition_variable m_wait;
    mutable std::mutex m_mutex;
    std::map<uint64_t, V> m_buffer;
    bool m_interrupted = false;

protected:
    std::chrono::milliseconds timeSpanInternal() const
    {
        return m_buffer.empty()
            ? std::chrono::milliseconds(0)
            : std::chrono::milliseconds(m_buffer.rbegin()->first - m_buffer.begin()->first);
            // largest key minus smallest key
    }

    /**
     * Wait for at most timeOut for the number of items in the buffer to be > minimumBufferSize.
     *    Terminates early if interrupt() is called, returning false. Also returns false if the wait
     *    times out and the buffer size is not strictly larger than minimumBufferSize.
     * 
     * @params[in] lock - an std::unique lock that the condition_variable should wait on.
     * @param[in] minimumBufferSize - the minimum number of items that the buffer should contain for
     *    the wait condition to return true. Uses a strictly greater than comparison.
     * @params[in] timeOut - the maximum amount of time to wait (in milliseconds)
     *     before terminating early. If 0 is passed, waits indefinitely or until interrupt()
     *     is called.
     * @return - true if the buffer size grows larger than minimum buffer size, false if interrupted
     *    or time out.
     */
    bool wait(
        std::unique_lock<std::mutex>& lock,
        size_t minimumBufferSize,
        const std::chrono::milliseconds& timeOut)
    {
        if (timeOut.count() > 0)
        {
            m_wait.wait_for(
                lock,
                timeOut,
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

//--------------------------------------------------------------------------------------------------
// BufferedPacketConsumer

class BufferedPacketConsumer:
    public AbstractVideoConsumer,
    public AbstractPacketConsumer
{
public:
    BufferedPacketConsumer(
        const std::weak_ptr<VideoStream>& streamReader,
        const CodecParameters& params);

    void givePacket(const std::shared_ptr<ffmpeg::Packet>& packet);
    void flush();
    
    std::shared_ptr<ffmpeg::Packet> popOldest(
        const std::chrono::milliseconds& timeOut = std::chrono::milliseconds(0));

    std::shared_ptr<ffmpeg::Packet> peekOldest(
        const std::chrono::milliseconds& timeOut = std::chrono::milliseconds(0));

    void interrupt();

    bool waitForTimeSpan(
        const std::chrono::milliseconds& timeSpan,
        const std::chrono::milliseconds& timeOut);
    std::chrono::milliseconds timeSpan() const;

    size_t size() const;
    bool empty() const;

    std::vector<uint64_t> timestamps() const;

private:
    TimeStampedBuffer<std::shared_ptr<ffmpeg::Packet>> m_buffer;
    std::atomic_bool m_dropUntilNextVideoKeyPacket = false;
};

//--------------------------------------------------------------------------------------------------
// BufferedVideoFrameConsumer

class BufferedVideoFrameConsumer:
    public AbstractVideoConsumer,
    public AbstractFrameConsumer
{
public:
    BufferedVideoFrameConsumer(
        const std::weak_ptr<VideoStream>& streamReader,
        const CodecParameters& params);

    virtual void flush() override;
    virtual void giveFrame(const std::shared_ptr<ffmpeg::Frame>& frame) override;

    std::shared_ptr<ffmpeg::Frame> popOldest(
        const std::chrono::milliseconds& timeOut = std::chrono::milliseconds(0));

    std::shared_ptr<ffmpeg::Frame> peekOldest(
        const std::chrono::milliseconds& timeOut = std::chrono::milliseconds(0));

    void interrupt();

    size_t size() const;
    bool empty() const;

    std::vector<uint64_t> timestamps() const;

private:
    TimeStampedBuffer<std::shared_ptr<ffmpeg::Frame>> m_buffer;
};

} //namespace usb_cam
} //namespace nx