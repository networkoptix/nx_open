// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_PERFORMANCE_H
#define QN_PERFORMANCE_H

#include <QtCore/QtGlobal>

class NX_VMS_CLIENT_CORE_API QnPerformance
{
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

    /**
     * \return                          CPU name.
     */
    static QString cpuName();

};


#endif // QN_PERFORMANCE_H
