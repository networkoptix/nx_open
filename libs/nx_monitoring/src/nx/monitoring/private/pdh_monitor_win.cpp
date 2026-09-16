// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "pdh_monitor_win.h"

#include <windows.h>

#include <pdhmsg.h>
#include <winioctl.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdio>
#include <cwchar>
#include <expected>
#include <filesystem>
#include <optional>
#include <ranges>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nx/ranges.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std_string_utils.h>

#include "disk_utils_win.h"

using namespace std::chrono;

namespace nx::monitoring {

namespace {

// PDH reports an aggregate instance alongside the per-disk ones.
constexpr std::string_view kTotalInstance = "_Total";

// PDH status codes are documented in hex.
std::string formatStatus(const DWORD status)
{
    std::array<char, 11> buffer{}; //< "0x" + 8 hex digits + NUL.
    std::snprintf(buffer.data(), buffer.size(), "0x%08lX", static_cast<unsigned long>(status));
    return buffer.data();
}

// The A entry points return localized names in the system ANSI code page; logs are UTF-8.
std::string ansiToUtf8(const std::string_view text)
{
    if (text.empty())
        return {};

    const int wideSize =
        MultiByteToWideChar(CP_ACP, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(std::max(wideSize, 0)), L'\0');
    MultiByteToWideChar(
        CP_ACP, 0, text.data(), static_cast<int>(text.size()), wide.data(), wideSize);

    const int size =
        WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideSize, nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(std::max(size, 0)), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideSize, result.data(), size, nullptr, nullptr);
    return result;
}

struct DiskDescription
{
    DWORD id;
    std::vector<std::string_view> partitions;
};

// Parses disk item description returned by PdhGetRawCounterArrayA. String to parse looks
// like this '1 C: D: E:' in most cases. But sometimes only disk id ('1') might be returned.
std::optional<DiskDescription> parseDiskDescription(const std::string_view description)
{
    const std::vector chunks = description | std::views::split(' ')
        | std::views::filter([](const auto& chunk) { return !std::ranges::empty(chunk); })
        | std::views::transform(nx::ranges::asStringView) | nx::ranges::to<std::vector>();
    if (chunks.empty())
    {
        NX_WARNING(NX_SCOPE_TAG, "Received an empty physical disk counter description");
        return std::nullopt;
    }

    const std::string_view idText = chunks.front();
    DWORD id = 0;
    const auto idEnd = idText.data() + idText.size();
    const auto [end, error] = std::from_chars(idText.data(), idEnd, id);
    if (error != std::errc{} || end != idEnd)
    {
        if (idText != kTotalInstance)
        {
            NX_WARNING(NX_SCOPE_TAG,
                "Invalid physical disk counter description: %1",
                std::string(description));
        }
        return std::nullopt;
    }

    return DiskDescription{
        .id = id,
        .partitions = chunks | std::views::drop(1) | nx::ranges::to<std::vector>(),
    };
}

std::string pdhErrorMessage(const HMODULE pdhLibrary, const DWORD status)
{
    if (!pdhLibrary)
        return {};

    LPSTR buffer = nullptr;
    if (FormatMessageA(FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_ALLOCATE_BUFFER
                | FORMAT_MESSAGE_IGNORE_INSERTS,
            pdhLibrary,
            status,
            /*dwLanguageId*/ 0,
            reinterpret_cast<LPSTR>(&buffer),
            /*nSize*/ 0,
            /*arguments*/ nullptr)
        == 0)
    {
        return {};
    }

    std::string result(buffer);
    LocalFree(buffer);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();
    return result;
}

std::optional<PDH_HCOUNTER> addPdhCounterToQuery(
    const HMODULE pdhLibrary, const PDH_HQUERY query, const std::string& counterPath)
{
    PDH_HCOUNTER counter = INVALID_HANDLE_VALUE;
    // English PDH counter paths are ASCII, so UTF-8 needs no conversion here.
    const auto status = PdhAddEnglishCounterA(query, counterPath.c_str(), 0, &counter);
    if (ERROR_SUCCESS != status)
    {
        NX_WARNING(NX_SCOPE_TAG,
            "PdhAddEnglishCounterA failed for %1: %2 (%3)",
            counterPath,
            formatStatus(status),
            pdhErrorMessage(pdhLibrary, status));
        return std::nullopt;
    }

    return counter;
}

struct RawCounterItem
{
    std::string name;
    PDH_RAW_COUNTER value;
};

std::optional<std::vector<RawCounterItem>> readRawCounterItems(
    const HMODULE pdhLibrary, const PDH_HCOUNTER counter)
{
    // Invalid handle crashes PdhGetRawCounterArrayA on Windows 7.
    if (counter == INVALID_HANDLE_VALUE)
        return std::nullopt;

    DWORD bufferSize = 0;
    DWORD itemCount = 0;
    const auto sizeStatus = PdhGetRawCounterArrayA(counter, &bufferSize, &itemCount, nullptr);
    if (sizeStatus != static_cast<PDH_STATUS>(PDH_MORE_DATA) && sizeStatus != ERROR_SUCCESS)
    {
        NX_WARNING(NX_SCOPE_TAG,
            "PdhGetRawCounterArrayA failed with status %1 (%2)",
            formatStatus(sizeStatus),
            pdhErrorMessage(pdhLibrary, sizeStatus));
        return std::nullopt;
    }

    // The additional character is required by PdhGetRawCounterArrayA.
    const auto requiredSize = bufferSize + sizeof(char);
    std::vector<PDH_RAW_COUNTER_ITEM_A> buffer(
        (requiredSize + sizeof(PDH_RAW_COUNTER_ITEM_A) - 1) / sizeof(PDH_RAW_COUNTER_ITEM_A),
        PDH_RAW_COUNTER_ITEM_A{});
    bufferSize = static_cast<DWORD>(buffer.size() * sizeof(PDH_RAW_COUNTER_ITEM_A));
    itemCount = 0;
    const auto readStatus =
        PdhGetRawCounterArrayA(counter, &bufferSize, &itemCount, buffer.data());
    if (ERROR_SUCCESS != readStatus)
    {
        NX_WARNING(NX_SCOPE_TAG,
            "PdhGetRawCounterArrayA failed with status %1 (%2)",
            formatStatus(readStatus),
            pdhErrorMessage(pdhLibrary, readStatus));
        return std::nullopt;
    }

    // PdhGetRawCounterArrayA stores the instance names in the tail of the same buffer, after the
    // item array, so they have to be copied out before the buffer goes away.
    return std::views::counted(buffer.data(), itemCount)
        | std::views::transform([](const PDH_RAW_COUNTER_ITEM_A& item) -> RawCounterItem
            { return {.name = item.szName ? item.szName : "", .value = item.RawValue}; })
        | nx::ranges::to<std::vector>();
}

std::unordered_map<DWORD, std::vector<std::filesystem::path>> readMountPointsByDiskId()
{
    std::unordered_map<DWORD, std::vector<std::filesystem::path>> result;
    std::array<wchar_t, MAX_PATH> volumeName{};
    const HANDLE volumeSearch =
        FindFirstVolumeW(volumeName.data(), static_cast<DWORD>(volumeName.size()));
    if (volumeSearch == INVALID_HANDLE_VALUE)
        return result;

    const auto closeVolumeSearch =
        nx::utils::makeScopeGuard([volumeSearch] { FindVolumeClose(volumeSearch); });
    do
    {
        // CreateFileW rejects a volume GUID path that ends with a backslash, while
        // GetVolumePathNamesForVolumeNameW requires one, so only the handle path is trimmed.
        const std::wstring_view name(volumeName.data());
        const std::wstring devicePath(
            name.substr(0, name.size() - (name.ends_with(L'\\') ? 1 : 0)));

        const HANDLE volume = CreateFileW(devicePath.c_str(),
            FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr);
        if (volume == INVALID_HANDLE_VALUE)
            continue;

        const auto closeVolume = nx::utils::makeScopeGuard([volume] { CloseHandle(volume); });
        STORAGE_DEVICE_NUMBER deviceNumber{};
        DWORD bytesReturned = 0;
        if (!DeviceIoControl(volume,
                IOCTL_STORAGE_GET_DEVICE_NUMBER,
                nullptr,
                0,
                &deviceNumber,
                sizeof(deviceNumber),
                &bytesReturned,
                nullptr))
        {
            continue;
        }

        DWORD pathBufferSize = 0;
        if (GetVolumePathNamesForVolumeNameW(volumeName.data(), nullptr, 0, &pathBufferSize)
            || GetLastError() != ERROR_MORE_DATA)
        {
            continue; //< The volume has no mount points, or the size query failed.
        }

        std::vector pathBuffer(pathBufferSize, wchar_t{});
        if (!GetVolumePathNamesForVolumeNameW(
                volumeName.data(), pathBuffer.data(), pathBufferSize, &pathBufferSize))
        {
            continue;
        }

        for (const auto& path: pathBuffer | std::views::split(L'\0')
                | std::views::filter([](const auto& path) { return !std::ranges::empty(path); }))
        {
            std::filesystem::path mountPoint(std::ranges::begin(path), std::ranges::end(path));
            if (mountPoint != mountPoint.root_path() && mountPoint.filename().empty())
                mountPoint = mountPoint.parent_path();
            result[deviceNumber.DeviceNumber].push_back(std::move(mountPoint));
        }
    } while (
        FindNextVolumeW(volumeSearch, volumeName.data(), static_cast<DWORD>(volumeName.size())));

    return result;
}

} // namespace

