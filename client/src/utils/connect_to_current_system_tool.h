#ifndef CONNECT_TO_CURRENT_SYSTEM_TOOL_H
#define CONNECT_TO_CURRENT_SYSTEM_TOOL_H

#include <QtCore/QObject>

#include <utils/common/id.h>

class QnChangeSystemNamePeerTask;
class QnUpdateDialog;
class QnMediaServerUpdateTool;

class QnConnectToCurrentSystemTool : public QObject {
    Q_OBJECT
public:
    enum ErrorCode {
        NoError,
        SystemNameChangeFailed,
        UpdateFailed
    };

    explicit QnConnectToCurrentSystemTool(QObject *parent = 0);

    void connectToCurrentSystem(const QSet<QnId> &targets);

    bool isRunning() const;

signals:
    void finished(ErrorCode errorCode);

private:
    void finish(ErrorCode errorCode);

private slots:
    void at_changeSystemNameTask_finished(int errorCode, const QSet<QnId> &failedPeers);
    void at_updateTool_stateChanged(int state);

private:
    bool m_running;
    QSet<QnId> m_targets;

    QnChangeSystemNamePeerTask *m_changeSystemNameTask;
    QnUpdateDialog *m_updateDialog;
    QnMediaServerUpdateTool *m_updateTool;
    int m_prevToolState;
};

#endif // CONNECT_TO_CURRENT_SYSTEM_TOOL_H
