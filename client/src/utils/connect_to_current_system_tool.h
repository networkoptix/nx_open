#ifndef CONNECT_TO_CURRENT_SYSTEM_TOOL_H
#define CONNECT_TO_CURRENT_SYSTEM_TOOL_H

#include <QtCore/QObject>

#include <utils/common/id.h>

class QnConfigurePeerTask;
class QnRestartPeerTask;
class QnUpdateDialog;
class QnMediaServerUpdateTool;

class QnConnectToCurrentSystemTool : public QObject {
    Q_OBJECT
public:
    enum ErrorCode {
        NoError,
        SystemNameChangeFailed,
        UpdateFailed,
        RestartFailed
    };

    explicit QnConnectToCurrentSystemTool(QObject *parent = 0);
    ~QnConnectToCurrentSystemTool();

    void connectToCurrentSystem(const QSet<QnId> &targets, const QString &password);

    bool isRunning() const;

signals:
    void finished(int errorCode);

private:
    void finish(ErrorCode errorCode);
    void changeSystemName();
    void updatePeers();
    void restartPeers();

private slots:
    void at_configureTask_finished(int errorCode, const QSet<QnId> &failedPeers);
    void at_restartPeerTask_finished(int errorCode);
    void at_updateTool_stateChanged(int state);

private:
    bool m_running;
    QSet<QnId> m_targets;
    QString m_password;

    QSet<QnId> m_restartTargets;
    QSet<QnId> m_updateTargets;
    QnConfigurePeerTask *m_configureTask;
    QnRestartPeerTask *m_restartPeerTask;
    QScopedPointer<QnUpdateDialog> m_updateDialog;
    QnMediaServerUpdateTool *m_updateTool;
    int m_prevToolState;
    bool m_updateFailed;
    bool m_restartAllPeers;
};

#endif // CONNECT_TO_CURRENT_SYSTEM_TOOL_H
