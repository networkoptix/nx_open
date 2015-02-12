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
