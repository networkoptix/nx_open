#ifndef ITE_DEVREADER_H
#define ITE_DEVREADER_H

#include <ctime>
#include <vector>
#include <chrono>
#include <mutex>

#include "it930x.h"
#include "mpeg_ts_packet.h"

#if 0
#include <ratio>

namespace
{
    struct ClockRDTSC
    {
        typedef unsigned long long                  rep;
        typedef std::ratio<1, 3300000000>           period; // CPU 3.3 GHz
        typedef std::chrono::duration<rep, period>  duration;
        typedef std::chrono::time_point<ClockRDTSC> time_point;
        static const bool is_steady =               true;

        static time_point now() noexcept
        {
            unsigned lo, hi;
            asm volatile("rdtsc" : "=a" (lo), "=d" (hi));
            return time_point(duration(static_cast<rep>(hi) << 32 | lo));
        }
    };
}
#endif

namespace ite
{
    //!
    class Timer
    {
    public:
        typedef std::chrono::steady_clock Clock;
        //typedef ClockRDTSC Clock;

        Timer()
        :   m_finished(false)
        {
        }

        void reset(const std::chrono::milliseconds& duration)
        {
            m_stopAt = Clock::now() + duration;
            m_finished = false;
        }

        bool timeIsUp()
        {
            if (m_finished)
                return true;

            if (Clock::now() > m_stopAt)
            {
                m_finished = true;
                return true;
            }
            return false;
        }

    private:
        std::chrono::time_point<Clock> m_stopAt;
        bool m_finished;
    };

    //!
    class DevReader
    {
    public:
        static const unsigned MIN_PACKETS_COUNT = 5;

        DevReader(It930x * dev = nullptr, size_t size = It930x::DEFAULT_PACKETS_NUM * MpegTsPacket::PACKET_SIZE)
        :   m_device(dev),
            m_size(size),
            m_pos(0),
            m_timerStarted( false ),
            m_timerFinished( false )
        {
            if (m_size < MIN_PACKETS_COUNT * MpegTsPacket::PACKET_SIZE)
                m_size = MIN_PACKETS_COUNT * MpegTsPacket::PACKET_SIZE;
        }

        bool hasDevice() const
        {
            std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            return m_device == nullptr;
        }

        void setDevice(It930x * dev)
        {
            {
                std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

                if (m_device != dev)
                    m_device = dev;
            }

            clear();
            if (dev)
                readDevice();
        }

        int read(uint8_t* buf, int bufSize)
        {
            if (bufSize <= size())
            {
                memcpy(buf, data(), bufSize);
                m_pos += bufSize;
                return bufSize;
            }

            int hasRead = size();
            memcpy(buf, data(), size());
            buf += hasRead;
            bufSize -= hasRead;

            int ret = readDevice();
            if (ret < 0)
                return ret;
            if (ret == 0)
                return hasRead;

            return hasRead + read(buf, bufSize);
        }

        void clear()
        {
            m_buf.clear();
            m_pos = 0;
            m_timerStarted = m_timerFinished = false;
        }

        bool synced() const
        {
            if (m_buf.size() < MIN_PACKETS_COUNT * MpegTsPacket::PACKET_SIZE)
                return false;

            uint8_t shift = m_pos % MpegTsPacket::PACKET_SIZE;
            if (MpegTsPacket(&m_buf[shift] + MpegTsPacket::PACKET_SIZE * 0).syncOK() &&
                MpegTsPacket(&m_buf[shift] + MpegTsPacket::PACKET_SIZE * 1).syncOK() &&
                MpegTsPacket(&m_buf[shift] + MpegTsPacket::PACKET_SIZE * 2).syncOK() &&
                MpegTsPacket(&m_buf[shift] + MpegTsPacket::PACKET_SIZE * 3).syncOK())
                return true;

            return false;
        }

        void startTimer(const std::chrono::milliseconds& duration)
        {
            m_timer.reset( duration );
            m_timerStarted = true;
            m_timerFinished = false;
        }

        void stopTimer()
        {
            m_timerStarted = false;
            m_timerFinished = false;
        }

        bool timerAlive() const { return m_timerStarted && !m_timerFinished; }

    private:
        mutable std::mutex m_mutex;
        It930x * m_device;
        std::vector<uint8_t> m_buf;
        size_t m_size;
        size_t m_pos;
        Timer m_timer;
        bool m_timerStarted;
        bool m_timerFinished;

        const uint8_t* data() const { return &m_buf[m_pos]; }
        int size() const { return m_buf.size() - m_pos; }

        int readDevice()
        {
#if 1       // HACK: return from libAV read cycle
            if (m_timerFinished)
                return -1;
            if (m_timerStarted && m_timer.timeIsUp())
            {
                m_timerStarted = false;
                m_timerFinished = true;
                return -1;
            }
#endif
            std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            if (! m_device || ! m_device->hasStream())
                return -1;

            m_buf.resize(m_size);
            int ret = m_device->read( &(*m_buf.begin()), m_buf.size() );
            if (ret < 0) {
                m_buf.clear();
                m_pos = 0;
                return ret;
            }

            if (size_t(ret) != m_size)
                m_buf.resize(ret);
            m_pos = 0;
            return ret;
        }
    };
}

#endif // ITE_DEVREADER_H
