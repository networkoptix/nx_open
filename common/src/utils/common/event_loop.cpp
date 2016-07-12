#include "event_loop.h"

#include <QtCore/QThread>
#if !defined(Q_OS_ANDROID)
    #include <QtCore/private/qthread_p.h>
#endif

bool qnHasEventLoop(QThread* thread)
{
#if !defined(Q_OS_ANDROID)
    int loopLevel = QThreadData::get2(thread)->loopLevel;
    return loopLevel > 0;
#else
    return true;
#endif
}
