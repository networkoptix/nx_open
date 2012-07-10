#ifndef QN_PERFORMANCE_H
#define QN_PERFORMANCE_H

#include <QtGlobal>

class QnPerformance {
public:
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

    static qint64 currentCpuFrequency();

    /**
     * \returns                         Percent of CPU time (both user and kernel) consumed 
     *                                  by the current process in last few seconds.
     */
    static qint64 currentCpuUsage();

    /**
    * \returns                          Brandstring includes manufacturer, model and clockspeed
    */
    static QString cpuBrand();

    /**
    * \returns                          Number of CPU cores
    */
    static int cpuCoreCount();
};


#endif // QN_PERFORMANCE_H
