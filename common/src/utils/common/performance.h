#ifndef QN_PERFORMANCE_H
#define QN_PERFORMANCE_H

#include <QtGlobal>

class QnPerformance {
public:
	struct CpuInfo{
		QString Model;
		uint Cores;
		QString Clock;
	};

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

	static qint64 currentCpuUsage();

	static CpuInfo getCpuInfo();
};


#endif // QN_PERFORMANCE_H
