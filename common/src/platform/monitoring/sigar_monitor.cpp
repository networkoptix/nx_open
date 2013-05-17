#include "sigar_monitor.h"

#include <cassert>

#include <QtCore/QElapsedTimer>
#include <QStringList>

#include <utils/common/warnings.h>

extern "C"
{
    #include <sigar.h>
    #include <sigar_format.h>
}

#ifdef _MSC_VER
#   pragma comment(lib, "sigar.lib")
#endif

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

    QnPlatformMonitor::PartitionSpace partitionUsage(const QString &path) {
        QnPlatformMonitor::PartitionSpace result;

        if(!sigar)
            return result;

        sigar_file_system_usage_t usage;
        if(INVOKE(sigar_file_system_usage_get(sigar, path.toLatin1().constData(), &usage)) != SIGAR_OK)
            return result;

        result.path = path; // TODO
        result.freeBytes = usage.free * 1024;
        result.sizeBytes = usage.total * 1024;

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

        if(!lastNetworkStatByInterfaceName.contains(interfaceName)) { /* Is this the first call? */
            NetworkStat stat;
            stat.stat = current;
            stat.timer.start();
            lastNetworkStatByInterfaceName[interfaceName] = stat;
            return result;
        }

        NetworkStat &last = lastNetworkStatByInterfaceName[interfaceName];
        quint64 elapsed = last.timer.isValid()
                ? last.timer.elapsed()
                : 0;
        result.bytesPerSecIn = elapsed > 0
                ? qMax(current.rx_bytes - last.stat.rx_bytes, sigar_uint64_t(0)) * 1000 / elapsed
                : 0;
        result.bytesPerSecOut = elapsed > 0
                ? qMax(current.tx_bytes - last.stat.tx_bytes, sigar_uint64_t(0)) * 1000 / elapsed
                : 0;
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

    for(uint i = 0; i < fileSystems.number; i++) {
        const sigar_file_system_t &fileSystem = fileSystems.data[i];
        if(fileSystem.type != SIGAR_FSTYPE_LOCAL_DISK)
            continue; /* Skip non-hdds. */

        result.push_back(d->partitionUsage(QLatin1String(fileSystem.dir_name)));
    }

    INVOKE(sigar_file_system_list_destroy(d->sigar, &fileSystems));
    return result;
}

QList<QnPlatformMonitor::NetworkLoad> QnSigarMonitor::totalNetworkLoad() {
    Q_D(QnSigarMonitor);

    QList<NetworkLoad> result;

    sigar_net_interface_list_t networkInterfaces;

    if(INVOKE(sigar_net_interface_list_get(d->sigar, &networkInterfaces)) != SIGAR_OK)
        return result;

    QStringList interfacesNames;
    for(uint i = 0; i < networkInterfaces.number; i++) {
        QString interfaceName = QLatin1String(networkInterfaces.data[i]);
        
        // remove duplicating entries
        if (interfacesNames.contains(interfaceName))
            continue;
        interfacesNames.append(interfaceName);

        sigar_net_interface_config_t config;
        if (INVOKE(sigar_net_interface_config_get(d->sigar, interfaceName.toLatin1().constData(), &config) != SIGAR_OK))
            continue;
        if ((config.flags & (SIGAR_IFF_UP | SIGAR_IFF_RUNNING) ) == 0)
            continue;

        result.push_back(d->networkLoad(interfaceName));
    }

    INVOKE(sigar_net_interface_list_destroy(d->sigar, &networkInterfaces));
    return result;
}
