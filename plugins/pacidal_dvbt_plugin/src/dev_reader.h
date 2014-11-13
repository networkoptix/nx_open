#ifndef ITE_DEVREADER_H
#define ITE_DEVREADER_H

#include <ctime>
#include <vector>
#include <chrono>

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
        :
            m_finished( true )
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
        DevReader(It930Stream * s, size_t size)
        :   m_stream(s),
            m_size(size),
            m_pos(0),
            m_timerStarted( false ),
            m_timerFinished( false )
        {
            readDev();
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

            int ret = readDev();
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
        It930Stream * m_stream;
        std::vector<uint8_t> m_buf;
        const size_t m_size;
        size_t m_pos;
        Timer m_timer;
        bool m_timerStarted;
        bool m_timerFinished;

        const uint8_t* data() const { return &m_buf[m_pos]; }
        int size() const { return m_buf.size() - m_pos; }

        int readDev()
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
            m_buf.resize(m_size);
            int ret = m_stream->read( &m_buf[0], m_size );
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

    //!
    class ReadThread
    {
    public:
        static const unsigned PACKETS_QUEUE_SIZE = 32;  // ~30fps * 1s
        static const unsigned MAX_PTS_DRIFT = 63000;    // 700 ms
        static const unsigned WAIT_LIBAV_FRAME_DURATION_S = 1;

        ReadThread(CameraManager* camera = nullptr, LibAV* lib = nullptr)
        :
            m_stopMe(false),
            m_camera( camera ),
            m_libAV( lib )
        {
        }

        ReadThread(ReadThread&& obj)
        :
            m_stopMe(false),
            m_camera( obj.m_camera ),
            m_libAV( obj.m_libAV )
        {
        }

        void operator () ()
        {
            static std::chrono::seconds dura( WAIT_LIBAV_FRAME_DURATION_S );

            if (m_camera)
                m_camera->setThreadObj( this );

            DevReader * devReader = m_camera->devReader();

            while (!m_stopMe)
            {
                devReader->startTimer( dura );

                unsigned streamNum;
                std::unique_ptr<VideoPacket> packet = nextDevPacket( streamNum );

                if (!devReader->timerAlive()) // break even if the packet is correct
                    break;

                if (!packet)
                    continue;

                std::shared_ptr<VideoPacketQueue> queue = m_camera->queue( streamNum );
                if (!queue)
                    continue;

                {
                    std::lock_guard<std::mutex> lock( queue->mutex() ); // LOCK

                    if (queue->size() >= PACKETS_QUEUE_SIZE)
                    {
                        queue->pop_front();
                        queue->incLost();
                    }

                    std::shared_ptr<VideoPacket> sp( packet.release() );
                    queue->push_back(sp);
                }
            }

            m_camera->setThreadObj( nullptr );
        }

        void stop() { m_stopMe.store( true ); }

    private:
        typedef int64_t PtsT;

        struct PtsTime
        {
            PtsT pts;
            time_t sec;

            PtsTime()
            :
                pts(0)
            {}
        };

        static const unsigned USEC_IN_SEC = 1000*1000;

        std::atomic_bool m_stopMe;
        CameraManager* m_camera;
        LibAV* m_libAV;
        std::vector<PtsTime> m_times;
        std::vector<PtsT> m_ptsPrev;

        std::unique_ptr<VideoPacket> nextDevPacket(unsigned& streamNum)
        {
            AVPacket avPacket;
            double timeBase = 1.0;

            {
                std::lock_guard<std::mutex> lock( m_libAV->mutex() ); // LOCK

                if (!m_libAV->isOK())
                    return std::unique_ptr<VideoPacket>();

                if (m_libAV->nextFrame( &avPacket ) < 0)
                {
                    LibAV::freePacket( &avPacket );
                    return std::unique_ptr<VideoPacket>();
                }

                if (avPacket.flags & AV_PKT_FLAG_CORRUPT)
                {
                    if ( !m_libAV->discardCorrupt() )
                        LibAV::freePacket( &avPacket );
                    return std::unique_ptr<VideoPacket>();
                }

                streamNum = avPacket.stream_index;
                timeBase = m_libAV->secTimeBase( streamNum );
            }

            std::unique_ptr<VideoPacket> packet( new VideoPacket( avPacket.data, avPacket.size ) );
            if (avPacket.flags & AV_PKT_FLAG_KEY)
                packet->setKeyFlag();

            packet->setTime( usecTime( streamNum, avPacket.pts, timeBase ) );

            LibAV::freePacket( &avPacket );
            return packet;
        }

        uint64_t usecTime(unsigned idx, PtsT pts, double timeBase)
        {
            if (m_times.size() <= idx)
            {
                m_times.resize( idx + 1 );
                m_ptsPrev.resize( idx + 1, 0 );
            }

            PtsTime& ptsTime = m_times[idx];
            const PtsT& ptsPrev = m_ptsPrev[idx];

            PtsT drift = pts - ptsPrev;
            if (drift < 0) // abs
                drift = -drift;

            if (!ptsPrev || drift > MAX_PTS_DRIFT)
            {
                ptsTime.pts = pts;
                ptsTime.sec = time(NULL);
            }

            m_ptsPrev[idx] = pts;
            return (ptsTime.sec + (pts - ptsTime.pts) * timeBase) * USEC_IN_SEC;
        }

        ReadThread(const ReadThread&);
        ReadThread& operator = (const ReadThread&);
    };
}

#endif // ITE_DEVREADER_H
