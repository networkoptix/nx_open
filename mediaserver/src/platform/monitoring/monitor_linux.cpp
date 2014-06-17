#include "monitor_linux.h"

#include <iostream>
#include <map>
#include <set>

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

#include <utils/common/log.h>


static const int BYTES_PER_MB = 1024*1024;
static const int NET_STAT_CALCULATION_PERIOD_SEC = 10;
static const int MS_PER_SEC = 1000;
//!used, if interface speed cannot be read (noticed on vmware)
static const int DEFAULT_INTERFACE_SPEED_MBPS = 1000;
static const size_t MAX_LINE_LENGTH = 512;

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

    typedef QnPlatformMonitor::Hdd Hdd;
    typedef QnPlatformMonitor::HddLoad HddLoad;

    //!Timeout (in seconds) during which partition list expires (needed to detect mounts/umounts)
    static const time_t PARTITION_LIST_EXPIRE_TIMEOUT_SEC = 60;
    //!Disk usage is evaluated as average on \a APPROXIMATION_VALUES_NUMBER prev values
    static const unsigned int APPROXIMATION_VALUES_NUMBER = 3;


    QnLinuxMonitorPrivate()
    :
        lastPartitionsUpdateTime(0)
    {
        memset(&lastDiskUsageUpdateTime, 0, sizeof(lastDiskUsageUpdateTime));
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
        FILE *file = fopen("/proc/diskstats", "r");
        if(!file)
        {
            m_hddStatCalcTimer.restart();
            return zeroLoad();
        }

        QHash<int, unsigned int> diskTimeById;
        char line[MAX_LINE_LENGTH];
        while(fgets(line, MAX_LINE_LENGTH, file) != NULL) {
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

        fclose(file);
        m_hddStatCalcTimer.restart();

        return result;
    }

    QList<QnPlatformMonitor::NetworkLoad> totalNetworkLoad()
    {
        calcNetworkStat();

        QList<QnPlatformMonitor::NetworkLoad> netStat;
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

        FILE *file = fopen("/proc/partitions", "r");
        if(!file)
            return;

        diskById.clear();
        char line[MAX_LINE_LENGTH];
        //!map<devname, pair<major, minor> >
        std::map<QString, std::pair<unsigned int, unsigned int> > allPartitions;
        for(int i = 0; fgets(line, MAX_LINE_LENGTH, file) != NULL; ++i) {
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

        fclose(file);

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
                NX_LOG( QString::fromLatin1("No /sys/class/net/. Cannot read network statistics"), cl_logWARNING );
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
                    ctx.macAddress = QnMacAddress( QLatin1String(readFileContents( QString::fromLatin1("/sys/class/net/%1/address").arg(interfaceName) )) );
                    int sysType = readFileContents( QString::fromLatin1("/sys/class/net/%1/type").arg(interfaceName) ).toInt();
                    switch( sysType )
                    {
                        case ARPHRD_LOOPBACK:
                            ctx.type = QnPlatformMonitor::LoopbackInterface;
                            break;

                        case ARPHRD_TUNNEL:
                        case ARPHRD_TUNNEL6:
                        case ARPHRD_IPDDP:
                        case ARPHRD_IPGRE:
                            ctx.type = QnPlatformMonitor::VirtualInterface;
                            break;

                        default:
                            ctx.type = QnPlatformMonitor::PhysicalInterface;
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
        public QnPlatformMonitor::NetworkLoad
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

private:
    Q_DECLARE_PUBLIC(QnLinuxMonitor);
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

QList<QnPlatformMonitor::HddLoad> QnLinuxMonitor::totalHddLoad()
{
    return d_ptr->totalHddLoad();
}

QList<QnPlatformMonitor::NetworkLoad> QnLinuxMonitor::totalNetworkLoad()
{
    return d_ptr->totalNetworkLoad();
}

QList<QnPlatformMonitor::PartitionSpace> QnLinuxMonitor::totalPartitionSpaceInfo()
{
    QList<QnPlatformMonitor::PartitionSpace> partitions = base_type::totalPartitionSpaceInfo();
    //filtering driectories, mounted to the same device

    //map<device, path>
    std::map<QString, QString> deviceToPath;
    std::set<QString> mountPointsToIgnore;
    FILE* file = fopen("/proc/mounts", "r");
    if( !file )
        return partitions;

    char line[MAX_LINE_LENGTH];
    for( int i = 0; fgets(line, MAX_LINE_LENGTH, file) != NULL; ++i )
    {
        if( i == 0 )
            continue; /* Skip header. */

        char cDevName[MAX_LINE_LENGTH];
        char cPath[MAX_LINE_LENGTH];
        if( sscanf(line, "%s %s ", cDevName, cPath) != 2 )
            continue; /* Skip unrecognized lines. */

        const QString& devName = QString::fromUtf8(cDevName);
        const QString& path = QString::fromUtf8(cPath);

        auto p = deviceToPath.insert( std::make_pair(devName, path) );
        if( !p.second )
        {
            //device has mutiple mount points
            if( path.length() < p.first->second.length() )
            {
                mountPointsToIgnore.insert( p.first->second );
                p.first->second = path; //selecting shortest mount point
            }
            else
            {
                mountPointsToIgnore.insert( path );
            }
        }
    }

    const auto partitionsEnd = partitions.end();
    for( auto it = partitions.begin(); it != partitionsEnd; )
    {
        if( mountPointsToIgnore.find( it->path ) != mountPointsToIgnore.end() )
            it = partitions.erase(it);
        else
            ++it;
    }
    return partitions;
}
