#ifndef QN_CORE_PLATFORM_ABSTRACTION_H
#define QN_CORE_PLATFORM_ABSTRACTION_H

#include <QtCore/QObject>

#include <utils/common/singleton.h>

#include "monitoring/global_monitor.h"
#include "notification/platform_notifier.h"
#include "process/platform_process.h"

class QnCorePlatformAbstraction: public QObject, public Singleton<QnCorePlatformAbstraction> {
    Q_OBJECT
public:
    QnCorePlatformAbstraction(QObject *parent = NULL);
    virtual ~QnCorePlatformAbstraction();

    QnGlobalMonitor *monitor() const {
        return m_monitor;
    }

    QnPlatformNotifier *notifier() const {
        return m_notifier;
    }
    
    QnPlatformProcess *process(QProcess *source = NULL) const;

private:
    QnGlobalMonitor *m_monitor;
    QnPlatformNotifier *m_notifier;
    QnPlatformProcess *m_process;
};

#define qnPlatform (QnCorePlatformAbstraction::instance())


#endif // QN_CORE_PLATFORM_ABSTRACTION_H
