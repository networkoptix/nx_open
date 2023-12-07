// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "pdh_monitor_win.h"

#include <windows.h>
#include <winioctl.h>
#include <pdhmsg.h>

#include <nx/utils/log/log.h>

#include "disk_utils_win.h"

using namespace std::chrono;

namespace nx::monitoring {

namespace {

// Parses disk item description returned by PdhGetRawCounterArrayW. String to parse looks
// like this '1 C: D: E:' in most cases. But sometimes only disk id ('1') might be returned.
bool parseDiskDescription(LPCWSTR description, int* id, LPCWSTR* partitions)
{
    if (!description)
        return false;

    LPCWSTR pos = wcschr(description, L' ');
    bool hasName = (pos != nullptr);
    if (hasName)
    {
        while (*pos == L' ')
            pos++;
        if (*pos == L'\0')
            return false;
    }

    int localId = _wtoi(description);
    if (localId == 0 && description[0] != L'0')
        return false;

    if (partitions)
    {
        if (hasName)
            *partitions = pos;
        else
            *partitions = description;
    }

    if (id)
        *id = localId;

    return true;
}

} // namespace

#define INVOKE(expression) \
    (d_func()->checkError(#expression, expression))

PdhMonitor::PdhMonitor()
{
}

PdhMonitor::~PdhMonitor()
{
    if (m_query != INVALID_HANDLE_VALUE)
        INVOKE(PdhCloseQuery(m_query));

    if (m_pdhLibrary)
        FreeLibrary(m_pdhLibrary);
}

bool PdhMonitor::collectMonitoringData()
{
    if (!m_initialized)
    {
        m_initialized = true;
        m_pdhLibrary = LoadLibraryW(L"pdh.dll");
        if (!m_pdhLibrary)
            checkError("LoadLibrary", GetLastError());

        if (INVOKE(PdhOpenQuery(/*szDataSource*/ nullptr, /*dwUserData*/ 0, &m_query)) != ERROR_SUCCESS)
            m_query = INVALID_HANDLE_VALUE;

        addTotalCpuLoadCounter();
        addGpuTimeCounter();
        addDiskTimeCounter();
    }

    if (m_query == INVALID_HANDLE_VALUE)
        return false;

    // Note that we can't use GetTickCount64 since it doesn't exist under XP.
    // GetTickCount wraps every ~50 days, but for this check the wrap is irrelevant.
    std::chrono::milliseconds timeMSec(GetTickCount());
    std::chrono::milliseconds delta = timeMSec - m_lastCpuCollectTime;
    if (delta < kUpdateInterval)
        return false; // Don't update too often.

    m_lastCpuCollectTime = timeMSec;
    if (INVOKE(PdhCollectQueryData(m_query)) != ERROR_SUCCESS)
        return false;

    readTotalCpuLoad();
    readGpuTimeCounterValues(delta);
    calculateTotalHddLoad();

    return true;
}

qreal PdhMonitor::getTotalCpuLoad()
{
    return m_totalCpuLoad;
}

qreal PdhMonitor::getThisProcessGpuUsage()
{
    return m_thisProcessGpuUsage;
}

QList<ActivityMonitor::HddLoad> PdhMonitor::getTotalHddLoad()
{
    return m_totalHddLoad;
}

void PdhMonitor::addTotalCpuLoadCounter()
{
    if (m_query == INVALID_HANDLE_VALUE)
    {
        NX_WARNING(this, "Error in addTotalCpuLoadCounter: invalid query.");
        return;
    }

    // For counters indexes see registry (009 - english counters).
    // Computer\HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Perflib\009\Counter
    //
    // Counters description:
    // https://docs.microsoft.com/en-us/previous-versions/windows/it-pro/windows-server-2003/cc786359(v=ws.10)

    constexpr int kProcessor = 238;
    constexpr int kProcessorTime = 6;

    const QString cpuQuery =
        nx::format("\\%1(_Total)\\%2", perfName(kProcessor), perfName(kProcessorTime));
    if (INVOKE(PdhAddCounter(m_query,
        reinterpret_cast<LPCWSTR>(cpuQuery.utf16()), 0, &m_totalCpuCounter)) == ERROR_SUCCESS)
    {
        if (checkCountersExist(cpuQuery))
        {
            // Skip the initial invalid zero value.
            PDH_FMT_COUNTERVALUE counterVal;
            PdhGetFormattedCounterValue(m_totalCpuCounter, PDH_FMT_DOUBLE, nullptr, &counterVal);
        }
        else
        {
            NX_INFO(this, "No cpu performance counters for query.");
            m_totalCpuCounter = INVALID_HANDLE_VALUE;
        }
    }
    else
        m_totalCpuCounter = INVALID_HANDLE_VALUE;
}

void PdhMonitor::addGpuTimeCounter()
{
    if (m_query == INVALID_HANDLE_VALUE)
    {
        NX_WARNING(this, "Error in addGpuTimeCounter: invalid query.");
        return;
    }

    const QString gpuQuery = QString("\\GPU Engine(pid_%1_*_engtype_3D)\\Running Time")
        .arg(GetCurrentProcessId());

    // PdhLookupPerfNameByIndex does not work with "GPU Engine" - returns empty localized name.
    // PdhAddEnglishCounter is supported since Windows Vista [desktop apps only] and
    // Windows Server 2008 [desktop apps only], but that's ok because "GPU Engine" counter
    // is available starting from Windows 10.
    if (INVOKE(PdhAddEnglishCounter(m_query,
        reinterpret_cast<LPCWSTR>(gpuQuery.utf16()), 0, &m_gpuRunningTimeCounter)) != ERROR_SUCCESS)
    {
        m_gpuRunningTimeCounter = INVALID_HANDLE_VALUE;
    }

    if (!checkCountersExist(gpuQuery))
    {
        NX_INFO(this, "No gpu performance counters for query.");
        m_gpuRunningTimeCounter = INVALID_HANDLE_VALUE;
    }
}

void PdhMonitor::addDiskTimeCounter()
{
    if (m_query == INVALID_HANDLE_VALUE)
    {
        NX_WARNING(this, "Error in addHddCounter: invalid query.");
        return;
    }

    constexpr int kPhysicalDisk = 234;
    constexpr int kPhysicalDiskTime = 200;

    const QString hddQuery =
        nx::format("\\%1(*)\\%2", perfName(kPhysicalDisk), perfName(kPhysicalDiskTime));
    if (INVOKE(PdhAddCounter(m_query,
        reinterpret_cast<LPCWSTR>(hddQuery.utf16()), 0, &m_diskTimeCounter)) != ERROR_SUCCESS)
    {
        m_diskTimeCounter = INVALID_HANDLE_VALUE;
    }

    if (!checkCountersExist(hddQuery))
    {
        NX_INFO(this, "No disk performance counters for query.");
        m_diskTimeCounter = INVALID_HANDLE_VALUE;
    }
}

void PdhMonitor::readTotalCpuLoad()
{
    if (m_totalCpuCounter == INVALID_HANDLE_VALUE)
        return;

    PDH_FMT_COUNTERVALUE counterVal;
    if (INVOKE(PdhGetFormattedCounterValue(m_totalCpuCounter, PDH_FMT_DOUBLE, nullptr, &counterVal))
       == ERROR_SUCCESS)
    {
        m_totalCpuLoad = counterVal.doubleValue / 100.0;
    }
}

std::tuple<QByteArray, int> PdhMonitor::getRawCounterArray(PDH_HCOUNTER counter)
{
    // Invalid handle crashes PdhGetRawCounterArrayW on Windows 7.
    if (counter == INVALID_HANDLE_VALUE)
        return {{}, -1};

    DWORD bufferSize = 0, itemCount = 0;

    PDH_STATUS status = PdhGetRawCounterArrayW(counter, &bufferSize, &itemCount, nullptr);
    if (status != (PDH_STATUS)PDH_MORE_DATA && status != ERROR_SUCCESS)
    {
        checkError("PdhGetRawCounterArrayW", status);
        return {{}, -1};
    }

    // Note that additional symbol is important here.
    // See http://msdn.microsoft.com/en-us/library/windows/desktop/aa372642%28v=vs.85%29.aspx.
    QByteArray buffer(bufferSize + sizeof(WCHAR), '\0');
    bufferSize = buffer.size();
    itemCount = 0;

    auto items = reinterpret_cast<PDH_RAW_COUNTER_ITEM_W*>(buffer.data());
    if (INVOKE(PdhGetRawCounterArrayW(counter, &bufferSize, &itemCount, items)
        != ERROR_SUCCESS))
    {
        return {{}, -1};
    }

    return {buffer, itemCount};
}

void PdhMonitor::readGpuTimeCounterValues(std::chrono::milliseconds interval)
{
    auto [buffer, itemCount] = getRawCounterArray(m_gpuRunningTimeCounter);
    if (itemCount <= 0)
        return;

    auto items = reinterpret_cast<PDH_RAW_COUNTER_ITEM_W*>(buffer.data());

    // There might be multiple GPU in the system, but we are using only one,
    // so just compute the maximum running time across all GPUs.

    LONGLONG maxGpuRunningTime = 0;

    for (int i = 0; i < itemCount; ++i)
    {
        const auto name = QString::fromWCharArray((WCHAR*) items[i].szName);

        PDH_FMT_COUNTERVALUE result;
        PDH_STATUS status = PdhCalculateCounterFromRawValue(
            m_gpuRunningTimeCounter,
            PDH_FMT_LARGE,
            &items[i].RawValue,
            nullptr,
            &result);
        if (status != PDH_CSTATUS_NEW_DATA && status != ERROR_SUCCESS)
        {
            checkError("PdhCalculateCounterFromRawValue", status);
            continue;
        }

        if (result.largeValue > maxGpuRunningTime)
            maxGpuRunningTime = result.largeValue;
    }

    const auto runningDelta = maxGpuRunningTime - m_lastGpuRunningTime;
    m_lastGpuRunningTime = maxGpuRunningTime;

    m_thisProcessGpuUsage = (qreal) runningDelta / (interval.count() * 10'000.0);
}

void PdhMonitor::readDiskCounterValues()
{
    m_itemByDiskId.clear();

    auto [buffer, itemCount] = getRawCounterArray(m_diskTimeCounter);
    if (itemCount <= 0)
        return;

    auto items = reinterpret_cast<PDH_RAW_COUNTER_ITEM_W*>(buffer.data());

    // Populating map {0: "c: d:", 1: "e: f:"}
    QMap<int, QString> driveIndexToPartitions;
    for (const auto& driveName: getDriveNames())
    {
        const auto handle = getDriveHandle(driveName);

        // We do not really use the value of this variable, but we have to pass a non-null
        // lpBytesReturned parameter to DeviceIoControl(); see the function documentation for the
        // details.
        DWORD bytesReturned = 0;

        STORAGE_DEVICE_NUMBER sdn;
        const auto driveType = GetDriveType(reinterpret_cast<LPCWSTR>(driveName.constData()));
        if (*handle != INVALID_HANDLE_VALUE
            && (driveType == DRIVE_FIXED || driveType == DRIVE_REMOVABLE)
            && DeviceIoControl(*handle, IOCTL_STORAGE_GET_DEVICE_NUMBER, nullptr, 0, &sdn,
            sizeof(sdn), &bytesReturned, nullptr))
        {
            if (driveIndexToPartitions.contains(sdn.DeviceNumber))
                driveIndexToPartitions[sdn.DeviceNumber] += " " + driveName.left(2); //< " c:"
            else
                driveIndexToPartitions[sdn.DeviceNumber] = driveName.left(2);
        }
        else
        {
            NX_VERBOSE(this, "readDiskCounterValues: DeviceIoControl failed for drive '%1'",
                driveName);
        }
    }

    // Creating HDD items and populating result.
    for (int i = 0; i < itemCount; ++i)
    {
        int id;
        LPCWSTR partitions;
        if (!parseDiskDescription(items[i].szName, &id, &partitions))
            continue; //< A '_Total' entry or something unexpected.

        ActivityMonitor::Hdd hdd(id, QLatin1String("HDD") + QString::number(id),
            QString::fromWCharArray(partitions));
        // 'partitions' string is unreliable on VirtualBox.
        if (!hdd.partitions.contains(L':'))
        {
            if (driveIndexToPartitions.contains(id))
            {
                hdd.partitions = driveIndexToPartitions[id];
            }
            else
            {
                NX_VERBOSE(
                    this,
                    "readDiskCounterValues: Disk item '%1' partition '%2' doesn't contain ':'. "
                    "Using id '%3' instead",
                    items[i].szName, hdd.partitions, hdd.name);
                hdd.partitions = hdd.name;
            }
        }

        m_itemByDiskId[id] = HddItem(hdd,  items[i].RawValue);
    }
}

qreal PdhMonitor::diskCounterValue(
    const PDH_RAW_COUNTER& last_counter_value,
    const PDH_RAW_COUNTER& current)
{
    if (last_counter_value.FirstValue == current.FirstValue)
        return 0.0;

    PDH_FMT_COUNTERVALUE result;
    PDH_STATUS status = PdhCalculateCounterFromRawValue(
        m_diskTimeCounter,
        PDH_FMT_DOUBLE /*| PDH_FMT_NOCAP100*/,  //TODO #akolesnikov disk usage can be greater then 100% somehow. Maybe, disk can do some I/O concurrently
        const_cast<PDH_RAW_COUNTER*>(&current),
        const_cast<PDH_RAW_COUNTER*>(&last_counter_value),
        &result);
    if (status != PDH_CSTATUS_NEW_DATA && status != ERROR_SUCCESS)
    {
        checkError("PdhCalculateCounterFromRawValue", status);
        return 0.0;
    }

    return result.doubleValue / 100.0;
}

void PdhMonitor::calculateTotalHddLoad()
{
    m_lastItemByDiskId = m_itemByDiskId;
    readDiskCounterValues();

    QList<ActivityMonitor::HddLoad> result;
    for (const HddItem& item: m_itemByDiskId)
    {
        qreal load = 0.0;
        if (m_lastItemByDiskId.contains(item.hdd.id))
            load = diskCounterValue(m_lastItemByDiskId[item.hdd.id].counter, item.counter);

        result.push_back(ActivityMonitor::HddLoad(item.hdd, load));
    }

    m_totalHddLoad = result;
}

bool PdhMonitor::checkCountersExist(const QString& query) const
{
    DWORD countersNumber = 0;
    const auto status = PdhExpandWildCardPathW(
        /* Do not use log file */ NULL,
        reinterpret_cast<LPCWSTR>(query.utf16()),
        /* Result counter paths */ NULL,
        &countersNumber,
        /* Expand all wildcards */ 0);

    if (status != ERROR_SUCCESS)
    {
        if (status == PDH_INVALID_PATH)
             NX_WARNING(this, "Error in PdhExpandWildCardPathW: invalid query.");
        else if (status == PDH_CSTATUS_NO_OBJECT)
             NX_WARNING(this, "Error in PdhExpandWildCardPathW: no pdh object.");
        else
             NX_WARNING(this, "Error in PdhExpandWildCardPathW.");
    }

    return countersNumber > 0;
}

// For INVOKE to work.
const PdhMonitor* PdhMonitor::d_func() const
{
    return this;
}

DWORD PdhMonitor::checkError(const char* expression, DWORD status) const
{
    NX_ASSERT(expression);

    if (status == ERROR_SUCCESS)
        return status;

    QByteArray function(expression);
    int index = function.indexOf('(');
    if (index != -1)
        function.truncate(index);

    LPWSTR buffer = nullptr;
    if (FormatMessageW(
        FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_ALLOCATE_BUFFER
            | FORMAT_MESSAGE_IGNORE_INSERTS,
        m_pdhLibrary, status, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr) == 0)
    {
        buffer = nullptr; // Error in FormatMessage.
    }

    if (buffer)
    {
        QString message = QString::fromWCharArray(buffer);
        while (message.endsWith('\n') || message.endsWith('\r'))
            message.truncate(message.size() - 1);

        NX_WARNING(this, "Error in %1: %2 (%3).", function, status, message);
        LocalFree(buffer);
    }
    else
    {
        NX_WARNING(this, "Error in %1: %2.", function, status);
    }

    return status;
}

QString PdhMonitor::perfName(DWORD index)
{
    DWORD size = 0;
    PDH_STATUS status = PdhLookupPerfNameByIndexW(nullptr, index, nullptr, &size);
    if (status != (PDH_STATUS)PDH_MORE_DATA && status != ERROR_SUCCESS)
    {
        checkError("PdhLookupPerfNameByIndexW", status);
        return QString();
    }

    if (size == 0)
    {
        NX_WARNING(this, "Zero-sized performance object name (%1) received from OS", index);
        return QString();
    }

    QByteArray buffer((size + 1) * sizeof(WCHAR), '\0');
    if (INVOKE(PdhLookupPerfNameByIndexW(nullptr, index,
        reinterpret_cast<LPWSTR>(buffer.data()), &size)) != ERROR_SUCCESS)
    {
        return QString();
    }

    return QString::fromWCharArray(reinterpret_cast<LPCWSTR>(buffer.constData()));
}

} // namespace nx::monitoring
