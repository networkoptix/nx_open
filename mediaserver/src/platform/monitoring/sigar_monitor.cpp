
#include "sigar_monitor.h"

#include <cassert>

#include <QtCore/QElapsedTimer>
#include <QtCore/QStringList>

#include <utils/common/warnings.h>
#include <utils/common/util.h>
#include <utils/common/log.h>

extern "C"
{
#include <sigar.h>
#include <sigar_format.h>
}


namespace {
    /**
     * See http://stackoverflow.com/questions/3062594/differentiate-vmware-network-adapter-from-physical-network-adapters.
     */
    const char *virtualMacs[] = {
        "\x00\x05\x69",     /* vmware1 */
        "\x00\x0C\x29",     /* vmware2 */
        "\x00\x50\x56",     /* vmware3 */
        "\x00\x1C\x42",     /* parallels1 */
        "\x00\x03\xFF",     /* microsoft virtual pc */
        "\x00\x0F\x4B",     /* virtual iron 4 */
        "\x00\x16\x3E",     /* red hat xen , oracle vm , xen source, novell xen */
        "\x08\x00\x27"      /* virtualbox */
    };

} // anonymous namespace

#define INVOKE(expression)                                                      \
    (d_func()->checkError(#expression, expression))

// -------------------------------------------------------------------------- //
// QnSigarMonitorPrivate
// -------------------------------------------------------------------------- //
class QnSigarMonitorPrivate {
public:
    QnSigarMonitorPrivate() {
        sigar = NULL;
        memset(&cpu, 0, sizeof(cpu));
    }

    virtual ~QnSigarMonitorPrivate() {
        return;
    }

    int checkError(const char *expression, int status) const {
        assert(expression);

        if(status == SIGAR_OK)
            return status;

        QByteArray function(expression);
        int index = function.indexOf('(');
        if(index != -1)
            function.truncate(index);

        if(sigar == NULL) {
            qnWarning("Error in %1: %2.", function, status);
        } else {
            qnWarning("Error in %1: %2 (%3).", function, status, sigar_strerror(sigar, status));
        }
        return status;
    }

    qreal hddLoad(const QnPlatformMonitor::Hdd &hdd) {
        if(!sigar)
            return 0.0;

        sigar_disk_usage_t current;
        if(INVOKE(sigar_disk_usage_get(sigar, hdd.name.toLatin1().constData(), &current)) != SIGAR_OK)
            return 0.0;

        if(!lastUsageByHddName.contains(hdd.name)) { /* Is this the first call? */
            lastUsageByHddName[hdd.name] = current; 
            return 0.0;
        }

        sigar_disk_usage_t &last = lastUsageByHddName[hdd.name];
        if(current.snaptime == last.snaptime)
            return 0.0;

        qreal result = static_cast<qreal>(current.time - last.time) / (current.snaptime - last.snaptime);
        last = current;

        return result;
    }

    QnPlatformMonitor::PartitionType partitionType(sigar_file_system_type_e type) {
        switch(type) {
        case SIGAR_FSTYPE_LOCAL_DISK:
            return QnPlatformMonitor::LocalDiskPartition;
        case SIGAR_FSTYPE_NETWORK:
            return QnPlatformMonitor::NetworkPartition;
        case SIGAR_FSTYPE_RAM_DISK:
            return QnPlatformMonitor::RamDiskPartition;
        case SIGAR_FSTYPE_CDROM:
            return QnPlatformMonitor::OpticalDiskPartition;
        case SIGAR_FSTYPE_SWAP:
            return QnPlatformMonitor::SwapPartition;
        case SIGAR_FSTYPE_UNKNOWN:
        case SIGAR_FSTYPE_NONE:
        default:
            return QnPlatformMonitor::UnknownPartition;
        }
    }

    QnPlatformMonitor::PartitionSpace partitionUsage(const sigar_file_system_t &fileSystem) {
        QnPlatformMonitor::PartitionSpace result;

        if(!sigar)
            return result;

        sigar_file_system_usage_t usage;
        if(INVOKE(sigar_file_system_usage_get(sigar, fileSystem.dir_name, &usage)) != SIGAR_OK)
            return result;

        result.path = QLatin1String(fileSystem.dir_name);
        result.freeBytes = usage.free * 1024;
        result.sizeBytes = usage.total * 1024;
        if( strcmp( fileSystem.sys_type_name, "fuseblk" ) == 0 )
            result.type = QnPlatformMonitor::LocalDiskPartition;      //TODO #ak this is workaround, have to fix it correctly
        else
            result.type = partitionType(fileSystem.type);

        return result;
    }

    QnPlatformMonitor::NetworkLoad networkLoad(const QString &interfaceName) {
        QnPlatformMonitor::NetworkLoad result;
        result.interfaceName = interfaceName;

        if(!sigar)
            return result;

        sigar_net_interface_stat_t current;
        if (INVOKE(sigar_net_interface_stat_get(sigar, interfaceName.toLatin1().constData(), &current) != SIGAR_OK))
            return result;

        result.bytesPerSecMax = current.speed / 8;

        if(!lastNetworkStatByInterfaceName.contains(interfaceName)) { /* Is this the first call? */
            NetworkStat stat;
            stat.stat = current;
            stat.timer.start();
            lastNetworkStatByInterfaceName[interfaceName] = stat;
            return result;
        }

        NetworkStat &last = lastNetworkStatByInterfaceName[interfaceName];
        qint64 elapsed = last.timer.isValid() ? last.timer.elapsed() : 0;
        if(elapsed <= 0) {
            result.bytesPerSecIn = result.bytesPerSecOut = 0;
        } else {
            qint64 bytesIn = static_cast<qint64>(current.rx_bytes) - static_cast<qint64>(last.stat.rx_bytes);
            qint64 bytesOut = static_cast<qint64>(current.tx_bytes) - static_cast<qint64>(last.stat.tx_bytes);

#ifdef Q_OS_WIN
            // TODO: #Elric totally evil workaround for windows, flaw in GetIfTable.
            // The right way to fix it would be to reimplement this part in windows-specific
            // monitor to use 64-bit functions.
            if(bytesIn < 0) /* Integer overflow. */
                bytesIn = bytesIn + std::numeric_limits<unsigned int>::max();
            if(bytesOut < 0) /* Integer overflow. */
                bytesOut = bytesOut + std::numeric_limits<unsigned int>::max();
#endif

            result.bytesPerSecIn = bytesIn * 1000 / elapsed;
            result.bytesPerSecOut = bytesOut * 1000 / elapsed;
        }
        last.stat = current;
        last.timer.restart();

        return result;
    }


private:
    const QnSigarMonitorPrivate *d_func() const { return this; } /* For INVOKE to work. */

    struct NetworkStat {
        QElapsedTimer timer;
        sigar_net_interface_stat_t stat;
    };

private:
    sigar_t *sigar;
    sigar_cpu_t cpu;
    QHash<QString, sigar_disk_usage_t> lastUsageByHddName;
    QHash<QString, NetworkStat> lastNetworkStatByInterfaceName;

private:
    Q_DECLARE_PUBLIC(QnSigarMonitor)
    QnSigarMonitor *q_ptr;
};


// -------------------------------------------------------------------------- //
// QnSigarMonitor
// -------------------------------------------------------------------------- //
QnSigarMonitor::QnSigarMonitor(QObject *parent):
    QnPlatformMonitor(parent),
    d_ptr(new QnSigarMonitorPrivate())
{
    Q_D(QnSigarMonitor);
    d->q_ptr = this;

    INVOKE(sigar_open(&d->sigar));
}

QnSigarMonitor::~QnSigarMonitor() {
    Q_D(QnSigarMonitor);

    if(d->sigar)
        INVOKE(sigar_close(d->sigar));
}

qreal QnSigarMonitor::totalCpuUsage() {
    Q_D(QnSigarMonitor);

    if(!d->sigar)
        return 0.0;

    sigar_cpu_t cpu;
    if(INVOKE(sigar_cpu_get(d->sigar, &cpu)) != SIGAR_OK)
        return 0.0;

    if(d->cpu.total == 0) { /* Is this the first call? */
        d->cpu = cpu; 
        return 0.0;
    }

    if(cpu.total == d->cpu.total)
        return 0.0;

    sigar_cpu_perc_t result;
    if(INVOKE(sigar_cpu_perc_calculate(&d->cpu, &cpu, &result)) != SIGAR_OK)
        return 0.0;

    d->cpu = cpu;
    return result.combined;
}

qreal QnSigarMonitor::totalRamUsage() {
    Q_D(QnSigarMonitor);

    if(!d->sigar)
        return 0.0;

    sigar_mem_t mem;
    if(INVOKE(sigar_mem_get(d->sigar, &mem)) != SIGAR_OK)
        return 0.0;

    return mem.used_percent / 100.0;
}

QList<QnPlatformMonitor::HddLoad> QnSigarMonitor::totalHddLoad() {
    Q_D(QnSigarMonitor);

    QList<HddLoad> result;

    if(!d->sigar)
        return result;

    sigar_file_system_list_t fileSystems;

    if(INVOKE(sigar_file_system_list_get(d->sigar, &fileSystems)) != SIGAR_OK)
        return result;

    for(uint i = 0; i < fileSystems.number; i++) {
        const sigar_file_system_t &fileSystem = fileSystems.data[i];
        if(fileSystem.type != SIGAR_FSTYPE_LOCAL_DISK)
            continue; /* Skip non-hdds. */

        Hdd hdd(i, QLatin1String(fileSystem.dev_name), QLatin1String(fileSystem.dir_name));
        result.push_back(HddLoad(hdd, d->hddLoad(hdd)));
    }

    INVOKE(sigar_file_system_list_destroy(d->sigar, &fileSystems));
    return result;
}

QList<QnPlatformMonitor::PartitionSpace> QnSigarMonitor::totalPartitionSpaceInfo() {
    Q_D(QnSigarMonitor);

    QList<PartitionSpace> result;

    sigar_file_system_list_t fileSystems;

    if(INVOKE(sigar_file_system_list_get(d->sigar, &fileSystems)) != SIGAR_OK)
        return result;

    for(uint i = 0; i < fileSystems.number; i++)
        result.push_back(d->partitionUsage(fileSystems.data[i]));

    INVOKE(sigar_file_system_list_destroy(d->sigar, &fileSystems));
    return result;
}

QList<QnPlatformMonitor::NetworkLoad> QnSigarMonitor::totalNetworkLoad() {
    Q_D(QnSigarMonitor);

    QList<NetworkLoad> result;

    sigar_net_interface_list_t networkInterfaces;

    if(INVOKE(sigar_net_interface_list_get(d->sigar, &networkInterfaces)) != SIGAR_OK)
        return result;

    QSet<QString> interfaceNames;
    QSet<QnMacAddress> interfaceMacs;
    for(uint i = 0; i < networkInterfaces.number; i++) {
        QString interfaceName = QLatin1String(networkInterfaces.data[i]);
        if (interfaceNames.contains(interfaceName))
            continue; /* Skip duplicate interface entries. */ 

        sigar_net_interface_config_t config;
        if (INVOKE(sigar_net_interface_config_get(d->sigar, interfaceName.toLatin1().constData(), &config) != SIGAR_OK))
            continue;
       
        /* Interface is down. */
        if ((config.flags & (SIGAR_IFF_UP | SIGAR_IFF_RUNNING) ) != (SIGAR_IFF_UP | SIGAR_IFF_RUNNING) )
            continue;

        /* Interface has no address (QoS Scheduler or WAN Miniport)  */
        if (!config.address.addr.in)
            continue;

        NetworkLoad load = d->networkLoad(interfaceName);
        if(config.flags & SIGAR_IFF_LOOPBACK) {
            load.type = LoopbackInterface;
        } else if(config.hwaddr.family == sigar_net_address_t::SIGAR_AF_LINK) {
            load.macAddress = QnMacAddress(config.hwaddr.addr.mac);
            if(interfaceMacs.contains(load.macAddress))
                continue; /* Skip entries with duplicate macs. */

            /* Detect virtual interfaces. */ 
            for(size_t i = 0; i < arraysize(virtualMacs); i++) {
                if(memcmp(load.macAddress.bytes(), virtualMacs[i], 3) == 0) {
                    load.type = VirtualInterface;
                    break;
                }
            }

            if(load.type != VirtualInterface)
                load.type = PhysicalInterface;
        } else {
            load.type = PhysicalInterface;
        }

        interfaceMacs.insert(load.macAddress);
        interfaceNames.insert(interfaceName);
        result.push_back(load);
    }

    INVOKE(sigar_net_interface_list_destroy(d->sigar, &networkInterfaces));
    return result;
}
