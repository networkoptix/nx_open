#include "monitor_linux.h"
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <chrono>
#include <thread>

#include <boost/optional.hpp>

#include <QtCore/QElapsedTimer>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QHash>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>

#include <time.h>
#include <signal.h>
#include <errno.h>
#include <net/if_arp.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <nx/utils/log/log.h>
#include <nx/utils/concurrent.h>
#include <media_server/media_server_module.h>
#include <nx/vms/server/root_fs.h>
#include <nx/utils/mac_address.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/server/fs/partitions/read_partitions_linux.h>
#include <nx/vms/server/fs/partitions/partitions_information_provider_linux.h>
#include <platform/hardware_information.h>

static const int BYTES_PER_MB = 1024*1024;
//static const int NET_STAT_CALCULATION_PERIOD_SEC = 10;
static const int MS_PER_SEC = 1000;
//!used, if interface speed cannot be read (noticed on vmware)
static const int DEFAULT_INTERFACE_SPEED_MBPS = 1000;
static const size_t MAX_LINE_LENGTH = 512;

using PlatformMonitor = nx::vms::server::PlatformMonitor;

// -------------------------------------------------------------------------- //
// QnLinuxMonitorPrivate
// -------------------------------------------------------------------------- //
class QnLinuxMonitorPrivate
{
public:
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

    typedef PlatformMonitor::Hdd Hdd;
    typedef PlatformMonitor::HddLoad HddLoad;

    //!Timeout (in seconds) during which partition list expires (needed to detect mounts/umounts)
    static const time_t PARTITION_LIST_EXPIRE_TIMEOUT_SEC = 60;
    //!Disk usage is evaluated as average on \a APPROXIMATION_VALUES_NUMBER prev values
    static const unsigned int APPROXIMATION_VALUES_NUMBER = 3;

    int64_t prevCPUTimeTotal;
    int64_t prevCPUTimeIdle;


    QnLinuxMonitorPrivate()
    :
        prevCPUTimeTotal(-1),
        prevCPUTimeIdle(-1),
        lastPartitionsUpdateTime(0)
    {
        memset(&lastDiskUsageUpdateTime, 0, sizeof(lastDiskUsageUpdateTime));

        m_hddStatCalcTimer.start();
        m_networkStatCalcTimer.start();
    }

    virtual ~QnLinuxMonitorPrivate()
    {
    }

    QList<HddLoad> totalHddLoad() {
        QList<HddLoad> result;

        updatePartitions();

        const qint64 elapsed = m_hddStatCalcTimer.isValid() ? m_hddStatCalcTimer.elapsed() : 0;
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
        while(fgets(line, MAX_LINE_LENGTH, file.get()) != NULL) {
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
            if(!diskById.contains(id))
                continue; /* Not a partition. */
            const Hdd &hdd = diskById[id];

            diskTimeById[id] = diskStat.tot_io_ms;

            qreal load = 0.0;
            if(lastDiskTimeById.contains(id))
                load = static_cast<qreal>(diskStat.tot_io_ms - lastDiskTimeById[id]) / elapsed;

            result.push_back(HddLoad(hdd, load));
        }
        lastDiskTimeById = diskTimeById;

        m_hddStatCalcTimer.restart();

        return result;
    }

    QList<PlatformMonitor::NetworkLoad> totalNetworkLoad()
    {
        calcNetworkStat();

        QList<PlatformMonitor::NetworkLoad> netStat;
        for( std::map<QString, InterfaceStatisticsContext>::const_iterator
            it = m_ifNameToStatistics.begin();
            it != m_ifNameToStatistics.end();
            ++it )
        {
            netStat.push_back( it->second );
        }

        return netStat;
    }

