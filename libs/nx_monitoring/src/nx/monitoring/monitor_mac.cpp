// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "monitor_mac.h"

#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <sys/resource.h>
#include <net/if.h>
#include <net/route.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOBlockStorageDriver.h>
#include <IOKit/IOBSD.h>

#include <map>
#include <vector>
#include <chrono>
#include <thread>

#include <QtNetwork/QNetworkInterface>

#include <nx/utils/elapsed_timer.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>

namespace nx::monitoring {

using namespace std::chrono;

namespace {

constexpr int kDefaultInterfaceSpeedMbps = 1000;

static QString dictionaryGetString(CFDictionaryRef properties, CFStringRef name)
{
    if (const auto value = static_cast<CFStringRef>(CFDictionaryGetValue(properties, name)))
        return QString::fromCFString(value);

    return {};
}

static int64_t dictionaryGetInt(CFDictionaryRef properties, CFStringRef name)
{
    int64_t result = 0;

    if (const auto number = static_cast<CFNumberRef>(CFDictionaryGetValue(properties, name)))
        CFNumberGetValue(number, kCFNumberSInt64Type, &result);

    return result;
}

/** Recursively traverses the dictionary and accumulates the specified property value. */
class PropertyAccum
{
public:
    PropertyAccum(CFDictionaryRef properties, CFStringRef name): m_name(name)
    {
        traverseDict(properties);
    }

    int64_t value() const { return m_accum; }

private:
    void traverseValue(CFTypeRef value)
    {
        const auto typeID = CFGetTypeID(value);

        if (typeID == CFDictionaryGetTypeID())
            traverseDict(static_cast<CFDictionaryRef>(value));
        else if (typeID == CFArrayGetTypeID())
            traverseArray(static_cast<CFArrayRef>(value));
    }

    void traverseArray(CFArrayRef array)
    {
        const CFIndex count = CFArrayGetCount(array);

        for (CFIndex i = 0; i < count; ++i)
            traverseValue(CFArrayGetValueAtIndex(array, i));
    }

    void traverseDict(CFDictionaryRef properties)
    {
        const CFIndex count = CFDictionaryGetCount(properties);

        std::vector<CFTypeRef> keys(count, nullptr);
        std::vector<CFTypeRef> values(count, nullptr);

        CFDictionaryGetKeysAndValues(
            properties,
            (const void **) keys.data(),
            (const void **) values.data());

        for (CFIndex i = 0; i < count; ++i)
        {
            if (CFGetTypeID(keys[i]) != CFStringGetTypeID())
                continue;

            CFStringRef key = static_cast<CFStringRef>(keys[i]);
            const auto& value = values[i];

            if (CFGetTypeID(value) != CFNumberGetTypeID()
                || CFStringCompare(key, m_name, 0) != kCFCompareEqualTo)
            {
                traverseValue(value);
                continue;
            }

            // Found a property with the matching name, add its value to the accumulator.
            const auto number = static_cast<CFNumberRef>(value);
            int64_t numberValue = 0;

            if (CFNumberGetValue(number, kCFNumberSInt64Type, &numberValue))
                m_accum += numberValue;
        }
    }

private:
    int64_t m_accum = 0;
    CFStringRef m_name = nullptr;
};

struct CFMutableDictionaryRefDeleter
{
    using pointer = CFMutableDictionaryRef;
    void operator()(CFMutableDictionaryRef ref)
    {
        CFRelease(ref);
    }
};

using PropertiesPtr = std::unique_ptr<CFMutableDictionaryRef, CFMutableDictionaryRefDeleter>;

static PropertiesPtr getIOProperties(io_registry_entry_t entry)
{
    CFMutableDictionaryRef properties;
    const auto ret = IORegistryEntryCreateCFProperties(entry, &properties, kCFAllocatorDefault, 0);

    if (ret != KERN_SUCCESS)
    {
        NX_WARNING(NX_SCOPE_TAG, "Failed IORegistryEntryCreateCFProperties : %1", ret);
        return {};
    }

    return PropertiesPtr(properties);
}

} // namespace

class MacMonitor::NetworkLoadMonitor
{
public:
    NetworkLoadMonitor()
    {
        calcNetworkStat(milliseconds::zero());
        m_networkStatCalcTimer.restart();
    }