#define INVOKE(expression) (d_func()->checkError(#expression, expression))

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

        if (INVOKE(PdhOpenQuery(/*szDataSource*/ nullptr, /*dwUserData*/ 0, &m_query))
            != ERROR_SUCCESS)
            m_query = INVALID_HANDLE_VALUE;

        addTotalCpuLoadCounter();
        addGpuTimeCounter();
        addDiskTimeCounter();

        // Collect data twice on first run to init CPU counters, according to
        // PdhGetFormattedCounterValue manual.
        if (m_query != INVALID_HANDLE_VALUE)
            INVOKE(PdhCollectQueryData(m_query));
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
    if (std::optional diskIo = calculateTotalDiskIo())
        m_totalDiskIo = std::move(*diskIo);

    return true;
}

double PdhMonitor::getTotalCpuLoad()
{
    return m_totalCpuLoad;
}

double PdhMonitor::getThisProcessGpuUsage()
{
    return m_thisProcessGpuUsage;
}

std::vector<ActivityMonitor::HddLoad> PdhMonitor::getTotalHddLoad()
{
    return m_totalHddLoad;
}

std::vector<ActivityMonitor::DiskIo> PdhMonitor::getTotalDiskIo()
{
    return m_totalDiskIo;
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

    const std::string cpuQuery =
        "\\" + perfName(kProcessor) + "(_Total)\\" + perfName(kProcessorTime);

    if (!checkCountersExist(cpuQuery))
    {
        m_totalCpuCounter = INVALID_HANDLE_VALUE;
        return;
    }

    if (INVOKE(PdhAddCounterA(m_query, cpuQuery.c_str(), 0, &m_totalCpuCounter)) != ERROR_SUCCESS)
    {
        NX_WARNING(this, "No CPU performance counters for: %1", ansiToUtf8(cpuQuery));
        m_totalCpuCounter = INVALID_HANDLE_VALUE;
    }
}

