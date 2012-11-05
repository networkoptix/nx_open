#include "linux_monitor.h"

#ifdef Q_OS_LINUX

#include <QtCore/QHash>

#include <time.h>
#include <signal.h>
#include <errno.h>


class QnLinuxMonitorPrivate {
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


    QnLinuxMonitorPrivate():
        lastPartitionsUpdateTime(0)
    {
        memset(&lastDiskUsageUpdateTime, 0, sizeof(lastDiskUsageUpdateTime));
    }

    virtual ~QnLinuxMonitorPrivate() {}

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

        // TODO: read network drives?
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

private:
    QHash<int, Hdd> diskById;
    QHash<int, unsigned int> lastDiskTimeById;
    
    time_t lastPartitionsUpdateTime;
    struct timespec lastDiskUsageUpdateTime;

private:
    Q_DECLARE_PUBLIC(QnLinuxMonitor);
    QnLinuxMonitor *q_ptr;
};

QnLinuxMonitor::QnLinuxMonitor(QObject *parent):
    base_type(parent),
    d_ptr(new QnLinuxMonitorPrivate())
{
    d_ptr->q_ptr = this;
}

QnLinuxMonitor::~QnLinuxMonitor() {
    return;
}

QList<QnPlatformMonitor::HddLoad> QnLinuxMonitor::totalHddLoad() {
    return d_func()->totalHddLoad();
}

#endif
