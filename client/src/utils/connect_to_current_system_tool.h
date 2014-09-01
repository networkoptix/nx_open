#ifndef CONNECT_TO_CURRENT_SYSTEM_TOOL_H
#define CONNECT_TO_CURRENT_SYSTEM_TOOL_H

#include <QtCore/QObject>

#include <utils/common/id.h>
#include <ui/workbench/workbench_context_aware.h>

class QnConfigurePeerTask;
class QnUpdateDialog;
class QnMediaServerUpdateTool;
class QnWaitCompatibleServersPeerTask;

class QnConnectToCurrentSystemTool : public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    enum ErrorCode {
        NoError,
        AuthentificationFailed,
        ConfigurationFailed,
        UpdateFailed
    };

    explicit QnConnectToCurrentSystemTool(QnWorkbenchContext *context, QObject *parent = 0);
    ~QnConnectToCurrentSystemTool();

    void connectToCurrentSystem(const QSet<QUuid> &targets, const QString &password);

    bool isRunning() const;

    QSet<QUuid> targets() const;

signals:
    void finished(int errorCode);

private:
    void finish(ErrorCode errorCode);
    void configureServer();
    void waitPeers();
    void updatePeers();
    void revertApiUrls();

private slots:
    void at_configureTask_finished(int errorCode, const QSet<QUuid> &failedPeers);
    void at_waitTask_finished(int errorCode);
    void at_updateTool_stateChanged(int state);

private:
    bool m_running;
    QSet<QUuid> m_targets;
    QString m_password;

    QSet<QUuid> m_restartTargets;
    QSet<QUuid> m_updateTargets;
    QHash<QUuid, QUrl> m_oldUrls;
    QnConfigurePeerTask *m_configureTask;
    QnWaitCompatibleServersPeerTask *m_waitTask;
    QScopedPointer<QnUpdateDialog> m_updateDialog;
    QnMediaServerUpdateTool *m_updateTool;
    int m_prevToolState;
    bool m_updateFailed;
    bool m_restartAllPeers;
};

#endif // CONNECT_TO_CURRENT_SYSTEM_TOOL_H
