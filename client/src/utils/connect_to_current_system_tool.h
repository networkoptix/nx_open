#ifndef CONNECT_TO_CURRENT_SYSTEM_TOOL_H
#define CONNECT_TO_CURRENT_SYSTEM_TOOL_H

#include <QtCore/QObject>

#include <utils/common/id.h>

class QnChangeSystemNamePeerTask;

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
    void at_changeSystemnameTask_finished(int errorCode, const QSet<QnId> &failedPeers);

private:
    bool m_running;
    QSet<QnId> m_targets;

    QnChangeSystemNamePeerTask *m_changeSystemNameTask;
};

#endif // CONNECT_TO_CURRENT_SYSTEM_TOOL_H
