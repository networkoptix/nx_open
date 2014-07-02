#ifndef QN_WORKBENCH_UPDATE_WATCHER_H
#define QN_WORKBENCH_UPDATE_WATCHER_H

#include <QtCore/QObject>

#include <utils/common/software_version.h>

class QnUpdateChecker;

class QnWorkbenchUpdateWatcher: public QObject {
    Q_OBJECT
public:
    QnWorkbenchUpdateWatcher(QObject *parent = NULL);
    virtual ~QnWorkbenchUpdateWatcher();

    QnSoftwareVersion availableUpdate() const { return m_availableUpdate; }

signals:
    void availableUpdateChanged();

private slots:
    void at_checker_updateAvailable(const QnSoftwareVersion &version);
    void at_timer_timeout();

private:
    QnUpdateChecker *m_checker;
    QnSoftwareVersion m_availableUpdate;
};

#endif // QN_WORKBENCH_UPDATE_WATCHER_H
