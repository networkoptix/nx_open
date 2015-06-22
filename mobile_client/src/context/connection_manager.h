#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include <QtCore/QObject>

#include <context/context_aware.h>

class QnConnectionManager : public QObject, public QnContextAware {
    Q_OBJECT
    Q_ENUMS(ConnectionStatus)

    Q_PROPERTY(QString systemName READ systemName NOTIFY systemNameChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)

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
    QString systemName() const;

signals:
    void connectionFailed(ConnectionStatus status, const QString &statusMessage);
    void systemNameChanged(const QString &systemName);
    void initialResourcesReceived();
    void connectedChanged();

public slots:
    bool connectToServer(const QUrl &url);
    bool disconnectFromServer(bool force);

private:
};

#endif // CONNECTION_MANAGER_H