    virtual ~NetworkLoadMonitor()
    {
    }

    QList<ActivityMonitor::NetworkLoad> totalNetworkLoad();

private:
    class InterfaceStatisticsContext: public ActivityMonitor::NetworkLoad
    {
    public:
        uint64_t bytesReceived = 0;
        uint64_t bytesSent = 0;
    };

    void updateNetworkInterfaceStat(
        int index, milliseconds elapsed, uint64_t totalBytesReceived, uint64_t totalBytesSent);
    void updateNetworkStat();
    void calcNetworkStat(milliseconds elapsed);

    nx::Mutex m_networkLoadMutex;
    nx::utils::ElapsedTimer m_networkStatCalcTimer;
    std::map<QString, InterfaceStatisticsContext> m_ifNameToStatistics;
};

void MacMonitor::NetworkLoadMonitor::updateNetworkInterfaceStat(
    int index, milliseconds elapsed, uint64_t totalBytesReceived, uint64_t totalBytesSent)
{
    auto interface = QNetworkInterface::interfaceFromIndex(index);

    if (!interface.isValid())
    {
        NX_WARNING(this, "Unable to get network interface at index %1", index);
        return;
    }

    InterfaceStatisticsContext& ctx = m_ifNameToStatistics[interface.name()];

    if (ctx.interfaceName.isEmpty())
    {
        ctx.interfaceName = interface.name();
        ctx.macAddress = nx::utils::MacAddress(interface.hardwareAddress());
        switch (interface.type())
        {
            case QNetworkInterface::Loopback:
                ctx.type = ActivityMonitor::LoopbackInterface;
                break;

            case QNetworkInterface::Virtual:
                ctx.type = ActivityMonitor::VirtualInterface;
                break;

            default:
                ctx.type = ActivityMonitor::PhysicalInterface;
        }
    }

    static const double kNewValueWeight = 0.7;
    static const auto kMsPerSec = duration_cast<milliseconds>(1s).count();

    ctx.bytesPerSecMax = kDefaultInterfaceSpeedMbps * 1024 * 1024 / CHAR_BIT; //< unknown, assuming 1Gbps.

    if (ctx.bytesReceived > 0 && elapsed != milliseconds::zero())
    {
        const auto value = (totalBytesReceived - ctx.bytesReceived) * kMsPerSec / elapsed.count();
        ctx.bytesPerSecIn = ctx.bytesPerSecIn * (1 - kNewValueWeight) + value * kNewValueWeight;
    }
    ctx.bytesReceived = totalBytesReceived;

    if (ctx.bytesSent > 0 && elapsed != milliseconds::zero())
    {
        const auto value = (totalBytesSent - ctx.bytesSent) * kMsPerSec / elapsed.count();
        ctx.bytesPerSecOut = ctx.bytesPerSecOut * (1 - kNewValueWeight) + value * kNewValueWeight;
    }
    ctx.bytesSent = totalBytesSent;
}

void MacMonitor::NetworkLoadMonitor::calcNetworkStat(milliseconds elapsed)
{
    int mib[] = {
        CTL_NET,        //< networking subsystem
        PF_ROUTE,       //< type of information
        0,              //< protocol (IPPROTO_xxx)
        0,              //< address family
        NET_RT_IFLIST2, //< operation
        0
    };
    size_t len = 0;
    if (sysctl(mib, 6, NULL, &len, NULL, 0) == -1)
    {
        NX_WARNING(this, "Unable to get network interfaces count: %1", SystemError::toString(errno));
        return;
    }
    std::vector<char> buffer;
    buffer.resize(len);

    char* buf = buffer.data();
    if (sysctl(mib, 6, buf, &len, NULL, 0) == -1)
    {
        NX_WARNING(this, "Unable to get network interfaces: %1", SystemError::toString(errno));
        return;
    }

    for (auto ifm = reinterpret_cast<const if_msghdr*>(buf);
        reinterpret_cast<const char*>(ifm) < buf + len;
        reinterpret_cast<const char*&>(ifm) += ifm->ifm_msglen)
    {
        if (ifm->ifm_type != RTM_IFINFO2)
            continue;

        auto if2m = reinterpret_cast<const struct if_msghdr2*>(ifm);

        updateNetworkInterfaceStat(
            ifm->ifm_index, elapsed, if2m->ifm_data.ifi_ibytes, if2m->ifm_data.ifi_obytes);
    }
}

void MacMonitor::NetworkLoadMonitor::updateNetworkStat()
{
    const auto elapsed = m_networkStatCalcTimer.isValid()
        ? m_networkStatCalcTimer.elapsed()
        : milliseconds::zero();

    if (elapsed != milliseconds::zero())
        calcNetworkStat(elapsed);

    m_networkStatCalcTimer.restart();
}

QList<ActivityMonitor::NetworkLoad> MacMonitor::NetworkLoadMonitor::totalNetworkLoad()
{
    NX_MUTEX_LOCKER lock(&m_networkLoadMutex);

    updateNetworkStat();

    QList<ActivityMonitor::NetworkLoad> netStat;
    for (auto it = m_ifNameToStatistics.begin(); it != m_ifNameToStatistics.end(); ++it)
        netStat.push_back(it->second);

    return netStat;
}


class MacMonitor::CpuLoadMonitor
{
public:
    CpuLoadMonitor()
    {
        (void) getTotalCpuLoad();
        (void) getSelfCpuLoad();
    }