void PdhMonitor::addGpuTimeCounter()
{
    if (m_query == INVALID_HANDLE_VALUE)
    {
        NX_WARNING(this, "Error in addGpuTimeCounter: invalid query.");
        return;
    }

    const std::string gpuQuery = "\\GPU Engine(pid_" + std::to_string(GetCurrentProcessId())
        + "_*_engtype_3D)\\Running Time";

    // PdhLookupPerfNameByIndex does not work with "GPU Engine" - returns empty localized name.
    // PdhAddEnglishCounter is supported since Windows Vista [desktop apps only] and
    // Windows Server 2008 [desktop apps only], but that's ok because "GPU Engine" counter
    // is available starting from Windows 10.
    if (INVOKE(PdhAddEnglishCounterA(m_query, gpuQuery.c_str(), 0, &m_gpuRunningTimeCounter))
        != ERROR_SUCCESS)
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

    const std::string hddQuery =
        "\\" + perfName(kPhysicalDisk) + "(*)\\" + perfName(kPhysicalDiskTime);
    if (INVOKE(PdhAddCounterA(m_query, hddQuery.c_str(), 0, &m_diskTimeCounter)) != ERROR_SUCCESS)
    {
        m_diskTimeCounter = INVALID_HANDLE_VALUE;
    }

    if (!checkCountersExist(hddQuery))
    {
        NX_INFO(this, "No disk performance counters for query.");
        m_diskTimeCounter = INVALID_HANDLE_VALUE;
    }

    m_diskReadCounter =
        addPdhCounterToQuery(m_pdhLibrary, m_query, "\\PhysicalDisk(*)\\Disk Reads/sec")
            .value_or(INVALID_HANDLE_VALUE);
    m_diskWriteCounter =
        addPdhCounterToQuery(m_pdhLibrary, m_query, "\\PhysicalDisk(*)\\Disk Writes/sec")
            .value_or(INVALID_HANDLE_VALUE);
}

