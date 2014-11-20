#ifndef QN_WORKBENCH_UPDATE_WATCHER_H
#define QN_WORKBENCH_UPDATE_WATCHER_H

#include <QtCore/QObject>

#include <utils/common/software_version.h>
#include <ui/workbench/workbench_context_aware.h>

class QnUpdateChecker;
class QTimer;

class QnWorkbenchUpdateWatcher: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    QnWorkbenchUpdateWatcher(QObject *parent = NULL);
    virtual ~QnWorkbenchUpdateWatcher();

    QnSoftwareVersion latestVersion() const { return m_latestVersion; }

public slots:
    void start();
    void stop();

private slots:
    void at_checker_updateAvailable(const QnSoftwareVersion &updateVersion, const QUrl &releaseNotesUrl);

private:
    QnUpdateChecker *m_checker;
    QTimer *m_timer;
    QnSoftwareVersion m_latestVersion;
};

#endif // QN_WORKBENCH_UPDATE_WATCHER_H
