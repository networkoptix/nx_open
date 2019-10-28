#pragma once

#include "sigar_monitor.h"

class QnMacMonitor: public QnSigarMonitor {
    Q_OBJECT
    typedef QnSigarMonitor base_type;

public:
    QnMacMonitor();
    virtual ~QnMacMonitor();
};
