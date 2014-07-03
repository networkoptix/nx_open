#ifndef QN_WORKBENCH_UPDATE_WATCHER_H
#define QN_WORKBENCH_UPDATE_WATCHER_H

#include <QtCore/QObject>

#include <utils/common/software_version.h>

class QnUpdateChecker;
class QTimer;

class QnWorkbenchUpdateWatcher: public QObject {
    Q_OBJECT
public:
    QnWorkbenchUpdateWatcher(QObject *parent = NULL);
    virtual ~QnWorkbenchUpdateWatcher();

    QnSoftwareVersion availableUpdate() const { return m_availableUpdate; }

public slots:
    void start();
    void stop();

signals:
    void availableUpdateChanged();

private slots:
    void at_checker_updateAvailable(const QnSoftwareVersion &version);

private:
    QnUpdateChecker *m_checker;
    QTimer *m_timer;
    QnSoftwareVersion m_availableUpdate;
};

#endif // QN_WORKBENCH_UPDATE_WATCHER_H
