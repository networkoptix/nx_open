#ifndef QN_PERFORMANCE_H
#define QN_PERFORMANCE_H

#include <QtGlobal>

class QnPerformance {
public:
    /**
     * \returns                         CPU time (both user and kernel) consumed
     *                                  by the current thread since its start, in milliseconds.
     */
    static qint64 currentThreadTimeMSecs();

    /**
     * \returns                         CPU time (both user and kernel) consumed 
     *                                  by the current thread since its start, in nanoseconds.
     */
    static qint64 currentThreadTimeNSecs();

    /**
     * \returns                         Number of CPU cycles consumed by the current thread
     *                                  since its start.
     */
    static qint64 currentThreadCycles();

    /**
     * \returns                         Current CPU frequency. 
     * \see currentThreadCycles()
     */
    static qint64 currentCpuFrequency();

};


#endif // QN_PERFORMANCE_H
