// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "monitor_p_linux.h"

#include <array>
#include <chrono>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <expected>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QStringList>

#include <errno.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <sys/time.h>
#include <time.h>
#include <net/if_arp.h>

#include <nx/ranges.h>
#include <nx/utils/concurrent.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/format.h>
#include <nx/utils/log/log.h>
#include <nx/utils/mac_address.h>
#include <nx/utils/std_string_utils.h>
#include <nx/utils/system_error.h>

namespace nx::monitoring {

namespace {

const int BYTES_PER_MB = 1024 * 1024;
const size_t MAX_LINE_LENGTH = 512;

int64_t exponentialSmoothing(int64_t newValue, int64_t oldValue, const double kSmoothingFactor = 0.7)
{
    return static_cast<int64_t>(newValue * kSmoothingFactor + oldValue * (1 - kSmoothingFactor));
}

QByteArray readFileContents(const QString& filePath)
{
    QFile statFile(filePath);
    if (!statFile.open(QIODevice::ReadOnly))
        return QByteArray();
    return statFile.readAll().trimmed();
}

//!This structure is read from /proc/diskstat
struct DiskStatSys {
    int major_num;
    int minor_num;
    char device_name[FILENAME_MAX];
    unsigned int num_reads;
    unsigned int reads_merged;
    unsigned int sectors_read;
    unsigned int tot_read_ms;
    unsigned int num_writes;
    unsigned int writes_merged;
    unsigned int sectors_written;
    unsigned int tot_write_ms;
    unsigned int cur_io_cnt;
    unsigned int tot_io_ms;
    unsigned int io_weighted_ms;
};

class DiskStat {
public:
    int major_num;
    int minor_num;
    std::string deviceName;
    unsigned int diskUtilizationPercent;

    DiskStat():
        major_num(0),
        minor_num(0),
        diskUtilizationPercent(0)
    {}
};

class DiskStatContext {
public:
    DiskStatSys prevSysStat;
    DiskStat prevStat;
    bool initialized;

    DiskStatContext(): initialized(false) {
        memset(&prevSysStat, 0, sizeof(prevSysStat));
    }
};

std::uint64_t diskId(unsigned int majorNumber, unsigned int minorNumber)
{
    return (static_cast<std::uint64_t>(majorNumber) << 32) | minorNumber;
}

std::error_code streamError(const int errorCode)
{
    return errorCode != 0 ? std::error_code(errorCode, std::generic_category())
                          : std::make_error_code(std::io_errc::stream);
}

// TODO: #skolesnik Move this helper to shared file utilities.
std::expected<std::string, std::error_code> readTextFile(const std::filesystem::path& path)
{
    errno = 0;
    std::ifstream file(path, std::ios::binary);
    // libstdc++ leaves the underlying POSIX error in errno.
    const int openError = errno;
    if (!file.is_open())
        return std::unexpected(streamError(openError));

    std::array<char, 64 * 1024> buffer{};
    std::string contents;
    while (file)
    {
        errno = 0;
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const int readError = errno;
        contents.append(buffer.data(), static_cast<std::size_t>(file.gcount()));
        if (file.bad())
            return std::unexpected(streamError(readError));
    }

    return contents;
}

// Leading fields of one line in the Linux /proc/diskstats file. Extraction is positional, so the
// fields between readsCompleted and writesCompleted are parsed only to reach the latter.
struct DiskStatsFields
{
    unsigned int majorNumber;
    unsigned int minorNumber;
    std::string deviceName;
    std::uint64_t readsCompleted;
    std::uint64_t readsMerged;
    std::uint64_t sectorsRead;
    std::uint64_t readMilliseconds;
    std::uint64_t writesCompleted;

