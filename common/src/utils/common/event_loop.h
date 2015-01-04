#ifndef QN_EVENT_LOOP_H
#define QN_EVENT_LOOP_H

#include <QtCore/QThread>
#include <QtCore/5.2.1/QtCore/private/qthread_p.h>

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
	//ax23
	return true;

    int loopLevel = QThreadData::get2(thread)->loopLevel;
    return loopLevel > 0;
}

#endif // QN_EVENT_LOOP_H
