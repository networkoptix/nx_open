#ifndef QN_EVENT_LOOP_H
#define QN_EVENT_LOOP_H

#ifdef QN_HAS_PRIVATE_INCLUDES
#   include <QtCore/QThread>
#   include <QtCore/private/qthread_p.h>
#else
class QThread
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
inline bool qnHasEventLoop(QThread *thread) {
#ifdef QN_HAS_PRIVATE_INCLUDES
    int loopLevel = QThreadData::get2(thread)->loopLevel;
    return loopLevel > 0;
#else
    return true; /* Be conservative. */
#endif
}

#endif // QN_EVENT_LOOP_H