    virtual ~CpuLoadMonitor() {}

    qreal getTotalCpuLoad();
    qreal getSelfCpuLoad();

private:
    qreal calculateTotalCpuLoad(unsigned long long idleTicks, unsigned long long totalTicks);

    nx::Mutex m_cpuLoadMutex;

    unsigned long long m_prevTotalTicks = 0;
    unsigned long long m_prevIdleTicks = 0;

    microseconds m_prevSelfUsageTime{};
    time_point<steady_clock> m_timeStamp{};
};

qreal MacMonitor::CpuLoadMonitor::calculateTotalCpuLoad(
    unsigned long long idleTicks, unsigned long long totalTicks)
{
    unsigned long long totalTicksSinceLastTime = totalTicks - m_prevTotalTicks;
    unsigned long long idleTicksSinceLastTime = idleTicks - m_prevIdleTicks;
    m_prevTotalTicks = totalTicks;
    m_prevIdleTicks = idleTicks;

    qreal idleRate = 0;
    if (totalTicksSinceLastTime > 0)
        idleRate = static_cast<qreal>(idleTicksSinceLastTime) / totalTicksSinceLastTime;

    return 1.0 - idleRate;
}

qreal MacMonitor::CpuLoadMonitor::getTotalCpuLoad()
{
    NX_MUTEX_LOCKER lock(&m_cpuLoadMutex);

    host_cpu_load_info_data_t cpuinfo;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;

    if (host_statistics(
            mach_host_self(), HOST_CPU_LOAD_INFO, reinterpret_cast<host_info_t>(&cpuinfo), &count)
        != KERN_SUCCESS)
    {
        NX_WARNING(this, "Unable to get CPU load info: %1", SystemError::toString(errno));
        return 0;
    }

    unsigned long long totalTicks = 0;
    for (int i = 0; i < CPU_STATE_MAX; ++i)
        totalTicks += cpuinfo.cpu_ticks[i];

    return calculateTotalCpuLoad(cpuinfo.cpu_ticks[CPU_STATE_IDLE], totalTicks);
}

qreal MacMonitor::CpuLoadMonitor::getSelfCpuLoad()
{
    NX_MUTEX_LOCKER lock(&m_cpuLoadMutex);

    struct rusage data;
    if (getrusage(RUSAGE_SELF, &data) != KERN_SUCCESS)
    {
        NX_WARNING(this, "Unable to get rusage info: %1", SystemError::toString(errno));
        return 0;
    }

    const auto timeStamp = std::chrono::steady_clock::now();
    microseconds selfUsageTime = seconds(data.ru_utime.tv_sec) + microseconds(data.ru_utime.tv_usec)
        + seconds(data.ru_stime.tv_sec) + microseconds(data.ru_stime.tv_usec);

    const auto totalDelta = duration_cast<microseconds>(timeStamp - m_timeStamp);
    const auto usageDelta = selfUsageTime - m_prevSelfUsageTime;

    m_timeStamp = timeStamp;
    m_prevSelfUsageTime = selfUsageTime;

    if (totalDelta == microseconds::zero())
        return 0;

    return static_cast<qreal>(usageDelta.count())
        / (totalDelta.count() * std::thread::hardware_concurrency());
}

qreal MacMonitor::totalCpuUsage()
{
    return m_cpuLoadMonitor->getTotalCpuLoad();
}

qreal MacMonitor::thisProcessCpuUsage()
{
    return m_cpuLoadMonitor->getSelfCpuLoad();
}

class MacMonitor::GpuLoadMonitor
{
public:
    GpuLoadMonitor()
    {
        (void) getSelfGpuLoad();
    }

