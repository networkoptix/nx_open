// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <pdh.h>

#include "../activity_monitor.h"

namespace nx::monitoring {

class PdhMonitor
{
    struct HddItem
    {
        ActivityMonitor::Hdd hdd;
        PDH_RAW_COUNTER counter;
    };

    // Minimal interval between consequent re-reads of the performance counter.
    static constexpr std::chrono::milliseconds kUpdateInterval{500};

public:
    PdhMonitor();
    ~PdhMonitor();

    bool collectMonitoringData();

    // True once a fault below PDH has latched monitoring off for this process.
    static bool faulted();

    double getTotalCpuLoad();
    double getThisProcessGpuUsage();
    std::vector<ActivityMonitor::HddLoad> getTotalHddLoad();
    std::vector<ActivityMonitor::DiskIo> getTotalDiskIo();

private:
    void addTotalCpuLoadCounter();
    void addGpuTimeCounter();
    void addDiskTimeCounter();
    void readTotalCpuLoad();
    void readGpuTimeCounterValues(std::chrono::milliseconds delta);
    void readDiskCounterValues();
    std::optional<std::unordered_map<DWORD, PDH_RAW_COUNTER>> readDiskCountersByDiskId(
        PDH_HCOUNTER counter);
    double diskCounterValue(const PDH_RAW_COUNTER& last, const PDH_RAW_COUNTER& current);
    void calculateTotalHddLoad();
    std::optional<std::vector<ActivityMonitor::DiskIo>> calculateTotalDiskIo();
    // It is needed for query, containing wildcard.
    bool checkCountersExist(const std::string& query) const;

    const PdhMonitor* d_func() const;
    DWORD checkError(const char* expression, DWORD status) const;
    /** Localized performance object name in the system ANSI code page, as PDH expects it. */
    std::string perfName(DWORD index);

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
    PDH_HCOUNTER m_diskReadCounter = INVALID_HANDLE_VALUE;
    PDH_HCOUNTER m_diskWriteCounter = INVALID_HANDLE_VALUE;

    /** Data collected from the disk time counter, in a sane format. */
    std::unordered_map<DWORD, HddItem> m_itemByDiskId;

    /** Data collected from the disk time counter during the last collect operation. */
    std::unordered_map<DWORD, HddItem> m_lastItemByDiskId;

    std::unordered_map<DWORD, PDH_RAW_COUNTER> m_lastDiskReadCountersById;
    std::unordered_map<DWORD, PDH_RAW_COUNTER> m_lastDiskWriteCountersById;

    static constexpr std::chrono::minutes kMountPointsRefreshInterval{1};
    std::unordered_map<DWORD, std::vector<std::filesystem::path>> m_mountPointsByDiskId;
    std::chrono::steady_clock::time_point m_mountPointsReadAt{};

    /** Previous value for GPU running time for this process. */
    std::int64_t m_lastGpuRunningTime = 0;

    /** Final result values. */
    double m_totalCpuLoad = 0.0;
    double m_thisProcessGpuUsage = 0.0;
    std::vector<ActivityMonitor::HddLoad> m_totalHddLoad;
    std::vector<ActivityMonitor::DiskIo> m_totalDiskIo;
    bool m_initialized = false;
};

} // namespace nx::monitoring
