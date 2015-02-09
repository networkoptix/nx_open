#ifndef ITE_DEV_READ_THREAD_H
#define ITE_DEV_READ_THREAD_H

#include <atomic>

//#include "libav_wrap.h"
#include "dev_reader.h"

namespace ite
{
#if 0
    ///
    class LibAVReadThread
    {
    public:
        static const unsigned PACKETS_QUEUE_SIZE = 32;  // ~30fps * 1s
        static const unsigned MAX_PTS_DRIFT = 63000;    // 700 ms
        static const unsigned WAIT_LIBAV_FRAME_DURATION_S = 1;

        LibAVReadThread(CameraManager* camera = nullptr, LibAV* lib = nullptr)
        :
            m_stopMe(false),
            m_camera( camera ),
            m_libAV( lib )
        {
        }

        LibAVReadThread(LibAVReadThread&& obj)
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

        LibAVReadThread(const LibAVReadThread&);
        LibAVReadThread& operator = (const LibAVReadThread&);
    };
#endif

    ///
    class DevReadThread
    {
    public:
        DevReadThread(DevReader * reader = nullptr)
        :   m_stopMe(false),
            m_devReader(reader)
        {
        }

        DevReadThread(DevReadThread&& obj)
        :   m_stopMe(false),
            m_devReader(obj.m_devReader)
        {
        }

        void operator () ()
        {
            if (m_devReader)
                m_devReader->setThreadObj(this);

            if (! m_devReader->synced())
                m_devReader->sync();

            while (!m_stopMe)
            {
                if (! m_devReader->readStep()) // blocking
                    break;
            }

#if 1
            printf("stop reading TS stream\n");
#endif
            m_devReader->setThreadObj(nullptr);
        }

        void stop() { m_stopMe.store(true); }

    private:
        std::atomic_bool m_stopMe;
        DevReader * m_devReader;

        DevReadThread(const DevReadThread&);
        DevReadThread& operator = (const DevReadThread&);
    };
}

#endif
