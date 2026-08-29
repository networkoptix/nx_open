// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "monitor_linux.h"

#include <optional>
#include <thread>

#include <unistd.h>

#include <QtCore/QDir>

#include <nx/utils/log/log.h>
#include <nx/utils/system_error.h>

#include "private/monitor_p_linux.h"

namespace nx::monitoring {

static const size_t MAX_LINE_LENGTH = 512;

namespace {

struct MemoryStats
{
    std::uint64_t residentSize;
    std::uint64_t sharedSize;
};

MemoryStats getMemoryStats()
{
    static constexpr auto kStatmFilename = "/proc/self/statm";
    std::ifstream statm(kStatmFilename);
    if (!statm.is_open())
    {
        NX_WARNING(NX_SCOPE_TAG,
            "Failed to open file %1: %2", kStatmFilename, SystemError::toString(errno));
        return {0, 0};
    }

    statm.ignore(255, ' '); //< Skip first value (size).

    MemoryStats result;

    // Sizes are reported in pages!
    statm >> result.residentSize;
    statm >> result.sharedSize;

    static const std::uint64_t pageSize = sysconf(_SC_PAGE_SIZE);

    // Convert sizes to bytes.
    result.residentSize *= pageSize;
    result.sharedSize *= pageSize;

    return result;
}

} // namespace

LinuxMonitor::LinuxMonitor():
    base_type(),
    d(new Private())
{
}

LinuxMonitor::~LinuxMonitor()
{
}

double LinuxMonitor::totalCpuUsage()
{
    std::unique_ptr<FILE, decltype(&fclose)> file( fopen("/proc/stat", "r"), fclose );
    if(!file)
        return 0;

    int64_t cpuTimeTotal = 0;
    int64_t cpuTimeIdle = d->prevCPUTimeIdle;
    char line[MAX_LINE_LENGTH];
    while (fgets(line, MAX_LINE_LENGTH, file.get()) != nullptr)
    {
        QByteArray lineStr( line );
        const QList<QByteArray>& tokens = lineStr.split( ' ' );
        if( tokens.isEmpty() )
            continue;
        if( tokens[0] != "cpu" )
            continue;
        //skipping empty tokens due to multiple spaces
        int firstNonEmptyTokenIndex = 1;
        for( ; firstNonEmptyTokenIndex < tokens.size(); ++firstNonEmptyTokenIndex )
            if( !tokens[firstNonEmptyTokenIndex].isEmpty() )
                break;
        //calculating total CPU time
        for( int i = firstNonEmptyTokenIndex; i < tokens.size(); ++i )
            cpuTimeTotal += tokens[i].toLongLong();

        static const int IDLE_FIELD_POS = 3;
        if( tokens.size() < firstNonEmptyTokenIndex+IDLE_FIELD_POS+1 )
            break;
        cpuTimeIdle = tokens[firstNonEmptyTokenIndex+IDLE_FIELD_POS].toLongLong();

    }

    file.reset();

    if( d->prevCPUTimeTotal == -1 )
        d->prevCPUTimeTotal = cpuTimeTotal;
    if( d->prevCPUTimeIdle == -1 )
        d->prevCPUTimeIdle = cpuTimeIdle;

    //calculating CPU load
    const int64_t cpuTimeTotalDiff = cpuTimeTotal - d->prevCPUTimeTotal;
    const int64_t cpuTimeIdleDiff = cpuTimeIdle - d->prevCPUTimeIdle;
    d->prevCPUTimeIdle = cpuTimeIdle;
    d->prevCPUTimeTotal = cpuTimeTotal;
    if( cpuTimeTotalDiff <= 0 )
        return 0;
    return 1.0 - cpuTimeIdleDiff / static_cast<double>(cpuTimeTotalDiff);
}

std::uint64_t LinuxMonitor::totalRamUsageBytes()
{
    std::unique_ptr<FILE, decltype(&fclose)> file(fopen("/proc/meminfo", "r"), fclose);
    if (!file)
        return 0;

    char line[MAX_LINE_LENGTH];
    std::optional<uint64_t> memTotalKB;
    std::optional<uint64_t> memFreeKB;
    std::optional<uint64_t> memCachedKB;
    while (fgets(line, MAX_LINE_LENGTH, file.get()))
    {
        const size_t length = strlen(line);

        char* sepPos = strchr(line, ':');
        if (sepPos == nullptr)
            continue;
        char* valStart = std::find_if(sepPos + 1, line + length, [](char ch){ return ch != ' '; });
        if (valStart == line + length)
           continue;    //could not find value
        char* valEnd = strchr( valStart, ' ' );
        if (valEnd)
            *valEnd = '\0';
        if (strncmp(line, "MemTotal", static_cast<size_t>(sepPos - line)) == 0)
            memTotalKB = atoll(valStart);
        else if (strncmp(line, "MemFree", static_cast<size_t>(sepPos - line)) == 0)
            memFreeKB = atoll(valStart);
        else if (strncmp(line, "Cached", static_cast<size_t>(sepPos - line)) == 0)
            memCachedKB = atoll(valStart);

        if (memTotalKB && memFreeKB && memCachedKB)
            break;
    }

    if (!memTotalKB || !memFreeKB || !memCachedKB)
        return 0;

    // Protecting from zero-memory-size error in /proc/meminfo. This situation is not possible in
    // real life, so nothing to worry about.
    if (*memTotalKB == 0)
        return 0;

    return (*memTotalKB - *memFreeKB - *memCachedKB) * 1024;
}

std::uint64_t LinuxMonitor::thisProcessRamUsageBytes()
{
    const MemoryStats stats = getMemoryStats();
    return stats.residentSize;
}

std::uint64_t LinuxMonitor::thisProcessPrivateRamUsageBytes()
{
    const MemoryStats stats = getMemoryStats();
    return stats.residentSize - stats.sharedSize;
}

double LinuxMonitor::thisProcessCpuUsage()
{
    // Normalising for multicore processors.
    return d->thisProcessCpuUsage() / static_cast<double>(std::thread::hardware_concurrency());
}

std::vector<ActivityMonitor::HddLoad> LinuxMonitor::totalHddLoad()
{
    return d->totalHddLoad();
}

std::vector<ActivityMonitor::NetworkLoad> LinuxMonitor::totalNetworkLoad()
{
    return d->totalNetworkLoad();
}

std::vector<ActivityMonitor::PartitionSpace> LinuxMonitor::totalPartitionSpaceInfo()
{
    if (d->partitionsInfoProvider)
        return d->partitionsInfoProvider->partitionInfo();

    return {};
}

void LinuxMonitor::setPartitionInformationProvider(
    std::unique_ptr<PartitionsInformationProvider> partitionInformationProvider)
{
    d->partitionsInfoProvider = std::move(partitionInformationProvider);
}

int LinuxMonitor::thisProcessThreads()
{
    const QString path("/proc/self/task");
    return QDir(path).entryList(QDir::Dirs | QDir::NoDotAndDotDot).count();
}


} // namespace nx::monitoring