    void updatePartitions() {
        const time_t time = ::time(NULL);

        if(!diskById.empty() && time - lastPartitionsUpdateTime <= PARTITION_LIST_EXPIRE_TIMEOUT_SEC)
            return;
        lastPartitionsUpdateTime = time;

        std::unique_ptr<FILE, decltype(&fclose)> file( fopen("/proc/partitions", "r"), fclose );
        if(!file)
            return;

        diskById.clear();
        char line[MAX_LINE_LENGTH];
        //!map<devname, pair<major, minor> >
        std::map<QString, std::pair<unsigned int, unsigned int> > allPartitions;
        for(int i = 0; fgets(line, MAX_LINE_LENGTH, file.get()) != NULL; ++i) {
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
            }

            const int id = calculateId( major, minor );
            diskById[id] = Hdd( id, devName, devName );
        }

        // TODO: #Elric read network drives?
    }

    int calculateId(int majorNumber, int minorNumber) {
        return (majorNumber << 16) + minorNumber;
    }

    QList<HddLoad> zeroLoad() const {
        QList<HddLoad> result;
        foreach(const Hdd &hdd, diskById)
            result.push_back(HddLoad(hdd, 0.0));
        return result;
    }

protected:
    static QByteArray readFileContents( const QString& filePath )
    {
        QFile statFile( filePath );
        if( !statFile.open( QIODevice::ReadOnly ) )
            return QByteArray();
        return statFile.readAll().trimmed();
    }

    void calcNetworkStat()
    {
        const qint64 elapsed = m_networkStatCalcTimer.isValid() ? m_networkStatCalcTimer.elapsed() : 0;
        if( elapsed > 0 )
        {
            //listing /sys/class/net/
            QDir sysClassNet( QLatin1String("/sys/class/net/") );
            if( !sysClassNet.exists() )
            {
                NX_WARNING (this, QString::fromLatin1("No /sys/class/net/. Cannot read network statistics"));
                return;
            }

            QStringList intefaceList = sysClassNet.entryList( QStringList(), QDir::Dirs | QDir::NoDotAndDotDot );
            foreach( const QString& interfaceName, intefaceList )
            {
                InterfaceStatisticsContext ctx;
                {
                    std::map<QString, InterfaceStatisticsContext>::iterator it = m_ifNameToStatistics.find(interfaceName);
                    if( it != m_ifNameToStatistics.end() )
                        ctx = it->second;
                }

                if( ctx.interfaceName.isEmpty() )
                {
                    ctx.interfaceName = interfaceName;
                    ctx.macAddress = nx::utils::MacAddress( QLatin1String(readFileContents( QString::fromLatin1("/sys/class/net/%1/address").arg(interfaceName) )) );
                    int sysType = readFileContents( QString::fromLatin1("/sys/class/net/%1/type").arg(interfaceName) ).toInt();
                    switch( sysType )
                    {
                        case ARPHRD_LOOPBACK:
                            ctx.type = PlatformMonitor::LoopbackInterface;
                            break;

                        case ARPHRD_TUNNEL:
                        case ARPHRD_TUNNEL6:
                        case ARPHRD_IPDDP:
                        case ARPHRD_IPGRE:
                            ctx.type = PlatformMonitor::VirtualInterface;
                            break;

                        default:
                            ctx.type = PlatformMonitor::PhysicalInterface;
                    }
                }

                static const double NEW_VALUE_WEIGHT=0.7;

                ctx.bytesPerSecMax = readFileContents( QString::fromLatin1("/sys/class/net/%1/speed").arg(interfaceName) ).toInt() * BYTES_PER_MB / CHAR_BIT;
                if( !ctx.bytesPerSecMax )
                    ctx.bytesPerSecMax = DEFAULT_INTERFACE_SPEED_MBPS * 1024 * 1024 / CHAR_BIT; //if unknown, assuming 1Gbps
                const uint64_t rx_bytes = readFileContents( QString::fromLatin1("/sys/class/net/%1/statistics/rx_bytes").arg(interfaceName) ).toLongLong();
                if( ctx.bytesReceived > 0 )
                    ctx.bytesPerSecIn = ctx.bytesPerSecIn * (1-NEW_VALUE_WEIGHT) + ((rx_bytes - ctx.bytesReceived) * MS_PER_SEC / elapsed) * NEW_VALUE_WEIGHT;
                ctx.bytesReceived = rx_bytes;
                const uint64_t tx_bytes = readFileContents( QString::fromLatin1("/sys/class/net/%1/statistics/tx_bytes").arg(interfaceName) ).toLongLong();
                if( ctx.bytesSent > 0 )
                    ctx.bytesPerSecOut = ctx.bytesPerSecOut * (1-NEW_VALUE_WEIGHT) + ((tx_bytes - ctx.bytesSent) * MS_PER_SEC / elapsed) * NEW_VALUE_WEIGHT;
                ctx.bytesSent = tx_bytes;

                //std::cout<<"Interface "<<interfaceName.toLatin1().constData()<<" (mac "<<ctx.macAddress.toString().toLatin1().constData()<<") statistics: "
                //    "speed "<<ctx.bytesPerSecMax<<", rx_bytes "<<rx_bytes<<", tx_bytes "<<tx_bytes<<", "
                //    "bytesPerSecIn "<<ctx.bytesPerSecIn<<", bytesPerSecOut "<<ctx.bytesPerSecOut<<"\n";

                m_ifNameToStatistics[interfaceName] = ctx;
            }
        }

        m_networkStatCalcTimer.restart();
    }

