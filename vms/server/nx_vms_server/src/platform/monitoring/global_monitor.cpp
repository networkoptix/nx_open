#include "global_monitor.h"

#ifdef __linux__
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/resource.h>
#endif

#include <iostream>
#include <nx/utils/log/log.h>
#include <nx/utils/type_utils.h>
#include <nx/utils/scope_guard.h>
#include <nx_vms_server_ini.h>
#include <utils/common/delete_later.h>
#include <platform/hardware_information.h>

namespace nx::vms::server {

using namespace std::chrono_literals;
const std::chrono::milliseconds GlobalMonitor::kCacheExpirationTime = 2s;

namespace {

#if defined (__linux__)
    void logMallocStatistics()
    {
        char* buffer = nullptr;
        const auto bufferGuard = nx::utils::makeScopeGuard([buffer] { if (buffer) free(buffer); });
        size_t len;

        auto stream = nx::utils::wrapUnique(open_memstream(&buffer, &len), &fclose);
        if (stream == nullptr)
        {
            NX_WARNING(typeid(GlobalMonitor), "Error with open_memstream: %1", strerror(errno));
            return;
        }

        malloc_info(0, stream.get());
        NX_INFO(typeid(GlobalMonitor), "malloc statistics: \n%1", buffer);
    }
#else
    void logMallocStatistics()
    {
        // Not implemented
        return;
    }
#endif

// TODO: Should be implemented like other statistics
#if defined (__linux__)
    void logOpenedHandleCount()
    {
        int fdCount = 0;
        char buf[64];
        struct dirent *dp;

        snprintf(buf, 64, "/proc/%i/fd/", getpid());

        auto dir = nx::utils::wrapUnique(opendir(buf), &closedir);
        if (dir == nullptr)
        {
            NX_WARNING(typeid(GlobalMonitor), "Failed to open a directory %1: %2",
                buf, strerror(errno));
            return;
        }

        while ((dp = readdir(dir.get())) != NULL)
            fdCount++;

        NX_INFO(typeid(GlobalMonitor), "Opened FDs: %1", fdCount);
    }
#elif defined (Q_OS_WIN)
    void logOpenedHandleCount()
    {
        DWORD typeChar = 0;
        DWORD typeDisk = 0;
        DWORD typePipe = 0;
        DWORD typeRemote = 0;
        DWORD typeUnknown = 0;
        DWORD handlesCount = 0;

        GetProcessHandleCount(GetCurrentProcess(), &handlesCount);
        handlesCount *= 4;
        for (std::ptrdiff_t handle = 0x4; handle < handlesCount; handle += 4)
        {
            switch (GetFileType((HANDLE)handle))
            {
                case FILE_TYPE_CHAR:
                    typeChar++;
                    break;
                case FILE_TYPE_DISK:
                    typeDisk++;
                    break;
                case FILE_TYPE_PIPE:
                    typePipe++;
                    break;
                case FILE_TYPE_REMOTE:
                    typeRemote++;
                    break;
                case FILE_TYPE_UNKNOWN:
                    if (GetLastError() == NO_ERROR) typeUnknown++;
                    break;
            }
        }

        NX_INFO(typeid(GlobalMonitor), "Disk files: %1", typeDisk);
        NX_INFO(typeid(GlobalMonitor), "Sockets, pipes: %1", typePipe);
        NX_INFO(typeid(GlobalMonitor), "Character devices: %1", typeChar);
        NX_INFO(typeid(GlobalMonitor), "Unknown: %1", typeUnknown);
    }
#else
    void logOpenedHandleCount()
    {
        NX_WARNING(typeid(GlobalMonitor), "Not implemented for this platform");
    }
#endif

} // namespace

GlobalMonitor::GlobalMonitor(
    std::unique_ptr<nx::vms::server::PlatformMonitor> base,
    nx::utils::TimerManager* timerManager)
    :
    nx::vms::server::PlatformMonitor(),
    m_cachedTotalCpuUsage(
        [this]() { return m_monitorBase->totalCpuUsage(); }),
    m_cachedTotalRamUsage(
        [this]() { return m_monitorBase->totalRamUsageBytes(); }, kCacheExpirationTime),
    m_cachedThisProcessCpuUsage(
        [this]() { return m_monitorBase->thisProcessCpuUsage(); }),
    m_cachedThisProcessRamUsage(
        [this]() { return m_monitorBase->thisProcessRamUsageBytes(); }, kCacheExpirationTime),
    m_cachedTotalHddLoad(
        [this]() { return m_monitorBase->totalHddLoad(); }),
    m_cachedTotalNetworkLoad(
        [this]() { return m_monitorBase->totalNetworkLoad(); }),
    m_cachedTotalPartitionSpaceInfo(
        [this]() { return m_monitorBase->totalPartitionSpaceInfo(); }, kCacheExpirationTime)
{
    NX_ASSERT(base);
    NX_ASSERT(base->thread() == thread(), "Cannot use a base monitor that lives in another thread.");
    NX_CRITICAL(timerManager);

    m_uptimeTimer.restart();

    // NOTE: We should update some of the cached values with fixed period because their
    // implementation performs calculations which depends on time passed between the calls.
    const auto timerId = timerManager->addNonStopTimer(
        [this](nx::utils::TimerId)
        {
            NX_VERBOSE(this, "ZORZ");
            m_cachedTotalCpuUsage.update();
            m_cachedThisProcessCpuUsage.update();
            m_cachedTotalHddLoad.update();
            m_cachedTotalNetworkLoad.update();
        },
        kCacheExpirationTime,
        /*firstShotDelay*/ 0ms);
    m_timerGuard = {timerManager, timerId};

    m_monitorBase = std::move(base);
}

GlobalMonitor::~GlobalMonitor()
{
}

void GlobalMonitor::logStatistics()
{
    NX_INFO(this, lm("OS CPU usage %1%").arg(totalCpuUsage() * 100, 0, 'f', 2));
    NX_INFO(this, lm("Process CPU usage %1%").arg(thisProcessCpuUsage() * 100, 0, 'f', 2));
    NX_INFO(this, lm("OS memory usage %1%").arg(
        ramUsageToPercentages(totalRamUsageBytes()) * 100, 0, 'f', 2));
    NX_INFO(this, lm("Process memory usage %1%").arg(
        ramUsageToPercentages(thisProcessRamUsageBytes()) * 100, 0, 'f', 2));

    NX_INFO(this, "HDD usage:");
    for (const HddLoad& hddLoad: totalHddLoad())
        NX_INFO(this, lm("\t%1: %2%").arg(hddLoad.hdd.name).arg(hddLoad.load * 100, 0, 'f', 2));

    NX_INFO(this, "File handles:");
    logOpenedHandleCount();

    NX_INFO(this, "Network usage:");
    for (const NetworkLoad& networkLoad: totalNetworkLoad())
    {
        NX_INFO(this, "\t%1. in %2KBps, out %3KBps",
            networkLoad.interfaceName,
            networkLoad.bytesPerSecIn / 1024,
            networkLoad.bytesPerSecOut / 1024);
    }

    if (ini().enableMallocStatisticsLogging)
        logMallocStatistics();

}

std::chrono::milliseconds GlobalMonitor::updatePeriod() const
{
    return kCacheExpirationTime;
}

qreal GlobalMonitor::totalCpuUsage() {
    return m_cachedTotalCpuUsage.get();
}

quint64 GlobalMonitor::totalRamUsageBytes() {
    return m_cachedTotalRamUsage.get();
}

qreal GlobalMonitor::thisProcessCpuUsage()
{
    return m_cachedThisProcessCpuUsage.get();
}

quint64 GlobalMonitor::thisProcessRamUsageBytes()
{
    return m_cachedThisProcessRamUsage.get();
}

QList<nx::vms::server::PlatformMonitor::HddLoad> GlobalMonitor::totalHddLoad() {
    return m_cachedTotalHddLoad.get();
}

QList<nx::vms::server::PlatformMonitor::NetworkLoad> GlobalMonitor::totalNetworkLoad() {
    return m_cachedTotalNetworkLoad.get();
}

QList<nx::vms::server::PlatformMonitor::PartitionSpace> GlobalMonitor::totalPartitionSpaceInfo()
{
    return m_cachedTotalPartitionSpaceInfo.get();
}

int GlobalMonitor::thisProcessThreads()
{
    return m_monitorBase->thisProcessThreads();
}

void GlobalMonitor::setRootFileSystem(nx::vms::server::RootFileSystem* rootFs)
{
    return m_monitorBase->setRootFileSystem(rootFs);
}

std::chrono::milliseconds GlobalMonitor::processUptime() const
{
    return m_uptimeTimer.elapsed();
}

qreal ramUsageToPercentages(quint64 bytes)
{
    return bytes / qreal(HardwareInformation::instance().physicalMemory);
}

} // namespace nx::vms::server
