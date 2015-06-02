#ifndef ITE_DEV_READ_THREAD_H
#define ITE_DEV_READ_THREAD_H

#include <atomic>

#include "dev_reader.h"

namespace ite
{
    ///
    class DevReadThread
    {
    public:
        DevReadThread(DevReader * reader = nullptr, unsigned timeMS = 0)
        :   m_stopMe(false),
            m_devReader(reader),
            m_timeMS(timeMS)
        {
        }

        DevReadThread(DevReadThread&& obj)
        :   m_stopMe(false),
            m_devReader(obj.m_devReader),
            m_timeMS(obj.m_timeMS)
        {
        }

        void operator () ()
        {
            if (m_devReader)
                m_devReader->setThreadObj(this);

            m_devReader->sync();

            bool err = false;
            if (m_timeMS)
            {
                Timer t(true);

                while (!m_stopMe && t.elapsedMS() < m_timeMS)
                {
                    if (! m_devReader->readStep(true)) // blocking
                    {
                        err = true;
                        break;
                    }
                }
            }
            else
            {
                while (!m_stopMe)
                {
                    if (! m_devReader->readStep(false)) // blocking
                    {
                        err = true;
                        break;
                    }
                }
            }
#if 1
            const char * reason = "timer";
            if (err)
                reason = "error";
            else if (m_stopMe.load())
                reason = "forced";
            debug_printf("[reader] stopped: %s\n", reason);
#endif
            m_devReader->setThreadObj(nullptr);
        }

        void stop() { m_stopMe.store(true); }

    private:
        std::atomic_bool m_stopMe;
        DevReader * m_devReader;
        unsigned m_timeMS;

        DevReadThread(const DevReadThread&);
        DevReadThread& operator = (const DevReadThread&);
    };
}

#endif
