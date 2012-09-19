#include "sigar_monitor.h"

#include <cassert>

#include <utils/common/warnings.h>

#include <sigar.h>
#include <sigar_format.h>

#ifdef _MSC_VER
#   pragma comment(lib, "sigar.lib")
#endif

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

private:
    sigar_t *sigar;
    sigar_cpu_t cpu;
    QHash<QString, sigar_disk_usage_t> usageByHddName;

private:
    Q_DECLARE_PUBLIC(QnSigarMonitor);
    QnSigarMonitor *q_ptr;
};

#define INVOKE(expression)                                                      \
    (d_func()->checkError(#expression, expression))

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

    return mem.used_percent;
}

QList<QnPlatformMonitor::Hdd> QnSigarMonitor::hdds() {
    Q_D(QnSigarMonitor);
    QList<QnPlatformMonitor::Hdd> result;

    if(!d->sigar)
        return result;

    sigar_file_system_list_t fileSystems;

    if(INVOKE(sigar_file_system_list_get(d->sigar, &fileSystems)) != SIGAR_OK)
        return result;

    for(int i = 0; i < fileSystems.number; i++) {
        const sigar_file_system_t &fileSystem = fileSystems.data[i];
        if(fileSystem.type != SIGAR_FSTYPE_LOCAL_DISK)
            continue; /* Skip non-hdds. */

        result.push_back(Hdd(i, QLatin1String(fileSystem.dev_name), QLatin1String(fileSystem.dir_name)));
    }
    
    INVOKE(sigar_file_system_list_destroy(d->sigar, &fileSystems));
    return result;
}

qreal QnSigarMonitor::totalHddLoad(const Hdd &hdd) {
    Q_D(QnSigarMonitor);

    if(!d->sigar)
        return 0.0;

    sigar_disk_usage_t disk;
    if(INVOKE(sigar_disk_usage_get(d->sigar, hdd.name.toLatin1().constData(), &disk)) != SIGAR_OK)
        return 0.0;

    if(!d->usageByHddName.contains(hdd.name)) { /* Is this the first call? */
        d->usageByHddName[hdd.name] = disk; 
        return 0.0;
    }

    sigar_disk_usage_t &last = d->usageByHddName[hdd.name];
    if(disk.snaptime == last.snaptime)
        return 0.0;

    qreal result = static_cast<qreal>(disk.time - last.time) / (disk.snaptime - last.snaptime);
    last = disk;

    return result;
}