    virtual ~GpuLoadMonitor() {}

    qreal getSelfGpuLoad();

private:
    struct GpuAccumData
    {
        int64_t deviceUtilizationPercentage = 0; //< Value in range 0..100.
        int64_t selfGpuTime = 0;
        int64_t totalGpuTime = 0;

        qreal computeSelfGpuLoad(const GpuAccumData& prev) const
        {
            const qreal selfDelta = selfGpuTime - prev.selfGpuTime;
            const auto totalDelta = totalGpuTime - prev.totalGpuTime;

            if (totalDelta == 0)
                return 0.0;

            return 0.01 * (qreal) deviceUtilizationPercentage * selfDelta / totalDelta;
        }
    };

    static std::vector<GpuAccumData> getAccumulatedData();

private:
    nx::Mutex m_gpuLoadMutex;
    std::vector<GpuAccumData> m_prevGpuAccumData;
};

qreal MacMonitor::thisProcessGpuUsage()
{
    return m_gpuLoadMonitor->getSelfGpuLoad();
}

qreal MacMonitor::GpuLoadMonitor::getSelfGpuLoad()
{
    // For each GPU get accumulated self GPU time, overall GPU time, and use previously accumulated
    // values to compute relative GPU load. Each GPU also reports device utilization percentage, so
    // we can compute device utilization by current process using its relative GPU load.

    const auto gpuAccumData = getAccumulatedData();

    NX_MUTEX_LOCKER lock(&m_gpuLoadMutex);

    if (gpuAccumData.size() != m_prevGpuAccumData.size())
    {
        m_prevGpuAccumData = gpuAccumData;
        return 0.0;
    }

    // We can return only a single value, so select a GPU with maximum load.
    qreal maxGpuLoad = 0.0;

    // Assume GPUs are always returned in the same order.
    for (size_t i = 0; i < gpuAccumData.size(); ++i)
    {
        const auto gpuLoad = gpuAccumData[i].computeSelfGpuLoad(m_prevGpuAccumData[i]);

        if (gpuLoad > maxGpuLoad)
            maxGpuLoad = gpuLoad;
    }

    m_prevGpuAccumData = gpuAccumData;

    return maxGpuLoad;
}

std::vector<MacMonitor::GpuLoadMonitor::GpuAccumData>
    MacMonitor::GpuLoadMonitor::getAccumulatedData()
{
    using namespace nx::utils;

    const QString selfProcessPrefix = nx::format("pid %1,", getpid());

    std::vector<GpuAccumData> dataPerGpu;

    static constexpr auto kServiceName = "IOAccelerator";

    io_iterator_t accelList = IO_OBJECT_NULL;

    // Iterate over all GPUs.

    const auto ret = IOServiceGetMatchingServices(
        kIOMasterPortDefault,
        IOServiceMatching(kServiceName),
        &accelList);

    if (ret != KERN_SUCCESS)
    {
        NX_WARNING(NX_SCOPE_TAG, "Failed IOServiceGetMatchingServices for '%1' : %2",
            kServiceName, ret);
        return {};
    }
    const auto accelListGuard = makeScopeGuard([&accelList] { IOObjectRelease(accelList); });

    io_iterator_t accel = IO_OBJECT_NULL;
    while ((accel = IOIteratorNext(accelList)))
    {
        const auto accelGuard = makeScopeGuard([&accel] { IOObjectRelease(accel); });

        const auto properties = getIOProperties(accel);
        if (!properties)
            continue;

        // For each GPU get accumulated usage time.
        auto& currentGpu = dataPerGpu.emplace_back();

        if (const auto perf = static_cast<CFDictionaryRef>(
            CFDictionaryGetValue(properties.get(), CFSTR("PerformanceStatistics"))))
        {
            currentGpu.deviceUtilizationPercentage =
                dictionaryGetInt(perf, CFSTR("Device Utilization %"));
        }

        // Iterate over all clients of current GPU.
        io_iterator_t clientList = IO_OBJECT_NULL;
        if (IORegistryEntryGetChildIterator(accel, kIOServicePlane, &clientList) != KERN_SUCCESS)
            continue;
        const auto clientListGuard = makeScopeGuard([&clientList] { IOObjectRelease(clientList); });

        io_object_t client = IO_OBJECT_NULL;
        while ((client = IOIteratorNext(clientList)))
        {
            const auto clientGuard = makeScopeGuard([&client] { IOObjectRelease(client); });

            const auto properties = getIOProperties(client);
            if (!properties)
                continue;

            // For each client (process) gather accumulated GPU time. There can be multiple
            // properties with the same name (for each API used, for each GL context etc.), we need
            // to traverse recursively into properties.
            const auto gpuTime =
                PropertyAccum(properties.get(), CFSTR("accumulatedGPUTime")).value();
            currentGpu.totalGpuTime += gpuTime;

            // If creator string starts with prefix which contains current process pid, save
            // gathered GPU time as used by current process.
            const auto creator =
                dictionaryGetString(properties.get(), CFSTR("IOUserClientCreator"));

            if (creator.startsWith(selfProcessPrefix))
                currentGpu.selfGpuTime += gpuTime;
        }
    }

    return dataPerGpu;
}

class MacMonitor::HddLoadMonitor
{
public:
    HddLoadMonitor() { (void) getTotalHddLoad(); }

