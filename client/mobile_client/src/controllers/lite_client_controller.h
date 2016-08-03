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
    Q_PROPERTY(bool clientOnline READ clientOnline NOTIFY clientOnlineChanged)

    using base_type = Connective<QObject>;

public:
    QnLiteClientController(QObject* parent = nullptr);
    ~QnLiteClientController();

    QString serverId() const;
    void setServerId(const QString& serverId);

    bool clientOnline() const;

    Q_INVOKABLE QnLiteClientLayoutHelper* layoutHelper() const;

    Q_INVOKABLE void startLiteClient();
    Q_INVOKABLE void stopLiteClient();

signals:
    void serverIdChanged();
    void clientOnlineChanged();
    void clientStartError();

private:
    Q_DECLARE_PRIVATE(QnLiteClientController)
    QScopedPointer<QnLiteClientControllerPrivate> d_ptr;
};
