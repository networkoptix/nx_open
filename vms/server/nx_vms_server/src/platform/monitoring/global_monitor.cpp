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
#include <nx/utils/thread/mutex.h>
#include <nx/utils/log/log.h>
#include <nx_vms_server_ini.h>
#include <utils/common/delete_later.h>
#include <platform/hardware_information.h>

namespace nx::vms::server {

using namespace std::chrono_literals;
const std::chrono::milliseconds GlobalMonitor::kCacheExpirationTime = 2s;

namespace {

class StubMonitor: public QnPlatformMonitor {
public:
    StubMonitor(QObject *parent = NULL): QnPlatformMonitor(parent) {}

    virtual qreal totalCpuUsage() override { return 0.0; }
    virtual quint64 totalRamUsage() override { return 0; }
    virtual quint64 thisProcessRamUsage() override { return 0; }
    virtual QList<HddLoad> totalHddLoad() override { return {}; }
    virtual QList<NetworkLoad> totalNetworkLoad() override { return {}; }
    virtual QList<PartitionSpace> totalPartitionSpaceInfo() override { return {}; }
    virtual QString partitionByPath(const QString &) override { return {}; }
};

#if defined (__linux__)
void logMallocStatistics()
{
    FILE* stream;
    char* buffer;
    size_t len;

    stream = open_memstream(&buffer, &len);
    if (stream == NULL)
        NX_INFO(typeid(GlobalMonitor), "Error with open_memstream: %1", strerror(errno));

    malloc_info(0, stream);
    fclose(stream);
    NX_INFO(typeid(GlobalMonitor), "malloc statistics: \n%1", buffer);
    free(buffer);
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

    DIR *dir = opendir(buf);
    if (dir == nullptr)
    {
        NX_INFO(typeid(GlobalMonitor), "Failed to open a directory %1: %2", buf, strerror(errno));
        return;
    }

    while ((dp = readdir(dir)) != NULL)
        fdCount++;

    closedir(dir);
    NX_INFO(typeid(GlobalMonitor), lm("Opened: %1").args(fdCount));
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

    NX_INFO(typeid(GlobalMonitor), lit("Disk files: %1").arg(typeDisk));
    NX_INFO(typeid(GlobalMonitor), lit("Sockets, pipes: %1").arg(typePipe));
    NX_INFO(typeid(GlobalMonitor), lit("Character devices: %1").arg(typeChar));
    NX_INFO(typeid(GlobalMonitor), lit("Unknown: %1").arg(typeUnknown));
}
#else
void logOpenedHandleCount()
{
    NX_WARNING(typeid(GlobalMonitor), lit("Not implemented for this platform"));
}
#endif

} // namespace

GlobalMonitor::GlobalMonitor(QnPlatformMonitor* base, QObject* parent):
    QnPlatformMonitor(parent),
    m_cachedTotalCpuUsage(
        [this]() { return m_monitorBase->totalCpuUsage(); }, &m_mutex, kCacheExpirationTime),
    m_cachedTotalRamUsage(
        [this]() { return m_monitorBase->totalRamUsage(); }, &m_mutex, kCacheExpirationTime),
    m_cachedThisProcessRamUsage(
        [this]() { return m_monitorBase->thisProcessRamUsage(); }, &m_mutex, kCacheExpirationTime),
    m_cachedTotalHddLoad(
        [this]() { return m_monitorBase->totalHddLoad(); }, &m_mutex, kCacheExpirationTime),
    m_cachedTotalNetworkLoad(
        [this]() { return m_monitorBase->totalNetworkLoad(); }, &m_mutex, kCacheExpirationTime)
{
    if (!NX_ASSERT(base != nullptr))
        base = new StubMonitor();

    if (base->thread() != thread()) {
        NX_ASSERT(false, "Cannot use a base monitor that lives in another thread.");
        qnDeleteLater(base);
        base = new StubMonitor();
    }

    m_uptimeTimer.restart();

    base->setParent(this);
    m_monitorBase = base;
}

GlobalMonitor::~GlobalMonitor() {
}

void GlobalMonitor::logStatistics()
{
    NX_INFO(this, lm("Cpu usage %1%").arg(totalCpuUsage() * 100, 0, 'f', 2));
    NX_INFO(this, lm("Memory usage %1%").arg(
        ramUsageToPercentages(totalRamUsage()) * 100, 0, 'f', 2));

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

qint64 GlobalMonitor::updatePeriodMs() const
{
    return kCacheExpirationTime.count();
}

qreal GlobalMonitor::totalCpuUsage() {
    return m_cachedTotalCpuUsage.get();
}

quint64 GlobalMonitor::totalRamUsage() {
    return m_cachedTotalRamUsage.get();
}

quint64 GlobalMonitor::thisProcessRamUsage()
{
    return m_cachedThisProcessRamUsage.get();
}

QList<QnPlatformMonitor::HddLoad> GlobalMonitor::totalHddLoad() {
    return m_cachedTotalHddLoad.get();
}

QList<QnPlatformMonitor::NetworkLoad> GlobalMonitor::totalNetworkLoad() {
    return m_cachedTotalNetworkLoad.get();
}

QList<QnPlatformMonitor::PartitionSpace> GlobalMonitor::totalPartitionSpaceInfo()
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_monitorBase->totalPartitionSpaceInfo();
}

QString GlobalMonitor::partitionByPath(const QString &path) {
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_monitorBase->partitionByPath(path);
}

std::chrono::milliseconds GlobalMonitor::processUptime() const
{
    return m_uptimeTimer.elapsed();
}

void GlobalMonitor::setServerModule(QnMediaServerModule* serverModule)
{
    m_monitorBase->setServerModule(serverModule);
}

qreal ramUsageToPercentages(quint64 bytes)
{
    return bytes / qreal(HardwareInformation::instance().physicalMemory);
}

} // namespace nx::vms::server
