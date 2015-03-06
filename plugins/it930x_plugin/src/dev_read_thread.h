#ifndef ITE_DEV_READ_THREAD_H
#define ITE_DEV_READ_THREAD_H

#include <atomic>

#include "dev_reader.h"

#ifdef COUNT_OBJECTS
#include "device_mapper.h"
#include "discovery_manager.h"
#include "camera_manager.h"
#include "media_encoder.h"
#include "stream_reader.h"
#include "video_packet.h"
#endif

namespace ite
{
    ///
    class DevReadThread
    {
    public:
        DevReadThread(DevReader * reader = nullptr, bool rcOnly = false)
        :   m_stopMe(false),
            m_devReader(reader),
            m_rcOnly(rcOnly)
        {
        }

        DevReadThread(DevReadThread&& obj)
        :   m_stopMe(false),
            m_devReader(obj.m_devReader),
            m_rcOnly(obj.m_rcOnly)
        {
        }

        void operator () ()
        {
            if (m_devReader)
                m_devReader->setThreadObj(this);

            m_devReader->sync();

            if (m_rcOnly)
            {
                static const unsigned SEARCH_TIME_MS = 4000;
                Timer t(true);

                while (!m_stopMe && t.elapsedMS() < SEARCH_TIME_MS)
                {
                    if (! m_devReader->readStep(true)) // blocking
                        break;
                }
            }
            else
            {
                while (!m_stopMe)
                {
                    if (! m_devReader->readStep(false)) // blocking
                        break;
                }
            }

#if 1
            printf("stop reading TS stream\n");
            printCouners();
#endif
            m_devReader->setThreadObj(nullptr);
        }

        void stop() { m_stopMe.store(true); }

    private:
        std::atomic_bool m_stopMe;
        DevReader * m_devReader;
        bool m_rcOnly;

        DevReadThread(const DevReadThread&);
        DevReadThread& operator = (const DevReadThread&);

#ifdef COUNT_OBJECTS
        void printCouners()
        {
            printf("\n");
            printCtorDtor<ContentPacket>();
            printCtorDtor<Demux>();
            printCtorDtor<DeviceBuffer>();
            printCtorDtor<DevReader>();
            printCtorDtor<DeviceMapper>();

            printCtorDtor<DiscoveryManager>();
            printCtorDtor<CameraManager>();
            printCtorDtor<MediaEncoder>();
            printCtorDtor<StreamReader>();
            printCtorDtor<VideoPacket>();
            printf("\n");
        }

        template <typename T>
        void printCtorDtor()
        {
            typedef ObjectCounter<T> Counter;

            printf("%s:\t%d - %d = %d\n", Counter::name(), Counter::ctorCount(), Counter::dtorCount(), Counter::diffCount());
        }
#else
        static void printCouners() {}
#endif
    };
}

#endif
