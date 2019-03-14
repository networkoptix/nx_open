#pragma once

#include <QtCore/QObject>

#include <client_core/connection_context_aware.h>

#include <utils/common/connective.h>

namespace nx {
namespace client {
namespace mobile {
namespace resource {

class LiteClientLayoutHelper;

} // namespace resource
} // namespace mobile
} // namespace client
} // namespace nx

class QnLiteClientControllerPrivate;

class QnLiteClientController: public Connective<QObject>, public QnConnectionContextAware
{
    Q_OBJECT

    Q_PROPERTY(QString serverId READ serverId WRITE setServerId NOTIFY serverIdChanged)
    Q_PROPERTY(State clientState READ clientState NOTIFY clientStateChanged)
    Q_PROPERTY(bool serverOnline READ isServerOnline NOTIFY serverOnlineChanged)
    Q_PROPERTY(nx::client::mobile::resource::LiteClientLayoutHelper* layoutHelper
        READ layoutHelper NOTIFY nothingChanged)

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

    nx::client::mobile::resource::LiteClientLayoutHelper* layoutHelper() const;

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
