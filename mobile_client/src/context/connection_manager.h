#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include <QtCore/QObject>

#include <context/context_aware.h>

class QnConnectionManager : public QObject, public QnContextAware {
    Q_OBJECT
    Q_ENUMS(ConnectionStatus)
public:
    enum ConnectionStatus {
        Success,
        InvalidServer,
        InvalidVersion,
        Unauthorized,
        NetworkError
    };

    explicit QnConnectionManager(QObject *parent = 0);

    bool connected() const;

signals:
    void connected(const QUrl &url);
    void connectionFailed(const QUrl &url, ConnectionStatus status);
    void disconnected();

public slots:
    void connect(const QUrl &url);
    void disconnect(bool force);

private:
    bool connectToServer(const QUrl &url);
    bool disconnectFromServer(bool force);

private:
    bool m_connected;
    int m_connectHandle;
};

#endif // CONNECTION_MANAGER_H
