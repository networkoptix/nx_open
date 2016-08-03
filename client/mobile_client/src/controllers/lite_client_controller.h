#pragma once

#include <QtCore/QObject>

#include <utils/common/connective.h>
#include <resources/layout_properties.h>

class QnLiteClientLayoutHelper;

class QnLiteClientControllerPrivate;

class QnLiteClientController : public Connective<QObject>
{
    Q_OBJECT

    Q_PROPERTY(QString serverId READ serverId WRITE setServerId NOTIFY serverIdChanged)
    Q_PROPERTY(State clientState READ clientState NOTIFY clientStateChanged)
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

    QnLiteClientLayoutHelper* layoutHelper() const;

    Q_INVOKABLE void startLiteClient();
    Q_INVOKABLE void stopLiteClient();

signals:
    void serverIdChanged();
    void clientStateChanged();
    void clientStartError();

    // Dummy signal to make QML happy
    void nothingChanged();

private:
    Q_DECLARE_PRIVATE(QnLiteClientController)
    QScopedPointer<QnLiteClientControllerPrivate> d_ptr;
};
