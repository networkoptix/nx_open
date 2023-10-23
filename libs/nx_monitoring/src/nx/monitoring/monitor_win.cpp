// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "monitor_win.h"

#include <thread>
#include <cstdlib>

#include <windows.h>
#include <winioctl.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <iphlpapi.h>

#include <QtCore/QElapsedTimer>

#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>

#include "private/pdh_monitor_win.h"
#include "private/disk_utils_win.h"

// -------------------------------------------------------------------------- //
// WindowsMonitorPrivate
// -------------------------------------------------------------------------- //

namespace nx::monitoring {

class WindowsMonitorPrivate
{
    using TimePoint = quint64;

public:
    WindowsMonitorPrivate()
    {
        m_networkStatTimer.restart();

        NX_ASSERT(sizeof(FILETIME) == sizeof(TimePoint));

        FILETIME currentTimeStruct;
        GetSystemTimeAsFileTime(&currentTimeStruct);
        prevTimePoint = *reinterpret_cast<TimePoint*>(&currentTimeStruct);

        FILETIME unused, kernelTimeStruct, userTimeStruct;
        GetProcessTimes(GetCurrentProcess(), &unused, &unused, &kernelTimeStruct, &userTimeStruct);
        prevKernelTime = *reinterpret_cast<TimePoint*>(&kernelTimeStruct);
        prevUserTime = *reinterpret_cast<TimePoint*>(&userTimeStruct);
    }

    qreal getTotalCpuLoad()
    {
        pdhMonitor.collectMonitoringData();
        return pdhMonitor.getTotalCpuLoad();
    }

    qreal getThisProcessCpuUsage()
    {
        FILETIME currentTimeStruct;
        GetSystemTimeAsFileTime(&currentTimeStruct);
        FILETIME unused, kernelTimeStruct, userTimeStruct;
        GetProcessTimes(GetCurrentProcess(), &unused, &unused, &kernelTimeStruct, &userTimeStruct);

        const TimePoint currentTime = *reinterpret_cast<TimePoint*>(&currentTimeStruct);
        const TimePoint kernelTime = *reinterpret_cast<TimePoint*>(&kernelTimeStruct);
        const TimePoint userTime = *reinterpret_cast<TimePoint*>(&userTimeStruct);

        qreal result = (kernelTime - prevKernelTime) + (userTime - prevUserTime);
        result /= (currentTime - prevTimePoint);
        result /= std::thread::hardware_concurrency();

        prevTimePoint = currentTime;
        prevUserTime = userTime;
        prevKernelTime = kernelTime;

        return result;
    }

    qreal getThisProcessGpuUsage()
    {
        pdhMonitor.collectMonitoringData();
        return pdhMonitor.getThisProcessGpuUsage();
    }

    QList<nx::monitoring::ActivityMonitor::HddLoad> getTotalHddLoad()
    {
        pdhMonitor.collectMonitoringData();
        return pdhMonitor.getTotalHddLoad();
    }