    virtual ~HddLoadMonitor() {}

    QList<ActivityMonitor::HddLoad> getTotalHddLoad();

private:
    static QString getDriveName(io_registry_entry_t drive);
    static nanoseconds getGetDriveReadWriteTime(io_registry_entry_t drive);
    qreal computeHddLoad(
        const ActivityMonitor::Hdd& hdd, nanoseconds timeReadWrite, nanoseconds timeDiff);

    nx::Mutex m_hddLoadMutex;

    QHash<QString, nanoseconds> m_readWriteTotalByHddName;
    time_point<steady_clock> m_timeStamp{};
};

qreal MacMonitor::HddLoadMonitor::computeHddLoad(
    const ActivityMonitor::Hdd& hdd, nanoseconds timeReadWrite, nanoseconds timeDiff)
{
    if (!m_readWriteTotalByHddName.contains(hdd.name) || timeDiff == nanoseconds::zero())
    {
        m_readWriteTotalByHddName[hdd.name] = timeReadWrite;
        return 0.0;
    }

    auto& last = m_readWriteTotalByHddName[hdd.name];
    nanoseconds readWriteDiff = timeReadWrite - last;
    last = timeReadWrite;

    const auto ratio = static_cast<qreal>(readWriteDiff.count()) / timeDiff.count();

    return qMin(ratio, 1.0);
}

QString MacMonitor::HddLoadMonitor::getDriveName(io_registry_entry_t drive)
{
    io_registry_entry_t driveMedia;
    const auto ret = IORegistryEntryGetChildEntry(drive, kIOServicePlane, &driveMedia);
    if (ret != KERN_SUCCESS)
    {
        NX_WARNING(NX_SCOPE_TAG, "Failed IORegistryEntryGetChildEntry : %1", ret);
        return {};
    }
    const auto driveMediaGuard =
        nx::utils::makeScopeGuard([&driveMedia]() { IOObjectRelease(driveMedia); });

    const auto properties = getIOProperties(driveMedia);
    if (!properties)
        return {};

    return dictionaryGetString(properties.get(), CFSTR(kIOBSDNameKey));
}

nanoseconds MacMonitor::HddLoadMonitor::getGetDriveReadWriteTime(io_registry_entry_t drive)
{
    const auto properties = getIOProperties(drive);
    if (!properties)
        return {};

    int64_t timeRead = 0;
    int64_t timeWrite = 0;

    if (const auto statistics = static_cast<CFDictionaryRef>(
        CFDictionaryGetValue(properties.get(), CFSTR(kIOBlockStorageDriverStatisticsKey))))
    {
        timeRead = dictionaryGetInt(
            statistics,
            CFSTR(kIOBlockStorageDriverStatisticsTotalReadTimeKey));
        timeWrite = dictionaryGetInt(
            statistics,
            CFSTR(kIOBlockStorageDriverStatisticsTotalWriteTimeKey));
    }

    return nanoseconds(timeRead + timeWrite);
}

QList<ActivityMonitor::HddLoad> MacMonitor::HddLoadMonitor::getTotalHddLoad()
{
    NX_MUTEX_LOCKER lock(&m_hddLoadMutex);

    io_iterator_t driveList;
    static constexpr auto kServiceName = "IOBlockStorageDriver";

    const auto ret = IOServiceGetMatchingServices(
        kIOMasterPortDefault, IOServiceMatching(kServiceName), &driveList);

    if (ret != KERN_SUCCESS)
    {
        NX_WARNING(this, "Failed IOServiceGetMatchingServices : %1", ret);
        return {};
    }
    const auto driveListGuard =
        nx::utils::makeScopeGuard([&driveList]() { IOObjectRelease(driveList); });

    QList<ActivityMonitor::HddLoad> result;

    const auto timeStamp = std::chrono::steady_clock::now();
    const auto timeDiff = duration_cast<nanoseconds>(timeStamp - m_timeStamp);
    m_timeStamp = timeStamp;

    int id = 0;
    for (auto drive = IOIteratorNext(driveList); drive != 0; drive = IOIteratorNext(driveList))
    {
        auto driveGuard = nx::utils::makeScopeGuard([&drive]() { IOObjectRelease(drive); });
        const QString driveName = getDriveName(drive);
        if (driveName.isEmpty())
            continue;

        Hdd hdd(id++, driveName, driveName);
        const nanoseconds driveReadWriteTime = getGetDriveReadWriteTime(drive);
        result.push_back(HddLoad(hdd, computeHddLoad(hdd, driveReadWriteTime, timeDiff)));
    }

    return result;
}

QList<ActivityMonitor::HddLoad> MacMonitor::totalHddLoad()
{
    return m_hddLoadMonitor->getTotalHddLoad();
}

MacMonitor::MacMonitor():
    base_type(),
    m_networkLoadMonitor(new NetworkLoadMonitor()),
    m_cpuLoadMonitor(new CpuLoadMonitor()),
    m_gpuLoadMonitor(new GpuLoadMonitor()),
    m_hddLoadMonitor(new HddLoadMonitor())
{
}

MacMonitor::~MacMonitor()
{
}

QList<ActivityMonitor::PartitionSpace> MacMonitor::totalPartitionSpaceInfo()
{
    QList<ActivityMonitor::PartitionSpace> foundPartitions;

    int count = getfsstat(nullptr, 0, MNT_NOWAIT);

    if (count < 0)
    {
        NX_WARNING(this, "Unable to get number of partitions: %1", SystemError::toString(errno));
        return foundPartitions;
    }

    std::vector<struct statfs> fsList(count);

    count = getfsstat(fsList.data(), fsList.size() * sizeof(struct statfs), MNT_NOWAIT);
    if (count < 0)
    {
        NX_WARNING(this, "Unable to read partitions: %1", SystemError::toString(errno));
        return foundPartitions;
    }

    for (const auto& fs: fsList)
    {
        ActivityMonitor::PartitionSpace partition;

        if (fs.f_flags & MNT_DONTBROWSE || fs.f_owner != 0)
            continue;

        partition.devName = QLatin1String(fs.f_mntfromname);
        partition.path = QLatin1String(fs.f_mntonname);
        partition.freeBytes = fs.f_bavail * fs.f_bsize;
        partition.sizeBytes = fs.f_blocks * fs.f_bsize;
        partition.type = (fs.f_flags & MNT_REMOVABLE)
            ? PartitionType::removable
            : getPartitionTypeByFsType(fs.f_fstypename);

        foundPartitions << partition;
    }

    return foundPartitions;
}

QList<ActivityMonitor::NetworkLoad> MacMonitor::totalNetworkLoad()
{
    return m_networkLoadMonitor->totalNetworkLoad();
}

int MacMonitor::thisProcessThreads()
{
    mach_msg_type_number_t count;
    thread_act_array_t list;

    if (task_threads(mach_task_self(), &list, &count) != KERN_SUCCESS)
        return 1;

    mach_vm_deallocate(mach_task_self(), (mach_vm_address_t)list, count * sizeof(*list));

    return count;
}

quint64 MacMonitor::totalRamUsageBytes()
{
    vm_size_t pageSize;
    vm_statistics64_data_t vmStats;

    mach_port_t machPort = mach_host_self();

    if (host_page_size(machPort, &pageSize) != KERN_SUCCESS)
    {
        NX_WARNING(this, "Unable to get page size: %1", SystemError::toString(errno));
        return 0;
    }

    mach_msg_type_number_t count = sizeof(vmStats) / sizeof(natural_t);

    if (host_statistics64(
            machPort, HOST_VM_INFO, reinterpret_cast<host_info64_t>(&vmStats), &count)
        != KERN_SUCCESS)
    {
        NX_WARNING(this, "Unable to get virtual memory info: %1", SystemError::toString(errno));
        return 0;
    }

    return (static_cast<quint64>(vmStats.active_count) + static_cast<quint64>(vmStats.wire_count))
        * static_cast<quint64>(pageSize);
}

quint64 MacMonitor::thisProcessRamUsageBytes()
{
    struct task_basic_info info;
    mach_msg_type_number_t infoCount = TASK_BASIC_INFO_COUNT;

    if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t) &info, &infoCount)
        != KERN_SUCCESS)
    {
        NX_WARNING(this, "Unable to get task info: %1", SystemError::toString(errno));
        return 0;
    }

    return info.resident_size;
}