    static std::expected<DiskStatsFields, std::string_view> parse(std::string_view line)
    {
        std::istringstream fields{std::string(line)};
        DiskStatsFields result{};
        if (fields >> result.majorNumber >> result.minorNumber >> result.deviceName
            >> result.readsCompleted >> result.readsMerged >> result.sectorsRead
            >> result.readMilliseconds >> result.writesCompleted)
        {
            return result;
        }

        return std::unexpected(line);
    }
};

double operationsPerSecond(const std::uint64_t current,
    const std::uint64_t previous,
    const std::chrono::milliseconds elapsed)
{
    if (elapsed <= std::chrono::milliseconds::zero() || current < previous)
        return 0.0;

    return (current - previous) / std::chrono::duration<double>(elapsed).count();
}

ActivityMonitor::DiskIo makeDiskIo(
    const std::map<std::uint64_t, ActivityMonitor::DiskIo>& diskIoById,
    const std::unordered_map<std::uint64_t, std::pair<std::uint64_t, std::uint64_t>>&
        previousOperationsById,
    const std::chrono::milliseconds elapsed,
    const DiskStatsFields& fields)
{
    const auto id = diskId(fields.majorNumber, fields.minorNumber);
    const auto previous = previousOperationsById.find(id);
    const auto [readOperationsPerSecond, writeOperationsPerSecond] =
        previous == previousOperationsById.end()
        ? std::pair{0.0, 0.0}
        : std::pair{operationsPerSecond(fields.readsCompleted, previous->second.first, elapsed),
              operationsPerSecond(fields.writesCompleted, previous->second.second, elapsed)};

    return {
        .device = fields.deviceName,
        .mountPoints = diskIoById.at(id).mountPoints,
        .readOperationsPerSecond = readOperationsPerSecond,
        .writeOperationsPerSecond = writeOperationsPerSecond,
    };
}

} // namespace

InterfaceStatisticsContext::InterfaceStatisticsContext():
    bytesReceived(0),
    bytesSent(0)
{
}

InterfaceStatisticsContext InterfaceStatisticsContext::create(const QString& name)
{
    InterfaceStatisticsContext ctx;

    ctx.interfaceName = name.toUtf8().toStdString();
    ctx.macAddress = nx::utils::MacAddress(QLatin1String(
        readFileContents(QString::fromLatin1("/sys/class/net/%1/address").arg(name))));

    int sysType = readFileContents(nx::format("/sys/class/net/%1/type").arg(name)).toInt();
    switch (sysType)
    {
        case ARPHRD_LOOPBACK:
            ctx.type = ActivityMonitor::LoopbackInterface;
            break;
        case ARPHRD_TUNNEL:
        case ARPHRD_TUNNEL6:
        case ARPHRD_IPDDP:
        case ARPHRD_IPGRE:
            ctx.type = ActivityMonitor::VirtualInterface;
            break;
        default:
            ctx.type = ActivityMonitor::PhysicalInterface;
    }

    return ctx;
}

void InterfaceStatisticsContext::update(int64_t elapsed)
{
    // Used, if interface speed cannot be read (noticed on vmware).
    static const int kDefaultInterfaceSpeedMpbs = 1000;
    static const int kMsPerSec = 1000;
    const QString name = QString::fromStdString(interfaceName);

    bytesPerSecMax = readFileContents(nx::format("/sys/class/net/%1/speed").arg(name)).toInt()
        * BYTES_PER_MB / CHAR_BIT;
    if (!bytesPerSecMax)
    {
        bytesPerSecMax = kDefaultInterfaceSpeedMpbs * 1024 * 1024 / CHAR_BIT;
        NX_DEBUG(NX_SCOPE_TAG, "Failed to get NIC speed, assuming 1Gbps"); // Noticed on vmware.
    }

    const uint64_t rx_bytes =
        readFileContents(nx::format("/sys/class/net/%1/statistics/rx_bytes").arg(name))
            .toULongLong();
    if (bytesReceived > 0)
    {
        auto currentValue = static_cast<int64_t>(rx_bytes - bytesReceived) * kMsPerSec / elapsed;
        bytesPerSecIn = exponentialSmoothing(currentValue, bytesPerSecIn);
    }
    bytesReceived = rx_bytes;

    const uint64_t tx_bytes =
        readFileContents(nx::format("/sys/class/net/%1/statistics/tx_bytes").arg(name))
            .toULongLong();
    if (bytesSent > 0)
    {
        auto currentValue = static_cast<int64_t>(tx_bytes - bytesSent) * kMsPerSec / elapsed;
        bytesPerSecOut = exponentialSmoothing(currentValue, bytesPerSecOut);
    }
    bytesSent = tx_bytes;
}

LinuxMonitor::Private::Private():
    prevCPUTimeTotal(-1),
    prevCPUTimeIdle(-1),
    m_networkStatCalcTimer(nx::utils::ElapsedTimerState::started),
    m_hddStatCalcTimer(nx::utils::ElapsedTimerState::started),
    m_diskIoStatCalcTimer(nx::utils::ElapsedTimerState::started),
    m_lastPartitionsUpdateTime(0),
    m_previousProcessElapsedClocks(-1)
{
    memset(&m_lastDiskUsageUpdateTime, 0, sizeof(m_lastDiskUsageUpdateTime));
    memset(&m_previousProcessTimes, 0, sizeof(m_previousProcessTimes));
}

double LinuxMonitor::Private::thisProcessCpuUsage()
{
    struct tms currentProcessTimes{};
    const clock_t currentProcessElapsedClocks = times(&currentProcessTimes);
    if (currentProcessElapsedClocks == -1)
    {
        NX_DEBUG(this, "Can't calculate process CPU usage, syscall times() failed: %1",
            SystemError::getLastOSErrorText());
        return 0;
    }

    if (m_previousProcessElapsedClocks == -1)
    {
        m_previousProcessElapsedClocks = currentProcessElapsedClocks;
        m_previousProcessTimes = currentProcessTimes;
        return 0;
    }

    const clock_t systemTimeDelta = currentProcessTimes.tms_stime - m_previousProcessTimes.tms_stime;
    const clock_t userTimeDelta = currentProcessTimes.tms_utime - m_previousProcessTimes.tms_utime;
    const clock_t elapsedDelta = currentProcessElapsedClocks - m_previousProcessElapsedClocks;
    if (!NX_ASSERT(elapsedDelta > 0, "This method can't be called more often than 1 / _SC_CLK_TCK"))
        return 0;

    const double cpuUsage = (systemTimeDelta + userTimeDelta) / static_cast<double>(elapsedDelta);

    m_previousProcessElapsedClocks = currentProcessElapsedClocks;
    m_previousProcessTimes = currentProcessTimes;
    return cpuUsage;
}

std::vector<LinuxMonitor::Private::HddLoad> LinuxMonitor::Private::totalHddLoad()
{
    std::vector<HddLoad> result;

    updatePartitions();

    const std::int64_t elapsed = m_hddStatCalcTimer.elapsed().count();
    if( elapsed == 0 )
    {
        m_hddStatCalcTimer.restart();
        return zeroLoad();
    }

    /* Reading current disk statistics. */
    std::unique_ptr<FILE, decltype(&fclose)> file( fopen("/proc/diskstats", "r"), fclose );
    if( !file )
    {
        m_hddStatCalcTimer.restart();
        return zeroLoad();
    }

    std::unordered_map<int, unsigned int> diskTimeById;
    char line[MAX_LINE_LENGTH];
    while(fgets(line, MAX_LINE_LENGTH, file.get()) != nullptr) {
        DiskStatSys diskStat;
        memset(diskStat.device_name, 0, sizeof(diskStat.device_name));

        if(sscanf(line, "%u %u %s %u %u %u %u %u %u %u %u %u %u %u",
            &diskStat.major_num,
            &diskStat.minor_num,
            diskStat.device_name,
            &diskStat.num_reads,
            &diskStat.reads_merged,
            &diskStat.sectors_read,
            &diskStat.tot_read_ms,
            &diskStat.num_writes,
            &diskStat.writes_merged,
            &diskStat.sectors_written,
            &diskStat.tot_write_ms,
            &diskStat.cur_io_cnt,
            &diskStat.tot_io_ms,
            &diskStat.io_weighted_ms) != 14)
        {
            continue;
        }

        int id = calculateId(diskStat.major_num, diskStat.minor_num);
        if(!m_diskById.contains(id))
            continue; /* Not a partition. */
        const Hdd& hdd = m_diskById.at(id);

        diskTimeById[id] = diskStat.tot_io_ms;

        double load = 0.0;
        if(m_lastDiskTimeById.contains(id))
            load = static_cast<double>(diskStat.tot_io_ms - m_lastDiskTimeById.at(id)) / elapsed;

        result.push_back({.hdd = hdd, .load = load});
    }
    m_lastDiskTimeById = diskTimeById;

    m_hddStatCalcTimer.restart();

    return result;
}

std::vector<ActivityMonitor::DiskIo> LinuxMonitor::Private::totalDiskIo()
{
    if (!partitionsInfoProvider)
        return {};

    const std::map diskIoById = nx::ranges::fold_left(partitionsInfoProvider->partitionInfo(),
        std::map<std::uint64_t, DiskIo>{},
        [](auto result, const PartitionSpace& partition)
        {
            struct stat info{};
            const std::filesystem::path& path = partition.path;
            if (::stat(path.c_str(), &info) != 0)
            {
                const auto errorCode = errno; //< Preserve it before logging can alter it.
                NX_WARNING(NX_SCOPE_TAG,
                    "Unable to identify the block device for %1: %2",
                    path.string(),
                    SystemError::toString(errorCode));
                return result;
            }

            result[diskId(major(info.st_dev), minor(info.st_dev))].mountPoints.emplace_back(path);
            return result;
        });

    const std::expected contents = readTextFile("/proc/diskstats");
    if (!contents)
    {
        NX_WARNING(NX_SCOPE_TAG, "Unable to read /proc/diskstats: %1", contents.error().message());
        return {};
    }

    // One line per block device.
    const auto [diskStats, invalidLines] = std::string_view(*contents) | std::views::split('\n')
        | std::views::filter([](const auto& line) { return !std::ranges::empty(line); })
        | std::views::transform(nx::ranges::asStringView)
        | std::views::transform(DiskStatsFields::parse) | nx::actions::partitionSums;

    if (!invalidLines.empty())
    {
        NX_WARNING(NX_SCOPE_TAG,
            "Unable to parse %1 lines from /proc/diskstats: %2",
            invalidLines.size(),
            nx::utils::join(invalidLines, " | "));
    }

    // /proc/diskstats also lists devices without mount points, which are not reported.
    // TODO: #skolesnik Derive the mapping from /proc/self/mountinfo instead; the provider may be
    // stale.
    const std::vector reported = diskStats
        | std::views::filter([&diskIoById](const DiskStatsFields& fields)
            { return diskIoById.contains(diskId(fields.majorNumber, fields.minorNumber)); })
        | nx::ranges::to<std::vector>();

    const std::vector result = reported
        | std::views::transform(std::bind_front(makeDiskIo,
            std::cref(diskIoById),
            std::cref(m_lastDiskOperationsById),
            m_diskIoStatCalcTimer.elapsed()))
        | nx::ranges::to<std::vector>();

    m_lastDiskOperationsById = reported
        | std::views::transform(
            [](const DiskStatsFields& fields)
            {
                return std::pair{diskId(fields.majorNumber, fields.minorNumber),
                    std::pair{fields.readsCompleted, fields.writesCompleted}};
            })
        | nx::ranges::to<std::unordered_map>();
    m_diskIoStatCalcTimer.restart();

    return result;
}

std::vector<ActivityMonitor::NetworkLoad> LinuxMonitor::Private::totalNetworkLoad()
{
    calcNetworkStat();

    std::vector<ActivityMonitor::NetworkLoad> netStat;
    for (auto it = m_ifNameToStatistics.begin(); it != m_ifNameToStatistics.end(); ++it)
        netStat.push_back(it->second);

    return netStat;
}

void LinuxMonitor::Private::updatePartitions()
{
    const time_t time = ::time(nullptr);

    if(!m_diskById.empty() && time - m_lastPartitionsUpdateTime <= kPartitionListExpireTimeoutSec)
        return;
    m_lastPartitionsUpdateTime = time;

    std::unique_ptr<FILE, decltype(&fclose)> file( fopen("/proc/partitions", "r"), fclose );
    if(!file)
        return;

    m_diskById.clear();
    char line[MAX_LINE_LENGTH];
    //!map<devname, pair<major, minor> >
    std::map<QString, std::pair<unsigned int, unsigned int> > allPartitions;
    for(int i = 0; fgets(line, MAX_LINE_LENGTH, file.get()) != nullptr; ++i) {
        if(i == 0)
            continue; /* Skip header. */

        unsigned int majorNumber = 0, minorNumber = 0, numBlocks = 0;
        char devName[MAX_LINE_LENGTH];
        if(sscanf(line, "%u %u %u %s", &majorNumber, &minorNumber, &numBlocks, devName) != 4)
            continue; /* Skip unrecognized lines. */

        QString devNameString = QString::fromUtf8(devName);
        //if(devNameString.isEmpty() || devNameString[devNameString.size() - 1].isDigit())
        if( devNameString.isEmpty() )
            continue; /* Not a physical drive. */

        allPartitions[devNameString] = std::make_pair( majorNumber, minorNumber );
    }

    for( const auto& val: allPartitions )
    {
        const QString& devName = val.first;
        const auto major = val.second.first;
        const auto minor = val.second.second;

        if( devName[devName.size()-1].isDigit() )
        {
            //checking for presense of sub-partitions
            auto it = allPartitions.upper_bound( devName );
            if( it == allPartitions.end() || !it->first.startsWith(devName) )
                continue;   //partition devName does not have sub partitions, considering it not a physical device
            if( it->first[devName.size()].isDigit() )
                continue;   //the fallowing record is just a 2 digit patition number
        }

        const int id = calculateId(int(major), int(minor));
        m_diskById[id] = {
            .id = id,
            .name = devName.toUtf8().toStdString(),
            .partitions = devName.toUtf8().toStdString(),
        };
    }

    // TODO: #rvasilenko Read network drives?
}

int LinuxMonitor::Private::calculateId(int majorNumber, int minorNumber)
{
    return (majorNumber << 16) + minorNumber;
}

std::vector<LinuxMonitor::Private::HddLoad> LinuxMonitor::Private::zeroLoad() const
{
    std::vector<HddLoad> result;
    for (const auto& [id, hdd]: m_diskById)
        result.push_back({.hdd = hdd, .load = 0.0});
    return result;
}

void LinuxMonitor::Private::calcNetworkStat()
{
    const int64_t elapsedMs = m_networkStatCalcTimer.elapsed().count();
    if (elapsedMs == 0)
        return;

    const QDir sysClassNet("/sys/class/net/");
    if (!sysClassNet.exists())
    {
        NX_WARNING(this, "No /sys/class/net/. Cannot read network statistics");
        return;
    }

    auto intefaceList = sysClassNet.entryList(QStringList(), QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& interfaceName: intefaceList)
    {
        InterfaceStatisticsContext ctx;
        const auto it = m_ifNameToStatistics.find(interfaceName);
        if (it != m_ifNameToStatistics.end())
            ctx = it->second;
        else
            ctx = InterfaceStatisticsContext::create(interfaceName);

        ctx.update(elapsedMs);
        m_ifNameToStatistics[interfaceName] = ctx;
    }

    m_networkStatCalcTimer.restart();
}

} // namespace nx::monitoring