    void recalcNetworkStatistics()
    {
        static const size_t MS_PER_SEC = 1000;

        if (m_mibIfTableBuffer.size() == 0)
            m_mibIfTableBuffer.resize(256);

        DWORD resultCode = NO_ERROR;
        MIB_IFTABLE* networkInterfaceTable = nullptr;
        for (int i = 0; i < 2; ++i)
        {
            ULONG bufSize = (ULONG) m_mibIfTableBuffer.size();
            networkInterfaceTable = reinterpret_cast<MIB_IFTABLE*>(m_mibIfTableBuffer.data());
            resultCode = GetIfTable(networkInterfaceTable, &bufSize, TRUE);
            if (resultCode == ERROR_INSUFFICIENT_BUFFER)
            {
                m_mibIfTableBuffer.resize(bufSize);
                continue;
            }
            break;
        }

        if (resultCode != NO_ERROR)
            return;

        ++m_networkStatCalcCounter;

        const qint64 currentClock = m_networkStatTimer.elapsed();
        for (size_t i = 0; i < networkInterfaceTable->dwNumEntries; ++i)
        {
            const MIB_IFROW& ifInfo = networkInterfaceTable->table[i];
            if (ifInfo.dwPhysAddrLen == 0)
                continue;
            if (!(ifInfo.dwType & IF_TYPE_ETHERNET_CSMACD))
                continue;
            if (ifInfo.dwOperStatus != IF_OPER_STATUS_CONNECTED
                && ifInfo.dwOperStatus != IF_OPER_STATUS_OPERATIONAL)
            {
                continue;
            }

            const QByteArray physicalAddress(reinterpret_cast<const char*>(ifInfo.bPhysAddr),
                ifInfo.dwPhysAddrLen);

            std::pair<std::map<QByteArray, NetworkInterfaceStatData>::iterator, bool>
                p = m_interfaceLoadByMAC.emplace(physicalAddress, NetworkInterfaceStatData());
            if (p.second)
            {
                p.first->second.load.interfaceName = (ifInfo.dwDescrLen == 0)
                    ? QString("Unnamed")
                    : QString::fromLocal8Bit(reinterpret_cast<const char*>(ifInfo.bDescr),
                        ifInfo.dwDescrLen - 1);

                p.first->second.load.macAddress = nx::utils::MacAddress::fromRawData(
                    reinterpret_cast<const unsigned char*>(physicalAddress.constData()));
                p.first->second.load.type = nx::monitoring::ActivityMonitor::PhysicalInterface;
                p.first->second.load.bytesPerSecMax = ifInfo.dwSpeed / CHAR_BIT;
                p.first->second.prevMeasureClock = currentClock;
                p.first->second.inOctets = ifInfo.dwInOctets;
                p.first->second.outOctets = ifInfo.dwOutOctets;
            }
            NetworkInterfaceStatData& intfLoad = p.first->second;
            intfLoad.networkStatCalcCounter = m_networkStatCalcCounter;
            if (intfLoad.prevMeasureClock == currentClock)
                continue;   //not calculating load
            const qint64 msPassed = currentClock - intfLoad.prevMeasureClock;
            intfLoad.load.bytesPerSecIn = p.first->second.load.bytesPerSecIn * 0.3
                + ((ifInfo.dwInOctets - (DWORD) p.first->second.inOctets) * MS_PER_SEC / msPassed)
                * 0.7;
            intfLoad.load.bytesPerSecOut = p.first->second.load.bytesPerSecOut * 0.3
                + ((ifInfo.dwOutOctets - (DWORD) p.first->second.outOctets) * MS_PER_SEC / msPassed)
                * 0.7;

            p.first->second.inOctets = ifInfo.dwInOctets;
            p.first->second.outOctets = ifInfo.dwOutOctets;
            p.first->second.prevMeasureClock = currentClock;
        }

        //removing disabled interface
        for (auto it = m_interfaceLoadByMAC.begin(); it != m_interfaceLoadByMAC.end();)
        {
            if (it->second.networkStatCalcCounter < m_networkStatCalcCounter)
                it = m_interfaceLoadByMAC.erase(it);
            else
                ++it;
        }
    }

    QList<nx::monitoring::ActivityMonitor::NetworkLoad> networkInterfacelLoadData()
    {
        QList<nx::monitoring::ActivityMonitor::NetworkLoad> loadData;
        for (const auto& ifStatData: m_interfaceLoadByMAC)
            loadData.push_back(ifStatData.second.load);
        return loadData;
    }

private:
    struct NetworkInterfaceStatData
    {
        nx::monitoring::ActivityMonitor::NetworkLoad load;
        ULONG64 inOctets = 0;
        ULONG64 outOctets = 0;
        qint64 prevMeasureClock = -1;
        size_t networkStatCalcCounter = 0;
    };

    TimePoint prevTimePoint;
    TimePoint prevKernelTime;
    TimePoint prevUserTime;

    std::map<QByteArray, NetworkInterfaceStatData> m_interfaceLoadByMAC;
    QElapsedTimer m_networkStatTimer;
    size_t m_networkStatCalcCounter = 0;
    std::vector<char> m_mibIfTableBuffer;

    Q_DECLARE_PUBLIC(WindowsMonitor);
    WindowsMonitor* q_ptr;
    PdhMonitor pdhMonitor;
};

WindowsMonitor::WindowsMonitor():
    base_type(),
    d_ptr(new WindowsMonitorPrivate())
{
    d_ptr->q_ptr = this;
}

WindowsMonitor::~WindowsMonitor()
{
}

qreal WindowsMonitor::totalCpuUsage()
{
    Q_D(WindowsMonitor);
    return d->getTotalCpuLoad();
}

quint64 WindowsMonitor::totalRamUsageBytes()
{
    MEMORYSTATUSEX memstat;
    memstat.dwLength = sizeof(memstat);
    if (!GlobalMemoryStatusEx(&memstat))
    {
        NX_WARNING(this, "GlobalMemoryStatusEx failed with error %1", GetLastError());
        return 0;
    }

    return memstat.ullTotalPhys - memstat.ullAvailPhys;
}

qreal WindowsMonitor::thisProcessCpuUsage()
{
    Q_D(WindowsMonitor);
    return d->getThisProcessCpuUsage();
}

qreal WindowsMonitor::thisProcessGpuUsage()
{
    Q_D(WindowsMonitor);
    return d->getThisProcessGpuUsage();
}

namespace {

// It took a lot of time and effort to make those functions work correctly, so while they are not
// used now, we may decide to use them in future. That's why they are still here.
#if 0
    bool isInserted(HANDLE handle)
    {
        DWORD bytesReturned;
        return DeviceIoControl(
            handle, IOCTL_STORAGE_CHECK_VERIFY2,
            nullptr, 0, nullptr, 0, &bytesReturned, nullptr);
    }

