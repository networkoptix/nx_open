#include "event_loop.h"

#include <QtCore/QThread>
#if !defined(Q_OS_ANDROID)
    #include <QtGui/QGuiApplication>
    #include <QtCore/private/qthread_p.h>
#endif

bool qnHasEventLoop(QThread* thread)
{
#if !defined(Q_OS_ANDROID)
    int loopLevel = QThreadData::get2(thread)->loopLevel;
    if (loopLevel > 0)
        return true;

    // Main thread of the GUI application will have event loop for sure.
    auto appInstance = QCoreApplication::instance();
    if (appInstance && thread == appInstance->thread())
        return dynamic_cast<QGuiApplication*>(appInstance);

    return false;
#else
    return true;
#endif
}