void PdhMonitor::readTotalCpuLoad()
{
    if (m_totalCpuCounter == INVALID_HANDLE_VALUE)
        return;

    PDH_FMT_COUNTERVALUE counterVal;
    if (INVOKE(
            PdhGetFormattedCounterValue(m_totalCpuCounter, PDH_FMT_DOUBLE, nullptr, &counterVal))
        == ERROR_SUCCESS)
    {
        m_totalCpuLoad = counterVal.doubleValue / 100.0;
    }
}

void PdhMonitor::readGpuTimeCounterValues(std::chrono::milliseconds interval)
{
    const std::optional items = readRawCounterItems(m_pdhLibrary, m_gpuRunningTimeCounter);
    if (!items || items->empty())
        return;

    // There might be multiple GPU in the system, but we are using only one,
    // so just compute the maximum running time across all GPUs.
    std::int64_t maxGpuRunningTime = 0;
    for (const RawCounterItem& item: *items)
    {
        PDH_FMT_COUNTERVALUE result;
        PDH_RAW_COUNTER rawValue = item.value;
        const PDH_STATUS status = PdhCalculateCounterFromRawValue(
            m_gpuRunningTimeCounter, PDH_FMT_LARGE, &rawValue, nullptr, &result);
        if (status != PDH_CSTATUS_NEW_DATA && status != ERROR_SUCCESS)
        {
            checkError("PdhCalculateCounterFromRawValue", status);
            continue;
        }

        maxGpuRunningTime = std::max(maxGpuRunningTime, result.largeValue);
    }

    const auto runningDelta = maxGpuRunningTime - m_lastGpuRunningTime;
    m_lastGpuRunningTime = maxGpuRunningTime;

    m_thisProcessGpuUsage = static_cast<double>(runningDelta) / (interval.count() * 10'000.0);
}

void PdhMonitor::readDiskCounterValues()
{
    m_itemByDiskId.clear();

    const std::optional items = readRawCounterItems(m_pdhLibrary, m_diskTimeCounter);
    if (!items || items->empty())
        return;

    // Populating map {0: "c: d:", 1: "e: f:"}
    std::unordered_map<DWORD, std::string> driveIndexToPartitions;
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
            && DeviceIoControl(*handle,
                IOCTL_STORAGE_GET_DEVICE_NUMBER,
                nullptr,
                0,
                &sdn,
                sizeof(sdn),
                &bytesReturned,
                nullptr))
        {
            const std::string partition = driveName.left(2).toStdString();
            if (driveIndexToPartitions.contains(sdn.DeviceNumber))
                driveIndexToPartitions[sdn.DeviceNumber] += " " + partition; //< " c:"
            else
                driveIndexToPartitions[sdn.DeviceNumber] = partition;
        }
        else
        {
            NX_VERBOSE(
                this, "readDiskCounterValues: DeviceIoControl failed for drive '%1'", driveName);
        }
    }

    // Creating HDD items and populating result.
    for (const RawCounterItem& item: *items)
    {
        const std::string& name = item.name;
        const std::optional description = parseDiskDescription(name);
        if (!description)
            continue; //< A '_Total' entry or something unexpected.

        const auto& [id, partitions] = *description;
        ActivityMonitor::Hdd hdd{
            .id = id,
            .name = "HDD" + std::to_string(id),
            .partitions = nx::utils::join(partitions, " "),
        };
        // 'partitions' string is unreliable on VirtualBox.
        if (!hdd.partitions.contains(':'))
        {
            if (driveIndexToPartitions.contains(id))
            {
                hdd.partitions = driveIndexToPartitions.at(id);
            }
            else
            {
                NX_VERBOSE(this,
                    "readDiskCounterValues: Disk item '%1' partition '%2' doesn't contain ':'. "
                    "Using id '%3' instead",
                    name,
                    hdd.partitions,
                    hdd.name);
                hdd.partitions = hdd.name;
            }
        }

        m_itemByDiskId[id] = HddItem(hdd, item.value);
    }
}

