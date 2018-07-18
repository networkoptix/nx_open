#ifndef QN_WORKBENCH_UPDATE_WATCHER_H
#define QN_WORKBENCH_UPDATE_WATCHER_H

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>

#include <nx/utils/software_version.h>

class QnUpdateChecker;
class QTimer;
struct QnUpdateInfo;

class QnWorkbenchUpdateWatcher:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT

public:
    QnWorkbenchUpdateWatcher(QObject* parent = nullptr);
    virtual ~QnWorkbenchUpdateWatcher() override;

public:
    void start();
    void stop();

private:
    void showUpdateNotification(const QnUpdateInfo& info);
    void at_checker_updateAvailable(const QnUpdateInfo& info);

private:
    QnUpdateChecker* const m_checker;
    QTimer* const m_timer;
    nx::utils::SoftwareVersion m_notifiedVersion;
};

#endif // QN_WORKBENCH_UPDATE_WATCHER_H
