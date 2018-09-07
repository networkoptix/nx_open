#pragma once

#include "abstract_video_consumer.h"

#include <condition_variable>
#include <mutex>
#include <deque>
#include <map>

namespace nx {
namespace usb_cam {

//////////////////////////////////////////// Buffer<T> /////////////////////////////////////////////

template<typename K, typename V>
class Map
{
public:
    Map():
        m_interrupted(false)
    {
    }

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

    virtual void pushBack(const K& key, const V& item)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_buffer.insert(std::pair<K, V>(key, item));
        m_wait.notify_all();
    }

    V peekFront(bool waitForItem = false)
    {
        if (!waitForItem)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_buffer.empty() ? V() : m_buffer.begin()->second;
        }
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_buffer.empty() && !wait(lock))
            return V();
        if (m_buffer.empty())
            return V();
        return m_buffer.begin()->second;
    }

    virtual V popFront(uint64_t timeOut = 0)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_buffer.empty() && !wait(lock, 0, timeOut))
            return V();
        if (m_buffer.empty())
            return V();
        auto it = m_buffer.begin();
        auto v = it->second;
        m_buffer.erase(it);
        return v;
    }

    bool wait(size_t minimumBufferSize = 0, uint64_t timeOut = 0)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return wait(lock, minimumBufferSize, timeOut);
    }

    void interrupt()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_interrupted = true;
        m_wait.notify_all();
    }

protected:
    std::condition_variable m_wait;
    mutable std::mutex m_mutex;
    std::map<K, V> m_buffer;
    bool m_interrupted;

protected:
    bool wait(std::unique_lock<std::mutex>& lock, size_t minimumBufferSize = 0, uint64_t timeOut = 0)
    {
        if (timeOut > 0)
        {
            m_wait.wait_for(
                lock,
                std::chrono::milliseconds(timeOut),
                [&](){ return m_interrupted || m_buffer.size() > minimumBufferSize; });
        }
        else
        {
            m_wait.wait(
                lock,
                [&](){ return m_interrupted || m_buffer.size() > minimumBufferSize; });
        }
        return !(interrupted() || m_buffer.size() <= minimumBufferSize);
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

template<typename T> 
class Buffer
{
public: 
    Buffer():
        m_interrupted(false)
    {
    }

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

    virtual void pushBack(const T& item)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_buffer.push_back(item);
        m_wait.notify_all();
    }

    T peekFront(bool waitForItems = false)
    {
        if (!waitForItems)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_buffer.empty() ? T() : m_buffer.front();
        }
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!wait(lock))
            return T();
        return m_buffer.front();
    }

    virtual T popFront()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!wait(lock))
            return T();
        T item = m_buffer.front();
        m_buffer.pop_front();
        return item;
    }

    bool wait(size_t minimumBufferSize = 0)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return wait(lock, minimumBufferSize);
    }

    void interrupt()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_interrupted = true;
        m_wait.notify_all();
    }

protected:
    std::condition_variable m_wait;
    mutable std::mutex m_mutex;
    std::deque<T> m_buffer;
    bool m_interrupted;

protected:
    bool wait(std::unique_lock<std::mutex>& lock, size_t minimumBufferSize = 0)
    {
        m_wait.wait(lock, [&](){ return m_interrupted || m_buffer.size() > minimumBufferSize; });
        return !interrupted();
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

////////////////////////////////////// BufferedPacketConsumer //////////////////////////////////////

class BufferedPacketConsumer 
    :
    public Map<uint64_t, std::shared_ptr<ffmpeg::Packet>>,
    public PacketConsumer
{
    typedef std::pair<uint64_t, std::shared_ptr<ffmpeg::Packet>> PairType;
public:
    BufferedPacketConsumer();

    virtual void pushBack(const uint64_t& timeStamp, const std::shared_ptr<ffmpeg::Packet>& packet) override;

    virtual void flush() override;
    virtual void givePacket(const std::shared_ptr<ffmpeg::Packet>& packet) override;

    bool waitForTimeStampDifference(uint64_t difference); 
    uint64_t timeSpan() const;

    int dropOldNonKeyPackets();
    void dropUntilFirstKeyPacket();

protected:
    bool m_ignoreNonKeyPackets;
};


/////////////////////////////// BufferredAudioVideoPacketConsumer /////////////////////////////////

class BufferedAudioVideoPacketConsumer
    :
    public AbstractVideoConsumer,
    public BufferedPacketConsumer
{
public:
    BufferedAudioVideoPacketConsumer(
        const std::weak_ptr<VideoStream>& streamReader,
        const CodecParameters& params);

    virtual void pushBack(
        const uint64_t& timeStamp, 
        const std::shared_ptr<ffmpeg::Packet>& packet) override;
    virtual std::shared_ptr<ffmpeg::Packet> popFront(uint64_t timeOut = 0) override;

    int videoCount() const;
    int audioCount() const;

private:
    int m_videoCount;
    int m_audioCount;
};


/////////////////////////////////// BufferedVideoFrameConsumer /////////////////////////////////////

class BufferedVideoFrameConsumer 
    :
    public Map<uint64_t, std::shared_ptr<ffmpeg::Frame>>,
    //public Buffer<std::shared_ptr<ffmpeg::Frame>>,
    public AbstractVideoConsumer,
    public FrameConsumer
{
public:
    BufferedVideoFrameConsumer(
        const std::weak_ptr<VideoStream>& streamReader,
        const CodecParameters& params);

    virtual void flush() override;
    virtual void giveFrame(const std::shared_ptr<ffmpeg::Frame>& frame) override;
};

} //namespace usb_cam
} //namespace nx