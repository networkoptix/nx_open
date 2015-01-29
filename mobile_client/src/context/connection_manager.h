#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include <QtCore/QObject>

#include <context/context_aware.h>

class QnConnectionManager : public QObject, public QnContextAware {
    Q_OBJECT
    Q_ENUMS(ConnectionStatus)

    Q_PROPERTY(QString systemName READ systemName NOTIFY systemNameChanged)
public:
    enum ConnectionStatus {
        Success,
        InvalidServer,
        InvalidVersion,
        Unauthorized,
        NetworkError
    };

    explicit QnConnectionManager(QObject *parent = 0);

    bool isConnected() const;
    QString systemName() const;

signals:
    void connected(const QUrl &url);
    void connectionFailed(const QUrl &url, ConnectionStatus status);
    void disconnected();
    void systemNameChanged(const QString &systemName);

public slots:
    bool connectToServer(const QUrl &url);
    bool disconnectFromServer(bool force);

private:
    bool m_connected;
    int m_connectHandle;
};

#endif // CONNECTION_MANAGER_H
