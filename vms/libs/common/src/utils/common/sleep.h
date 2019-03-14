#ifndef cl_sleep_100
#define cl_sleep_100

#include <QtCore/QThread>

class QnSleep : public QThread
{
public:

    static void msleep ( unsigned long msecs )
    {
        QThread::msleep(msecs);
    }

    static void sleep ( unsigned long secs )
    {
        QThread::sleep(secs);
    }

    static void usleep ( unsigned long usecs ) //  does not work fine with windows ? delay is always ruound to 1 ms
    {
        QThread::usleep(usecs);
    }

};

#endif //cl_sleep_100
