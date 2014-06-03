#ifndef JOIN_SYSTEM_TOOL_H
#define JOIN_SYSTEM_TOOL_H

#include <QtCore/QObject>
#include <QtNetwork/QHostAddress>

#include <core/resource/resource_fwd.h>

class QTimer;

class QnJoinSystemTool : public QObject {
    Q_OBJECT
public:
    enum ErrorCode {
        NoError,
        Timeout
    };

    explicit QnJoinSystemTool(QObject *parent = 0);

    bool isRunning() const;

    void start(const QUrl &url, const QString &password);

signals:
    void finished(int errorCode);

private:
    void finish(int errorCode);

private slots:
    void at_resource_added(const QnResourcePtr &resource);
    void at_timer_timeout();

private:
    bool m_running;

    QUrl m_targetUrl;
    QString m_password;
    QHostAddress m_address;

    QTimer *m_timer;
};

#endif // JOIN_SYSTEM_TOOL_H