quint64 MacMonitor::thisProcessPrivateRamUsageBytes()
{
    vm_size_t pageSize = 0;
    if (host_page_size(mach_host_self(), &pageSize) != KERN_SUCCESS)
    {
        NX_WARNING(this, "Unable to get page size: %1", SystemError::toString(errno));
        return 0;
    }

    quint64 result = 0;
    mach_vm_size_t size = 0;

    // Iterate over memory regions and collect statistics.
    for (mach_vm_address_t addr = 0; ; addr += size)
    {
        vm_region_top_info_data_t info;
        mach_msg_type_number_t count = VM_REGION_TOP_INFO_COUNT;
        mach_port_t objectName;

        kern_return_t kr = mach_vm_region(
            mach_task_self(),
            &addr,
            &size,
            VM_REGION_TOP_INFO,
            (vm_region_info_t) &info,
            &count,
            &objectName);

        if (kr != KERN_SUCCESS)
            break;

        if (info.share_mode == SM_COW && info.ref_count == 1)
            info.share_mode = SM_PRIVATE; //< Treat single reference SM_COW as SM_PRIVATE.

        switch (info.share_mode)
        {
            case SM_PRIVATE:
                result += info.private_pages_resident * pageSize;
                result += info.shared_pages_resident * pageSize;
                break;
            case SM_COW:
                result += info.private_pages_resident * pageSize;
                break;
            default:
                break;
        }
    }

    return result;
}

} // namespace nx::monitoring
