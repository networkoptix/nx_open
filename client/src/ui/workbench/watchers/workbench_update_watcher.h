#ifndef QN_WORKBENCH_UPDATE_WATCHER_H
#define QN_WORKBENCH_UPDATE_WATCHER_H

#include <QtCore/QObject>

#include <utils/appcast/update_info.h>

class QnUpdateChecker;

class QnWorkbenchUpdateWatcher: public QObject {
    Q_OBJECT;
public:
    QnWorkbenchUpdateWatcher(QObject *parent = NULL);
    virtual ~QnWorkbenchUpdateWatcher();

    QnUpdateInfoItem availableUpdate() const {
        return m_availableUpdate;
    }

signals:
    void availableUpdateChanged();

private slots:
    void at_checker_updatesAvailable(QnUpdateInfoItemList updates);
    void at_timer_timeout();

private:
    QnUpdateChecker *m_checker;
    QnUpdateInfoItem m_availableUpdate;
};

#endif // QN_WORKBENCH_UPDATE_WATCHER_H
