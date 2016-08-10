#pragma once

#include <QtCore/QObject>

#include <utils/common/software_version.h>

#include "context_aware.h"

class QnConnectionManagerPrivate;
class QnConnectionManager : public QObject, public QnContextAware {
    Q_OBJECT

    Q_PROPERTY(QString systemName READ systemName NOTIFY systemNameChanged)
    Q_PROPERTY(State connectionState READ connectionState NOTIFY connectionStateChanged)
    Q_PROPERTY(bool online READ isOnline NOTIFY isOnlineChanged)
    Q_PROPERTY(ConnectionType connectionType READ connectionType NOTIFY connectionTypeChanged)
    Q_PROPERTY(QUrl currentUrl READ currentUrl NOTIFY currentUrlChanged)
    Q_PROPERTY(QString currentHost READ currentHost NOTIFY currentHostChanged)
    Q_PROPERTY(QString currentLogin READ currentLogin NOTIFY currentLoginChanged)
    Q_PROPERTY(QString currentPassword READ currentPassword NOTIFY currentPasswordChanged)
    Q_PROPERTY(bool initialResourcesReceived READ initialResourcesReceived NOTIFY initialResourcesReceivedChanged)

public:
    enum ConnectionStatus {
        Success,
        InvalidServer,
        InvalidVersion,
        Unauthorized,
        NetworkError
    };
    Q_ENUMS(ConnectionStatus)

    enum State {
        Disconnected,
        Connecting,
        Connected,
        Suspended
    };
    Q_ENUMS(State)

    enum ConnectionType
    {
        NormalConnection,
        CloudConnection,
        LiteClientConnection
    };
    Q_ENUM(ConnectionType)

    explicit QnConnectionManager(QObject* parent = nullptr);
    ~QnConnectionManager();

    QString systemName() const;

    State connectionState() const;
    bool isOnline() const;
    ConnectionType connectionType() const;

    bool initialResourcesReceived() const;

    Q_INVOKABLE int defaultServerPort() const;

    QUrl currentUrl() const;
    QString currentHost() const;
    QString currentLogin() const;
    QString currentPassword() const;

    QnSoftwareVersion connectionVersion() const;

signals:
    void connectionFailed(ConnectionStatus status, const QVariant &infoParameter);
    void systemNameChanged(const QString &systemName);
    void initialResourcesReceivedChanged();
    void connectionStateChanged();
    void isOnlineChanged();

    void currentUrlChanged();
    void currentHostChanged();
    void currentLoginChanged();
    void currentPasswordChanged();
    void connectionTypeChanged();

    void connectionVersionChanged();

public slots:
    void connectToServer(const QUrl &url);
    void disconnectFromServer(bool force);

private:
    QScopedPointer<QnConnectionManagerPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(QnConnectionManager)
};
