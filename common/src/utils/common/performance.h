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
     * \returns                         Percent of CPU time (both user and kernel) consumed 
     *                                  by all running processes in last few seconds.
     */
    static qint64 currentCpuTotalUsage();

    /**
    * \returns                          Brandstring includes manufacturer, model and clockspeed
    */
    static QString cpuBrand();

    /**
    * \returns                          Number of CPU cores
    */
    static int cpuCoreCount();

    /**
     * \returns                         Percentage of elapsed time that the fixed disk drives
     *                                  are busy servicing read or write requests.
     * \param hddUsage                  Output list of hdd usage data.
     */
    static bool currentHddUsage(QList<int> *output);
};


#endif // QN_PERFORMANCE_H