private:
    class InterfaceStatisticsContext
    :
        public PlatformMonitor::NetworkLoad
    {
    public:
        uint64_t bytesReceived;
        uint64_t bytesSent;

        InterfaceStatisticsContext()
        :
            bytesReceived( 0 ),
            bytesSent( 0 )
        {
        }
    };

    QHash<int, Hdd> diskById;
    QHash<int, unsigned int> lastDiskTimeById;
    std::map<QString, InterfaceStatisticsContext> m_ifNameToStatistics;
    QElapsedTimer m_networkStatCalcTimer;
    QElapsedTimer m_hddStatCalcTimer;

    time_t lastPartitionsUpdateTime;
    struct timespec lastDiskUsageUpdateTime;
    std::unique_ptr<nx::vms::server::fs::PartitionsInformationProvider> partitionsInfoProvider;

private:
    Q_DECLARE_PUBLIC(QnLinuxMonitor)
    QnLinuxMonitor *q_ptr;
};

// -------------------------------------------------------------------------- //
// QnLinuxMonitor
// -------------------------------------------------------------------------- //
QnLinuxMonitor::QnLinuxMonitor(QObject *parent):
    base_type(parent),
    d_ptr(new QnLinuxMonitorPrivate())
{
    d_ptr->q_ptr = this;
}

QnLinuxMonitor::~QnLinuxMonitor() {
    return;
}

qreal QnLinuxMonitor::totalCpuUsage()
{
    std::unique_ptr<FILE, decltype(&fclose)> file( fopen("/proc/stat", "r"), fclose );
    if(!file)
        return 0;

    int64_t cpuTimeTotal = 0;
    int64_t cpuTimeIdle = d_ptr->prevCPUTimeIdle;
    char line[MAX_LINE_LENGTH];
    for( int i = 0; fgets(line, MAX_LINE_LENGTH, file.get()) != NULL; ++i )
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

    if( d_ptr->prevCPUTimeTotal == -1 )
        d_ptr->prevCPUTimeTotal = cpuTimeTotal;
    if( d_ptr->prevCPUTimeIdle == -1 )
        d_ptr->prevCPUTimeIdle = cpuTimeIdle;

    //calculating CPU load
    const int64_t cpuTimeTotalDiff = cpuTimeTotal - d_ptr->prevCPUTimeTotal;
    const int64_t cpuTimeIdleDiff = cpuTimeIdle - d_ptr->prevCPUTimeIdle;
    d_ptr->prevCPUTimeIdle = cpuTimeIdle;
    d_ptr->prevCPUTimeTotal = cpuTimeTotal;
    if( cpuTimeTotalDiff <= 0 )
        return 0;
    return 1.0 - cpuTimeIdleDiff / (qreal)cpuTimeTotalDiff;
}

