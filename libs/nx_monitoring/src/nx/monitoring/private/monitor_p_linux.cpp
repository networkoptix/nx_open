// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "monitor_p_linux.h"

#include <iostream>
#include <map>
#include <set>
#include <chrono>
#include <thread>

#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QHash>

#include <time.h>
#include <signal.h>
#include <errno.h>
#include <net/if_arp.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <nx/utils/log/log.h>
#include <nx/utils/concurrent.h>
#include <nx/utils/mac_address.h>


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

} // namespace

InterfaceStatisticsContext::InterfaceStatisticsContext():
    bytesReceived(0),
    bytesSent(0)
{
}

InterfaceStatisticsContext InterfaceStatisticsContext::create(const QString& name)
{
    InterfaceStatisticsContext ctx;

    ctx.interfaceName = name;
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

    bytesPerSecMax = readFileContents(
        nx::format("/sys/class/net/%1/speed").arg(interfaceName)).toInt() * BYTES_PER_MB / CHAR_BIT;
    if (!bytesPerSecMax)
    {
        bytesPerSecMax = kDefaultInterfaceSpeedMpbs * 1024 * 1024 / CHAR_BIT;
        NX_DEBUG(NX_SCOPE_TAG, "Failed to get NIC speed, assuming 1Gbps"); // Noticed on vmware.
    }

    const uint64_t rx_bytes = readFileContents(
        nx::format("/sys/class/net/%1/statistics/rx_bytes").arg(interfaceName)).toULongLong();
    if (bytesReceived > 0)
    {
        auto currentValue = static_cast<int64_t>(rx_bytes - bytesReceived) * kMsPerSec / elapsed;
        bytesPerSecIn = exponentialSmoothing(currentValue, bytesPerSecIn);
    }
    bytesReceived = rx_bytes;

    const uint64_t tx_bytes = readFileContents(
        nx::format("/sys/class/net/%1/statistics/tx_bytes").arg(interfaceName)).toULongLong();
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
    m_lastPartitionsUpdateTime(0),
    m_previousProcessElapsedClocks(-1)
{
    memset(&m_lastDiskUsageUpdateTime, 0, sizeof(m_lastDiskUsageUpdateTime));
    memset(&m_previousProcessTimes, 0, sizeof(m_previousProcessTimes));
}

qreal LinuxMonitor::Private::thisProcessCpuUsage()
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

    const qreal cpuUsage = (systemTimeDelta + userTimeDelta) / static_cast<qreal>(elapsedDelta);

    m_previousProcessElapsedClocks = currentProcessElapsedClocks;
    m_previousProcessTimes = currentProcessTimes;
    return cpuUsage;
}

QList<LinuxMonitor::Private::HddLoad> LinuxMonitor::Private::totalHddLoad()
{
    QList<HddLoad> result;

    updatePartitions();

    const qint64 elapsed = m_hddStatCalcTimer.elapsed().count();
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

    QHash<int, unsigned int> diskTimeById;
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
        const Hdd &hdd = m_diskById[id];

        diskTimeById[id] = diskStat.tot_io_ms;

        qreal load = 0.0;
        if(m_lastDiskTimeById.contains(id))
            load = static_cast<qreal>(diskStat.tot_io_ms - m_lastDiskTimeById[id]) / elapsed;

        result.push_back(HddLoad(hdd, load));
    }
    m_lastDiskTimeById = diskTimeById;

    m_hddStatCalcTimer.restart();

    return result;
}

QList<ActivityMonitor::NetworkLoad> LinuxMonitor::Private::totalNetworkLoad()
{
    calcNetworkStat();

    QList<ActivityMonitor::NetworkLoad> netStat;
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
        m_diskById[id] = Hdd( id, devName, devName );
    }

    // TODO: #rvasilenko Read network drives?
}

int LinuxMonitor::Private::calculateId(int majorNumber, int minorNumber)
{
    return (majorNumber << 16) + minorNumber;
}

QList<LinuxMonitor::Private::HddLoad> LinuxMonitor::Private::zeroLoad() const
{
    QList<HddLoad> result;
    foreach(const Hdd &hdd, m_diskById)
        result.push_back(HddLoad(hdd, 0.0));
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
