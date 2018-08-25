#pragma once

#include "abstract_video_consumer.h"

#include <condition_variable>
#include <mutex>
#include <deque>

namespace nx {
namespace usb_cam {

//////////////////////////////////////////// Buffer<T> /////////////////////////////////////////////

#define FLUSH() \
void flush() override \
{ \
    clear(); \
}

#define GIVE(giveObject, ObjectType, object) \
void giveObject(const std::shared_ptr<ObjectType>& object) override \
{ \
    pushBack(object); \
}

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
        m_buffer.clear();
    }

    virtual void pushBack(const T& item)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_buffer.push_back(item);
        m_wait.notify_all();
    }

    T peekFront(bool wait = false)
    {
        if (!wait)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_buffer.empty() ? T() : m_buffer.front();
        }

        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_buffer.empty())
        {
            m_wait.wait(lock, [&](){ return m_interrupted || !m_buffer.empty(); });
            if(m_interrupted)
            {
                m_interrupted = false;
                return T();
            }
        }
        return m_buffer.front();
    }

    T popFront()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_buffer.empty())
        {
            m_wait.wait(lock, [&](){ return m_interrupted || !m_buffer.empty(); });
            if(m_interrupted)
            {
                m_interrupted = false;
                return T();
            }
        }
        T item = m_buffer.front();
        m_buffer.pop_front();
        return item;
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
};

////////////////////////////////////// BufferedPacketConsumer //////////////////////////////////////

class BufferedPacketConsumer 
    :
    public Buffer<std::shared_ptr<ffmpeg::Packet>>,
    public PacketConsumer
{
public:
    BufferedPacketConsumer();

    virtual void pushBack(const std::shared_ptr<ffmpeg::Packet>& packet) override;

    FLUSH()
    GIVE(givePacket, ffmpeg::Packet, packet)

    int dropOldNonKeyPackets();
    void dropUntilFirstKeyPacket();

private:
    bool m_ignoreNonKeyPackets;
};


/////////////////////////////////// BufferredVideoPacketConsumer ///////////////////////////////////

class BufferedVideoPacketConsumer 
    :
    public BufferedPacketConsumer,
    public AbstractVideoConsumer
{
public:
    BufferedVideoPacketConsumer(
        const std::weak_ptr<VideoStream>& streamReader,
        const CodecParameters& params);

    FLUSH()
    GIVE(givePacket, ffmpeg::Packet, packet)
};


/////////////////////////////////// BufferedVideoFrameConsumer /////////////////////////////////////

class BufferedVideoFrameConsumer 
    :
    public Buffer<std::shared_ptr<ffmpeg::Frame>>,
    public AbstractVideoConsumer,
    public FrameConsumer
{
public:
    BufferedVideoFrameConsumer(
        const std::weak_ptr<VideoStream>& streamReader,
        const CodecParameters& params);

    FLUSH()
    GIVE(giveFrame, ffmpeg::Frame, frame)
};

} //namespace usb_cam
} //namespace nx