#ifndef QN_PLATFORM_ABSTRACTION_H
#define QN_PLATFORM_ABSTRACTION_H

#include <QtCore/QObject>

#include "monitoring/global_monitor.h"
#include "notification/platform_notifier.h"
#include "process/platform_process.h"

class QnPlatformAbstraction: public QObject {
    Q_OBJECT;
public:
    QnPlatformAbstraction(QObject *parent = NULL);
    virtual ~QnPlatformAbstraction();

    /**
     * \returns                         Global instance of platform abstraction object.
     *                                  Note that this instance must be created first (e.g. on the stack, like a <tt>QApplication</tt>).
     */
    static QnPlatformAbstraction *instance() {
        return s_instance;
    }

    QnGlobalMonitor *monitor() const {
        return m_monitor;
    }

    QnPlatformNotifier *notifier() const {
        return m_notifier;
    }

    QnPlatformProcess *process(QProcess *source = NULL) const;

private:
    static QnPlatformAbstraction *s_instance;
    QnGlobalMonitor *m_monitor;
    QnPlatformNotifier *m_notifier;
    QnPlatformProcess *m_process;
};

#define qnPlatform (QnPlatformAbstraction::instance())


#endif // QN_PLATFORM_ABSTRACTION_H
