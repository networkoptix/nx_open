#ifndef QN_MAC_MONITOR_H
#define QN_MAC_MONITOR_H

#include "sigar_monitor.h"

class QnMacMonitor: public QnSigarMonitor {
    Q_OBJECT
    typedef QnSigarMonitor base_type;

public:
    QnMacMonitor(QObject *parent = NULL);
    virtual ~QnMacMonitor();
};

#endif // QN_MAC_MONITOR_H
