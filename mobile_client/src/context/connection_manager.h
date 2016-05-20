#pragma once

#include <QtCore/QObject>

#include "context_aware.h"

class QnConnectionManagerPrivate;
class QnConnectionManager : public QObject, public QnContextAware {
    Q_OBJECT
    Q_ENUMS(ConnectionStatus)
    Q_ENUMS(State)

    Q_PROPERTY(QString systemName READ systemName NOTIFY systemNameChanged)
    Q_PROPERTY(State connectionState READ connectionState NOTIFY connectionStateChanged)
    Q_PROPERTY(bool isOnline READ isOnline NOTIFY isOnlineChanged)
    Q_PROPERTY(QUrl currentUrl READ currentUrl NOTIFY currentUrlChanged)
    Q_PROPERTY(QString currentHost READ currentHost NOTIFY currentHostChanged)
    Q_PROPERTY(int currentPort READ currentPort NOTIFY currentPortChanged)
    Q_PROPERTY(QString currentLogin READ currentLogin NOTIFY currentLoginChanged)
    Q_PROPERTY(QString currentPassword READ currentPassword NOTIFY currentPasswordChanged)

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
    bool isOnline() const;

    Q_INVOKABLE int defaultServerPort() const;

    QUrl currentUrl() const;
    QString currentHost() const;
    int currentPort() const;
    QString currentLogin() const;
    QString currentPassword() const;

signals:
    void connectionFailed(ConnectionStatus status, const QVariant &infoParameter);
    void systemNameChanged(const QString &systemName);
    void initialResourcesReceived();
    void connectionStateChanged();
    void isOnlineChanged();

    void currentUrlChanged();
    void currentHostChanged();
    void currentPortChanged();
    void currentLoginChanged();
    void currentPasswordChanged();

public slots:
    void connectToServer(const QUrl &url);
    void disconnectFromServer(bool force);

private:
    QScopedPointer<QnConnectionManagerPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(QnConnectionManager)
};
