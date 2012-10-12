#ifndef QN_WORKBENCH_UPDATE_WATCHER_H
#define QN_WORKBENCH_UPDATE_WATCHER_H

#include <QtCore/QObject>

#include <utils/appcast/update_info.h>

class QnWorkbenchUpdateWatcher: public QObject {
    Q_OBJECT;
public:
    QnWorkbenchUpdateWatcher(QObject *parent = NULL);
    virtual ~QnWorkbenchUpdateWatcher();

    QnUpdateInfoItems availableUpdates() const {
        return m_availableUpdates;
    }

signals:
    void availableUpdatesChanged();

private slots:
    void at_checker_updatesAvailable();
    void at_timer_timeout();

private:
    QnUpdateChecker *m_checker;
    QnUpdateInfoItems m_availableUpdates;
};

#endif // QN_WORKBENCH_UPDATE_WATCHER_H
