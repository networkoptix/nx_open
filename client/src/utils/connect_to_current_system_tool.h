#ifndef CONNECT_TO_CURRENT_SYSTEM_TOOL_H
#define CONNECT_TO_CURRENT_SYSTEM_TOOL_H

#include <QtCore/QObject>

#include <utils/common/id.h>

class QnChangeSystemNamePeerTask;
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

    void connectToCurrentSystem(const QSet<QnId> &targets);

    bool isRunning() const;

signals:
    void finished(int errorCode);

private:
    void finish(ErrorCode errorCode);
    void changeSystemName();
    void updatePeers();
    void restartPeers();

private slots:
    void at_changeSystemNameTask_finished(int errorCode, const QSet<QnId> &failedPeers);
    void at_restartPeerTask_finished(int errorCode);
    void at_updateTool_stateChanged(int state);

private:
    bool m_running;
    QSet<QnId> m_targets;

    QSet<QnId> m_restartTargets;
    QSet<QnId> m_updateTargets;
    QnChangeSystemNamePeerTask *m_changeSystemNameTask;
    QnRestartPeerTask *m_restartPeerTask;
    QScopedPointer<QnUpdateDialog> m_updateDialog;
    QnMediaServerUpdateTool *m_updateTool;
    int m_prevToolState;
    bool m_updateFailed;
    bool m_restartAllPeers;
};

#endif // CONNECT_TO_CURRENT_SYSTEM_TOOL_H
