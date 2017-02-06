#ifndef QN_WORKBENCH_UPDATE_WATCHER_H
#define QN_WORKBENCH_UPDATE_WATCHER_H

#include <QtCore/QObject>

#include <utils/common/software_version.h>
#include <ui/workbench/workbench_context_aware.h>

class QnUpdateChecker;
class QTimer;
struct QnUpdateInfo;

class QnWorkbenchUpdateWatcher: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
public:
    QnWorkbenchUpdateWatcher(QObject *parent = NULL);
    virtual ~QnWorkbenchUpdateWatcher();

public:
    void start();
    void stop();

private:
    void showUpdateNotification(const QnUpdateInfo &info);

    void at_checker_updateAvailable(const QnUpdateInfo &info);
private:
    QnUpdateChecker *m_checker;
    QTimer *m_timer;
    QnSoftwareVersion m_notifiedVersion;
};

#endif // QN_WORKBENCH_UPDATE_WATCHER_H
