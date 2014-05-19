#ifndef CONNECT_TO_CURRENT_SYSTEM_TOOL_H
#define CONNECT_TO_CURRENT_SYSTEM_TOOL_H

#include <QtCore/QObject>

#include <utils/common/id.h>

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
    bool m_running;
    QSet<QnId> m_targets;
};

#endif // CONNECT_TO_CURRENT_SYSTEM_TOOL_H