    bool isWritable(HANDLE handle)
    {
        DWORD bytesReturned;
        return DeviceIoControl(
            handle, IOCTL_DISK_IS_WRITABLE,
            nullptr, 0, nullptr, 0, &bytesReturned, nullptr);
    }
#endif // if 0


static _STORAGE_BUS_TYPE getBusType(HANDLE handle)
{
    STORAGE_PROPERTY_QUERY query;
    memset(&query, 0, sizeof(query));
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;

    DWORD bytes;
    STORAGE_DEVICE_DESCRIPTOR devd;
    if (!DeviceIoControl(
        handle, IOCTL_STORAGE_QUERY_PROPERTY,
        &query, sizeof(query),
        &devd, sizeof(devd),
        &bytes, nullptr))
    {
        return BusTypeUnknown;
    }

    return devd.BusType;
}

static ActivityMonitor::PartitionType getRealDriveType(const QString& drive)
{
    const auto handle = getDriveHandle(drive);
    if (*handle == INVALID_HANDLE_VALUE)
        return ActivityMonitor::PartitionType::unknown;

    const auto busType = getBusType(*handle);

    return busType == BusTypeUsb
        ? ActivityMonitor::PartitionType::removable : ActivityMonitor::PartitionType::local;
}

static ActivityMonitor::PartitionType getPartitionType(const QString& driveName)
{
    switch (GetDriveType(reinterpret_cast<LPCWSTR>(driveName.constData())))
    {
        case DRIVE_REMOVABLE: return ActivityMonitor::PartitionType::removable;
        case DRIVE_FIXED: return getRealDriveType(driveName);
        case DRIVE_REMOTE: return ActivityMonitor::PartitionType::network;
        case DRIVE_CDROM: return ActivityMonitor::PartitionType::optical;
        case DRIVE_RAMDISK: return ActivityMonitor::PartitionType::ram;
        case DRIVE_UNKNOWN:
        default: return ActivityMonitor::PartitionType::unknown;
    }

    return ActivityMonitor::PartitionType::unknown;
}

static std::tuple<int64_t, int64_t> getSpaceInfo(const QString& driveName)
{
    ULARGE_INTEGER bytesAvailable;
    ULARGE_INTEGER bytesTotal;
    ULARGE_INTEGER bytesFree;
    if (!GetDiskFreeSpaceEx(
        (LPCWSTR) driveName.constData(), &bytesAvailable, &bytesTotal, &bytesFree))
    {
        return std::make_tuple(-1, -1);
    }

    return std::make_tuple(bytesTotal.QuadPart, bytesFree.QuadPart);
}

static ActivityMonitor::PartitionSpace getPartitionInfo(const QString& driveName)
{
    ActivityMonitor::PartitionSpace result;
    result.path = driveName;
    result.devName = driveName;
    result.type = getPartitionType(driveName);
    std::tie(result.sizeBytes, result.freeBytes) = getSpaceInfo(driveName);

    return result;
}

} // <anonymous>

QList<nx::monitoring::ActivityMonitor::PartitionSpace> WindowsMonitor::totalPartitionSpaceInfo()
{
    QList<ActivityMonitor::PartitionSpace> result;
    for (const auto& n: getDriveNames())
        result.append(getPartitionInfo(n));

    return result;
}

QList<nx::monitoring::ActivityMonitor::HddLoad> WindowsMonitor::totalHddLoad()
{
    Q_D(WindowsMonitor);
    return d->getTotalHddLoad();
}

QList<nx::monitoring::ActivityMonitor::NetworkLoad> WindowsMonitor::totalNetworkLoad()
{
    Q_D(WindowsMonitor);

    d->recalcNetworkStatistics();
    return d->networkInterfacelLoadData();
}

int WindowsMonitor::thisProcessThreads()
{
    const auto currentProcessId = GetCurrentProcessId();
    const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 entry = {};

    entry.dwSize = sizeof(entry);
    auto isOk = Process32First(snapshot, &entry);
    while (isOk && entry.th32ProcessID != currentProcessId)
        isOk = Process32Next(snapshot, &entry);
    CloseHandle(snapshot);

    return isOk ? entry.cntThreads : 0;
}

quint64 WindowsMonitor::thisProcessRamUsageBytes()
{
    PROCESS_MEMORY_COUNTERS counters;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
        return counters.WorkingSetSize;
    NX_WARNING(this, "GetProcessMemoryInfo failed with error %1", GetLastError());
    return 0;
}

quint64 WindowsMonitor::thisProcessPrivateRamUsageBytes()
{
    PROCESS_MEMORY_COUNTERS_EX counters;
    if (GetProcessMemoryInfo(
        GetCurrentProcess(),
        reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
        sizeof(counters)))
    {
        return counters.PrivateUsage;
    }
    NX_WARNING(this, "GetProcessMemoryInfo (Ex) failed with error %1", GetLastError());
    return 0;
}

} // namespace nx::monitoring
