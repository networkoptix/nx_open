#ifndef QN_WINDOWS_MONITOR_H
#define QN_WINDOWS_MONITOR_H

#include <QtCore/QtGlobal>

#ifdef Q_OS_WIN
#include "sigar_monitor.h"

class QnWindowsMonitorPrivate;

class QnWindowsMonitor: public QnSigarMonitor {
    Q_OBJECT;

    typedef QnSigarMonitor base_type;

public:
    QnWindowsMonitor(QObject *parent = NULL);
    virtual ~QnWindowsMonitor();

    virtual QList<HddLoad> totalHddLoad() override;

private:
    Q_DECLARE_PRIVATE(QnWindowsMonitor);
    QScopedPointer<QnWindowsMonitorPrivate> d_ptr;
};

#endif // Q_OS_WIN

#endif // QN_WINDOWS_MONITOR_H
