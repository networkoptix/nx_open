// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <tuple>

#include <pdh.h>

#include <QtCore/QHash>
#include <QtCore/QList>

#include "../activity_monitor.h"

namespace nx::monitoring {

class PdhMonitor
{
    struct HddItem
    {
        HddItem() {}
        HddItem(const ActivityMonitor::Hdd& hdd, const PDH_RAW_COUNTER& counter):
            hdd(hdd), counter(counter)
        {}

        ActivityMonitor::Hdd hdd;
        PDH_RAW_COUNTER counter;
    };

    // Minimal interval between consequent re-reads of the performance counter.
    static constexpr std::chrono::milliseconds kUpdateInterval{500};

public:
    PdhMonitor();
    ~PdhMonitor();

    bool collectMonitoringData();

    qreal getTotalCpuLoad();
    qreal getThisProcessGpuUsage();
    QList<ActivityMonitor::HddLoad> getTotalHddLoad();

private:
    void addTotalCpuLoadCounter();
    void addGpuTimeCounter();
    void addDiskTimeCounter();
    void readTotalCpuLoad();
    void readGpuTimeCounterValues(std::chrono::milliseconds delta);
    void readDiskCounterValues();
    qreal diskCounterValue(const PDH_RAW_COUNTER& last, const PDH_RAW_COUNTER& current);
    void calculateTotalHddLoad();
    // It is needed for query, containing wildcard.
    bool checkCountersExist(const QString& query) const;

    std::tuple<QByteArray, int> getRawCounterArray(PDH_HCOUNTER counter);

    const PdhMonitor* d_func() const;
    DWORD checkError(const char* expression, DWORD status) const;
    QString perfName(DWORD index);

private:
    /** Handle to PHD dll. Used to query error messages via <tt>FormatMessage</tt>. */
    HMODULE m_pdhLibrary = 0;

    /** PDH query object. */
    PDH_HQUERY m_query = INVALID_HANDLE_VALUE;

    /** Time of the last collect operation. Counter is not re-read if the
     * time passed since the last collect is small. */
    std::chrono::milliseconds m_lastCpuCollectTime = std::chrono::milliseconds::zero();

    /** Cpu time counter, <tt>'\Processor(_Total)\% Processor Time'</tt>. */
    PDH_HCOUNTER m_totalCpuCounter = INVALID_HANDLE_VALUE;

    /**
     * GPU 3D engine running time counter for this process,
     * <tt>'\GPU Engine(pid_<this process pid>_*_engtype_3D)\Running Time'</tt>.
     */
    PDH_HCOUNTER m_gpuRunningTimeCounter = INVALID_HANDLE_VALUE;

    /** Disk time counter, <tt>'\PhysicalDisk(*)\% Disk Time'</tt>. */
    PDH_HCOUNTER m_diskTimeCounter = INVALID_HANDLE_VALUE;

    /** Data collected from the disk time counter, in a sane format.
     * Note that strings stored here point into the raw data buffer. */
    QHash<int, HddItem> m_itemByDiskId;

    /** Data collected from the disk time counter during the last collect operation. */
    QHash<int, HddItem> m_lastItemByDiskId;

    /** Previous value for GPU running time for this process. */
    qint64 m_lastGpuRunningTime = 0;

    /** Final result values. */
    qreal m_totalCpuLoad = 0;
    qreal m_thisProcessGpuUsage = 0;
    QList<ActivityMonitor::HddLoad> m_totalHddLoad;
    bool m_initialized = false;
};

} // namespace nx::monitoring
