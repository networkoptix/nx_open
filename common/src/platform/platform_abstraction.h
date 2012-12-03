#ifndef QN_PLATFORM_ABSTRACTION_H
#define QN_PLATFORM_ABSTRACTION_H

#include <QtCore/QObject>

#include "monitoring/global_monitor.h"
#include "notification/platform_notifier.h"
#include "images/platform_images.h"

class QnPlatformAbstraction: public QObject {
    Q_OBJECT
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

    QnPlatformImages *images() const {
        return m_images;
    }

private:
    static QnPlatformAbstraction *s_instance;
    QnGlobalMonitor *m_monitor;
    QnPlatformNotifier *m_notifier;
    QnPlatformImages *m_images;
};

#define qnPlatform (QnPlatformAbstraction::instance())


#endif // QN_PLATFORM_ABSTRACTION_H