quint64 QnLinuxMonitor::totalRamUsageBytes()
{
    std::unique_ptr<FILE, decltype(&fclose)> file( fopen("/proc/meminfo", "r"), fclose );
    if(!file)
        return 0;

    char line[MAX_LINE_LENGTH];
    boost::optional<uint64_t> memTotalKB;
    boost::optional<uint64_t> memFreeKB;
    boost::optional<uint64_t> memCachedKB;
    for( int i = 0; fgets(line, MAX_LINE_LENGTH, file.get()) != NULL; ++i )
    {
        const size_t length = strlen(line);

        char* sepPos = strchr( line, ':' );
        if( sepPos == NULL )
            continue;
        char* valStart = std::find_if( sepPos+1, line+length, [](char ch){ return ch != ' '; } );
        if( valStart == line+length )
           continue;    //could not find value
        char* valEnd = strchr( valStart, ' ' );
        if( valEnd )
            *valEnd = '\0';
        if( strncmp( line, "MemTotal", sepPos-line ) == 0 )
            memTotalKB = atoll( valStart );
        else if( strncmp( line, "MemFree", sepPos-line ) == 0 )
            memFreeKB = atoll( valStart );
        else if( strncmp( line, "Cached", sepPos-line ) == 0 )
            memCachedKB = atoll( valStart );

        if( memTotalKB && memFreeKB && memCachedKB )
            break;
    }

    if( !memTotalKB || !memFreeKB || !memCachedKB )
        return 0;

    // Protecting from zero-memory-size error in /proc/meminfo. This situation is not possible in
    // real life, so nothing to worry about.
    if (memTotalKB.get() == 0)
        return 0;

    return (memTotalKB.get() - memFreeKB.get() - memCachedKB.get()) * 1024;
}

quint64 QnLinuxMonitor::thisProcessRamUsageBytes()
{
    const char* kStatmFilename = "/proc/self/statm";
    std::ifstream statm(kStatmFilename);
    if (!statm.is_open())
    {
        NX_DEBUG(this, "Failed to open file %1: %2", kStatmFilename, strerror(errno));
        return 0;
    }

    statm.ignore(255, ' '); // Skip first value (size)
    long rss_in_pages;
    statm >> rss_in_pages;

    return rss_in_pages * sysconf(_SC_PAGE_SIZE);
}

QList<PlatformMonitor::HddLoad> QnLinuxMonitor::totalHddLoad()
{
    return d_ptr->totalHddLoad();
}

QList<PlatformMonitor::NetworkLoad> QnLinuxMonitor::totalNetworkLoad()
{
    return d_ptr->totalNetworkLoad();
}

