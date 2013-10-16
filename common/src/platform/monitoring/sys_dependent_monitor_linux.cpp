#include <qglobal.h>

#ifdef Q_OS_LINUX

#include "sys_dependent_monitor.h"

#include <iostream>
#include <map>

#include <QtCore/QElapsedTimer>
#include <QtCore/QFile>
#include <QtCore/QHash>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>

#include <time.h>
#include <signal.h>
#include <errno.h>
#include <net/if_arp.h>

#include <utils/common/log.h>
#include <utils/common/timermanager.h>


static const int BYTES_PER_MB = 1024*1024;
static const int NET_STAT_CALCULATION_PERIOD_SEC = 10;
static const int MS_PER_SEC = 1000;

class QnSysDependentMonitorPrivate
:
    public TimerEventHandler
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

    static const size_t MAX_LINE_LENGTH = 256;


    QnSysDependentMonitorPrivate()
    :
        m_calculateNetstatTaskID(0),
        lastPartitionsUpdateTime(0),
        m_terminated(false)
    {
        memset(&lastDiskUsageUpdateTime, 0, sizeof(lastDiskUsageUpdateTime));
        
        m_calculateNetstatTaskID = TimerManager::instance()->addTimer( this, NET_STAT_CALCULATION_PERIOD_SEC * MS_PER_SEC );
    }

    virtual ~QnSysDependentMonitorPrivate()
    {
        {
            QMutexLocker lk( &m_mutex );
            m_terminated = true;
        }
        if( m_calculateNetstatTaskID )
        {
            TimerManager::instance()->joinAndDeleteTimer( m_calculateNetstatTaskID );
            m_calculateNetstatTaskID = 0;
        }
    }

    QList<HddLoad> totalHddLoad() {
        QList<HddLoad> result;

        updatePartitions();

        struct timespec time;
        memset(&time, 0, sizeof(time));
        clock_gettime(CLOCK_MONOTONIC, &time);
        if(!lastDiskUsageUpdateTime.tv_sec) { /* It can overflow and become 0. In this case we will skip one calculation. Nothing serious... */
            lastDiskUsageUpdateTime = time;
            return zeroLoad(); /* First time doing nothing. */
        }

        /* Calculate elapsed time. */
        unsigned int elapsed = (time.tv_sec - lastDiskUsageUpdateTime.tv_sec) * 1000;
        if(time.tv_nsec >= lastDiskUsageUpdateTime.tv_nsec)
            elapsed += (time.tv_nsec - lastDiskUsageUpdateTime.tv_nsec) / 1000000;
        else
            elapsed -= (lastDiskUsageUpdateTime.tv_nsec - time.tv_nsec) / 1000000;
        if(elapsed == 0)
            return zeroLoad(); /* No point to update. */
        lastDiskUsageUpdateTime = time;

        /* Reading current disk statistics. */
        FILE *file = fopen("/proc/diskstats", "r");
        if(!file)
            return zeroLoad();

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

        return result;
    }

    QList<QnPlatformMonitor::NetworkLoad> totalNetworkLoad()
    {
        QMutexLocker lk( &m_mutex );

        QList<QnPlatformMonitor::NetworkLoad> netStat;
        for( std::map<QString, InterfaceStatisticsContext>::const_iterator
            it = m_ifNameToStatistics.begin();
            it != m_ifNameToStatistics.end();
            ++it )
        {
            netStat.push_back( it->second );
  
            std::cout<<"totalNetworkLoad. Returning "<<it->second.interfaceName.toLatin1().constData()<<" in: "<<it->second.bytesPerSecIn<<", out: "<<it->second.bytesPerSecOut<<"\n";
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
        for(int i = 0; fgets(line, MAX_LINE_LENGTH, file) != NULL; ++i) {
            if(i == 0)
                continue; /* Skip header. */

            unsigned int majorNumber = 0, minorNumber = 0, numBlocks = 0;
            char devName[MAX_LINE_LENGTH];
            if(sscanf(line, "%u %u %u %s", &majorNumber, &minorNumber, &numBlocks, devName) != 4)  
                continue; /* Skip unrecognized lines. */

            QString devNameString = QString::fromUtf8(devName);
            if(devNameString.isEmpty() || devNameString[devNameString.size() - 1].isDigit())
                continue; /* Not a physical drive. */

            int id = calculateId(majorNumber, minorNumber);
            diskById[id] = Hdd(id, devNameString, devNameString);
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

    //!Implementation of TimerEventHandler::onTimer
    virtual void onTimer( const quint64& /*timerID*/ ) override
    {
        const qint64 elapsed = m_networkStatCalcTimer.elapsed();
        if( m_networkStatCalcTimer.isValid() && elapsed > 0 )
        {
            //listing /sys/class/net/
            QDir sysClassNet( QLatin1String("/sys/class/net/") );
            if( !sysClassNet.exists() )
            {
                NX_LOG( QString::fromLatin1("No /sys/class/net/. Cannot read network statistics"), cl_logWARNING );
                QMutexLocker lk( &m_mutex );
                if( !m_terminated )
                    m_calculateNetstatTaskID = 0;
                return;
            }

            QStringList intefaceList = sysClassNet.entryList( QStringList(), QDir::Dirs | QDir::NoDotAndDotDot );
            foreach( const QString& interfaceName, intefaceList )
            {
                InterfaceStatisticsContext ctx;
                {
                    QMutexLocker lk( &m_mutex );
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

                ctx.bytesPerSecMax = readFileContents( QString::fromLatin1("/sys/class/net/%1/speed").arg(interfaceName) ).toInt() * BYTES_PER_MB / CHAR_BIT;
                if( !ctx.bytesPerSecMax )
                    ctx.bytesPerSecMax = 100 * 1024 * 1024 / CHAR_BIT;
                const uint64_t rx_bytes = readFileContents( QString::fromLatin1("/sys/class/net/%1/statistics/rx_bytes").arg(interfaceName) ).toLongLong();
                if( ctx.bytesReceived > 0 )
                    ctx.bytesPerSecIn = (rx_bytes - ctx.bytesReceived) * MS_PER_SEC / elapsed;
                ctx.bytesReceived = rx_bytes;
                const uint64_t tx_bytes = readFileContents( QString::fromLatin1("/sys/class/net/%1/statistics/tx_bytes").arg(interfaceName) ).toLongLong();
                if( ctx.bytesSent > 0 )
                    ctx.bytesPerSecOut = (tx_bytes - ctx.bytesSent) * MS_PER_SEC / elapsed;
                ctx.bytesSent = tx_bytes;

                //std::cout<<"Interface "<<interfaceName.toLatin1().constData()<<" (mac "<<ctx.macAddress.toString().toLatin1().constData()<<") statistics: "
                //    "speed "<<ctx.bytesPerSecMax<<", rx_bytes "<<rx_bytes<<", tx_bytes "<<tx_bytes<<", "
                //    "bytesPerSecIn "<<ctx.bytesPerSecIn<<", bytesPerSecOut "<<ctx.bytesPerSecOut<<"\n";
                    
                {
                    QMutexLocker lk( &m_mutex );
                    m_ifNameToStatistics[interfaceName] = ctx;
                }
            }
        }

        m_networkStatCalcTimer.restart();

        QMutexLocker lk( &m_mutex );
        if( !m_terminated )
            m_calculateNetstatTaskID = TimerManager::instance()->addTimer( this, NET_STAT_CALCULATION_PERIOD_SEC * MS_PER_SEC );
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

    mutable QMutex m_mutex;
    QHash<int, Hdd> diskById;
    QHash<int, unsigned int> lastDiskTimeById;
    std::map<QString, InterfaceStatisticsContext> m_ifNameToStatistics;
    QElapsedTimer m_networkStatCalcTimer;
    qint64 m_calculateNetstatTaskID;

    time_t lastPartitionsUpdateTime;
    struct timespec lastDiskUsageUpdateTime;

    bool m_terminated;

private:
    Q_DECLARE_PUBLIC(QnSysDependentMonitor);
    QnSysDependentMonitor *q_ptr;
};

QnSysDependentMonitor::QnSysDependentMonitor(QObject *parent):
    base_type(parent),
    d_ptr(new QnSysDependentMonitorPrivate())
{
    d_ptr->q_ptr = this;
}

QnSysDependentMonitor::~QnSysDependentMonitor() {
    return;
}

QList<QnPlatformMonitor::HddLoad> QnSysDependentMonitor::totalHddLoad() {
    return d_func()->totalHddLoad();
}

QList<QnPlatformMonitor::NetworkLoad> QnSysDependentMonitor::totalNetworkLoad()
{
    return d_func()->totalNetworkLoad();
}

#endif