std::optional<std::unordered_map<DWORD, PDH_RAW_COUNTER>> PdhMonitor::readDiskCountersByDiskId(
    const PDH_HCOUNTER counter)
{
    const std::optional items = readRawCounterItems(m_pdhLibrary, counter);
    if (!items)
        return std::nullopt;

    const auto [counters, invalidNames] = *items
        | std::views::filter(
            [](const RawCounterItem& item) { return item.name != kTotalInstance; })
        | std::views::transform(
            [](const RawCounterItem& item)
                -> std::expected<std::pair<DWORD, PDH_RAW_COUNTER>, std::string_view>
            {
                const std::optional description = parseDiskDescription(item.name);
                if (!description)
                    return std::unexpected(item.name);
                return std::pair{description->id, item.value};
            })
        | nx::actions::partitionSums;
    if (!invalidNames.empty())
    {
        NX_WARNING(NX_SCOPE_TAG,
            "Unable to parse physical disk counter descriptions: %1",
            nx::utils::join(invalidNames, ", "));
    }

    return counters | nx::ranges::to<std::unordered_map>();
}

double PdhMonitor::diskCounterValue(
    const PDH_RAW_COUNTER& last_counter_value,
    const PDH_RAW_COUNTER& current)
{
    if (last_counter_value.FirstValue == current.FirstValue)
        return 0.0;

    PDH_FMT_COUNTERVALUE result;
    const PDH_STATUS status = PdhCalculateCounterFromRawValue(m_diskTimeCounter,
        PDH_FMT_DOUBLE /*| PDH_FMT_NOCAP100*/, // TODO #akolesnikov disk usage can be greater then
                                               // 100% somehow. Maybe, disk can do some I/O
                                               // concurrently
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

namespace {

double diskOperationsPerSecond(const HMODULE pdhLibrary,
    const PDH_HCOUNTER counter,
    const PDH_RAW_COUNTER& last,
    const PDH_RAW_COUNTER& current)
{
    if (counter == INVALID_HANDLE_VALUE || last.FirstValue == current.FirstValue)
        return 0.0;

    PDH_FMT_COUNTERVALUE result{};
    const PDH_STATUS status = PdhCalculateCounterFromRawValue(counter,
        PDH_FMT_DOUBLE | PDH_FMT_NOCAP100,
        const_cast<PDH_RAW_COUNTER*>(&current),
        const_cast<PDH_RAW_COUNTER*>(&last),
        &result);
    if (status != PDH_CSTATUS_NEW_DATA && status != ERROR_SUCCESS)
    {
        NX_WARNING(NX_SCOPE_TAG,
            "PdhCalculateCounterFromRawValue failed with status %1 (%2)",
            formatStatus(status),
            pdhErrorMessage(pdhLibrary, status));
        return 0.0;
    }
    if (result.CStatus != PDH_CSTATUS_VALID_DATA && result.CStatus != PDH_CSTATUS_NEW_DATA)
    {
        NX_WARNING(NX_SCOPE_TAG,
            "PdhCalculateCounterFromRawValue returned invalid data status %1 (%2)",
            formatStatus(result.CStatus),
            pdhErrorMessage(pdhLibrary, result.CStatus));
        return 0.0;
    }

    return result.doubleValue;
}

double diskOperationsPerSecond(const HMODULE pdhLibrary,
    const PDH_HCOUNTER counter,
    const std::unordered_map<DWORD, PDH_RAW_COUNTER>& previousByDiskId,
    const std::unordered_map<DWORD, PDH_RAW_COUNTER>& currentByDiskId,
    const DWORD diskId)
{
    const auto previous = previousByDiskId.find(diskId);
    const auto current = currentByDiskId.find(diskId);
    if (previous == previousByDiskId.end() || current == currentByDiskId.end())
        return 0.0;

    return diskOperationsPerSecond(pdhLibrary, counter, previous->second, current->second);
}

} // namespace

void PdhMonitor::calculateTotalHddLoad()
{
    m_lastItemByDiskId = m_itemByDiskId;
    readDiskCounterValues();

    std::vector<ActivityMonitor::HddLoad> result;
    for (const auto& [diskId, item]: m_itemByDiskId)
    {
        double load = 0.0;
        if (m_lastItemByDiskId.contains(diskId))
            load = diskCounterValue(m_lastItemByDiskId.at(diskId).counter, item.counter);

        result.push_back({.hdd = item.hdd, .load = load});
    }

    m_totalHddLoad = std::move(result);
}

std::optional<std::vector<ActivityMonitor::DiskIo>> PdhMonitor::calculateTotalDiskIo()
{
    const std::optional readCountersByDiskId = readDiskCountersByDiskId(m_diskReadCounter);
    if (!readCountersByDiskId)
        return std::nullopt;
    const std::optional writeCountersByDiskId = readDiskCountersByDiskId(m_diskWriteCounter);
    if (!writeCountersByDiskId)
        return std::nullopt;

    // Volume enumeration is a handful of kernel calls per volume; the mapping only changes on
    // mount and unmount, so it is refreshed rarely.
    if (const auto now = steady_clock::now();
        now - m_mountPointsReadAt >= kMountPointsRefreshInterval)
    {
        m_mountPointsByDiskId = readMountPointsByDiskId();
        m_mountPointsReadAt = now;
    }

    const std::vector result = m_mountPointsByDiskId
        | std::views::transform(
            [this, &readCountersByDiskId, &writeCountersByDiskId](
                const std::pair<const DWORD, std::vector<std::filesystem::path>>& item)
                -> ActivityMonitor::DiskIo
            {
                const auto& [diskId, mountPoints] = item;
                return {
                    .device = "HDD" + std::to_string(diskId),
                    .mountPoints = mountPoints,
                    .readOperationsPerSecond = diskOperationsPerSecond(m_pdhLibrary,
                        m_diskReadCounter,
                        m_lastDiskReadCountersById,
                        *readCountersByDiskId,
                        diskId),
                    .writeOperationsPerSecond = diskOperationsPerSecond(m_pdhLibrary,
                        m_diskWriteCounter,
                        m_lastDiskWriteCountersById,
                        *writeCountersByDiskId,
                        diskId),
                };
            })
        | nx::ranges::to<std::vector>();

    m_lastDiskReadCountersById = *readCountersByDiskId;
    m_lastDiskWriteCountersById = *writeCountersByDiskId;
    return result;
}

bool PdhMonitor::checkCountersExist(const std::string& query) const
{
    DWORD countersNumber = 0;
    const auto status = PdhExpandWildCardPathA(
        /* Do not use log file */ NULL,
        query.c_str(),
        /* Result counter paths */ NULL,
        &countersNumber,
        /* Expand all wildcards */ 0);

    if (status != ERROR_SUCCESS && status != PDH_MORE_DATA)
    {
        const std::string queryText = ansiToUtf8(query);
        if (status == PDH_INVALID_PATH)
            NX_WARNING(this, "Error in PdhExpandWildCardPathA[%1]: invalid query.", queryText);
        else if (status == PDH_CSTATUS_NO_OBJECT)
            NX_WARNING(this, "Error in PdhExpandWildCardPathA[%1]: no pdh object.", queryText);
        else
            NX_WARNING(this,
                "Error in PdhExpandWildCardPathA[%1]: %2 (%3)",
                queryText,
                formatStatus(status),
                pdhErrorMessage(m_pdhLibrary, status));
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

    std::string_view function(expression);
    if (const auto index = function.find('('); index != std::string_view::npos)
        function = function.substr(0, index);

    const std::string message = pdhErrorMessage(m_pdhLibrary, status);
    if (!message.empty())
        NX_WARNING(this, "Error in %1: %2 (%3)", function, formatStatus(status), message);
    else
        NX_WARNING(this, "Error in %1: %2", function, formatStatus(status));

    return status;
}

std::string PdhMonitor::perfName(DWORD index)
{
    DWORD size = 0;
    const PDH_STATUS status = PdhLookupPerfNameByIndexA(nullptr, index, nullptr, &size);
    if (status != (PDH_STATUS) PDH_MORE_DATA && status != ERROR_SUCCESS)
    {
        checkError("PdhLookupPerfNameByIndexA", status);
        return {};
    }

    if (size == 0)
    {
        NX_WARNING(this, "Zero-sized performance object name (%1) received from OS", index);
        return {};
    }

    std::vector buffer(size + 1, char{});
    if (INVOKE(PdhLookupPerfNameByIndexA(nullptr, index, buffer.data(), &size)) != ERROR_SUCCESS)
    {
        return {};
    }

    return buffer.data();
}

} // namespace nx::monitoring
