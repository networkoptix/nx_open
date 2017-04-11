#pragma once

#include <QtCore/QObject>

#include <client_core/connection_context_aware.h>

#include <utils/common/connective.h>

class QnLiteClientLayoutHelper;

class QnLiteClientControllerPrivate;

class QnLiteClientController: public Connective<QObject>, public QnConnectionContextAware
{
    Q_OBJECT

    Q_PROPERTY(QString serverId READ serverId WRITE setServerId NOTIFY serverIdChanged)
    Q_PROPERTY(State clientState READ clientState NOTIFY clientStateChanged)
    Q_PROPERTY(bool serverOnline READ isServerOnline NOTIFY serverOnlineChanged)
    Q_PROPERTY(QnLiteClientLayoutHelper* layoutHelper READ layoutHelper NOTIFY nothingChanged)

    using base_type = Connective<QObject>;

public:
    enum class State
    {
        Stopped,
        Starting,
        Started,
        Stopping
    };
    Q_ENUM(State)

    QnLiteClientController(QObject* parent = nullptr);
    ~QnLiteClientController();

    QString serverId() const;
    void setServerId(const QString& serverId);

    State clientState() const;

    bool isServerOnline() const;

    QnLiteClientLayoutHelper* layoutHelper() const;

    Q_INVOKABLE void startLiteClient();
    Q_INVOKABLE void stopLiteClient();

signals:
    void serverIdChanged();
    void serverOnlineChanged();
    void clientStateChanged();
    void clientStartError();
    void clientStopError();

    // Dummy signal to make QML happy
    void nothingChanged();

private:
    Q_DECLARE_PRIVATE(QnLiteClientController)
    QScopedPointer<QnLiteClientControllerPrivate> d_ptr;
};
