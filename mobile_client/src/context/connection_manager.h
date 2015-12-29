#pragma once

#include <QtCore/QObject>

#include <context/context_aware.h>

class QnConnectionManagerPrivate;
class QnConnectionManager : public QObject, public QnContextAware {
    Q_OBJECT
    Q_ENUMS(ConnectionStatus)
    Q_ENUMS(State)

    Q_PROPERTY(QString systemName READ systemName NOTIFY systemNameChanged)
    Q_PROPERTY(State connectionState READ connectionState NOTIFY connectionStateChanged)

public:
    enum ConnectionStatus {
        Success,
        InvalidServer,
        InvalidVersion,
        Unauthorized,
        NetworkError
    };

    enum State {
        Disconnected,
        Connecting,
        Connected,
        Suspended
    };

    explicit QnConnectionManager(QObject *parent = 0);
    ~QnConnectionManager();

    QString systemName() const;

    State connectionState() const;

    Q_INVOKABLE int defaultServerPort() const;

signals:
    void connectionFailed(ConnectionStatus status, const QVariant &infoParameter);
    void systemNameChanged(const QString &systemName);
    void initialResourcesReceived();
    void connectionStateChanged();

public slots:
    void connectToServer(const QUrl &url);
    void disconnectFromServer(bool force);

private:
    QScopedPointer<QnConnectionManagerPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(QnConnectionManager)
};