static PlatformMonitor::PartitionType fsNameToType( const QString& fsName )
{
    if( fsName == "sysfs" )
        return PlatformMonitor::UnknownPartition;
    else if( fsName == "rootfs" )
        return PlatformMonitor::LocalDiskPartition;
    else if( fsName == "ramfs" )
        return PlatformMonitor::RamDiskPartition;
    else if( fsName == "bdev" )
        return PlatformMonitor::UnknownPartition;
    else if( fsName == "proc" )
        return PlatformMonitor::UnknownPartition;
    else if( fsName == "cgroup" )
        return PlatformMonitor::UnknownPartition;
    else if( fsName == "cpuset" )
        return PlatformMonitor::UnknownPartition;
    else if( fsName == "tmpfs" )
        return PlatformMonitor::RamDiskPartition;
    else if( fsName == "devtmpfs" )
        return PlatformMonitor::UnknownPartition;
    else if( fsName == "debugfs" )
        return PlatformMonitor::UnknownPartition;
    else if( fsName == "securityfs" )
        return PlatformMonitor::UnknownPartition;
    else if( fsName == "sockfs" )
        return PlatformMonitor::UnknownPartition;
    else if( fsName == "pipefs" )
        return PlatformMonitor::UnknownPartition;
    else if( fsName == "anon_inodefs" )
        return PlatformMonitor::UnknownPartition;
    else if( fsName == "devpts" )
        return PlatformMonitor::UnknownPartition;
    else if( fsName == "ext3" )
        return PlatformMonitor::LocalDiskPartition;
    else if( fsName == "ext2" )
        return PlatformMonitor::LocalDiskPartition;
    else if( fsName == "ext4" )
        return PlatformMonitor::LocalDiskPartition;
    else if( fsName == "exfat" )
        return PlatformMonitor::LocalDiskPartition;
    else if( fsName == "hugetlbfs" )
        return PlatformMonitor::UnknownPartition;
    else if( fsName == "vfat" )
        return PlatformMonitor::LocalDiskPartition;
    else if( fsName == "ecryptfs" )
        return PlatformMonitor::LocalDiskPartition;
    else if( fsName == "fuseblk" )  //NTFS
        return PlatformMonitor::LocalDiskPartition;
    else if( fsName == "fuse" )
        return PlatformMonitor::LocalDiskPartition;
    else if( fsName == "fusectl" )
        return PlatformMonitor::LocalDiskPartition;
    else if( fsName == "xfs" )
        return PlatformMonitor::LocalDiskPartition;
    else if( fsName == "pstore" )
        return PlatformMonitor::UnknownPartition;
    else if( fsName == "mqueue" )
        return PlatformMonitor::UnknownPartition;
    else if( fsName == "rpc_pipefs" )
        return PlatformMonitor::UnknownPartition;
    else if( fsName == "nfs" )
        return PlatformMonitor::NetworkPartition;
    else if( fsName == "nfs4" )
        return PlatformMonitor::NetworkPartition;
    else if( fsName == "nfsd" )
        return PlatformMonitor::NetworkPartition;
    else if( fsName == "cifs" )
        return PlatformMonitor::NetworkPartition;
    else if( fsName == "fuse.osxfs" ) // Mounted volumes when Docker host is macOS.
        return PlatformMonitor::LocalDiskPartition;
    else
        return PlatformMonitor::UnknownPartition;
}

/*!
    \note If same device mounted to multiple points, returns mount point with shortest name
*/
QList<PlatformMonitor::PartitionSpace> QnLinuxMonitor::totalPartitionSpaceInfo()
{
    std::list<nx::vms::server::fs::PartitionInfo> partitions;
    QList<PlatformMonitor::PartitionSpace> result;

    const auto errCode = nx::vms::server::fs::readPartitionsInformation(
        d_ptr->partitionsInfoProvider.get(),
        &partitions);
    if (errCode != SystemError::noError)
    {
        NX_WARNING(this, "Unable to read partitions: %1", SystemError::toString(errCode));
        return result;
    }

    for (const auto& data: partitions)
    {
        PlatformMonitor::PartitionSpace partitionInfo;
        partitionInfo.devName = data.devName;
        partitionInfo.path = data.path;
        partitionInfo.type = data.isUsb
            ? PlatformMonitor::RemovableDiskPartition : fsNameToType(data.fsName);
        partitionInfo.sizeBytes = data.sizeBytes;
        partitionInfo.freeBytes = data.freeBytes;
        result.push_back(partitionInfo);
    }

    return result;
}

void QnLinuxMonitor::setServerModule(QnMediaServerModule* serverModule)
{
    d_ptr->partitionsInfoProvider.reset(
        new nx::vms::server::fs::PartitionsInformationProvider(serverModule->rootFileSystem()));
}

int QnLinuxMonitor::thisProcessThreads()
{
    const QString path("/proc/self/task");
    return QDir(path).entryList(QDir::Dirs | QDir::NoDotAndDotDot).count();
}
