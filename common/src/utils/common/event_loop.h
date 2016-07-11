#pragma once

#include <QtCore/QThread>
#if !defined(Q_OS_ANDROID)
    #include <QtCore/private/qthread_p.h>
#endif

/**
 * \param thread                        Thread to check.
 * \returns                             Whether the given thread has a running event loop.
 *
 * \note                                This function always returns true if Qt
 *                                      private headers were not available during
 *                                      compilation. Therefore it should be used for
 *                                      error checking only.
 */
inline bool qnHasEventLoop(QThread* thread)
{
#if !defined(Q_OS_ANDROID)
    int loopLevel = QThreadData::get2(thread)->loopLevel;
    return loopLevel > 0;
#else
    return true;
#endif
}
