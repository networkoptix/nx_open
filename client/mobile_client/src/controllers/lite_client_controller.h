#pragma once

#include <QtCore/QObject>

#include <utils/common/connective.h>
#include <resources/layout_properties.h>

class QnLiteClientControllerPrivate;

class QnLiteClientController : public Connective<QObject>
{
    Q_OBJECT

    Q_PROPERTY(QString serverId READ serverId WRITE setServerId NOTIFY serverIdChanged)
    Q_PROPERTY(bool clientOnline READ clientOnline NOTIFY clientOnlineChanged)
    Q_PROPERTY(QnLayoutProperties::DisplayMode displayMode READ displayMode WRITE setDisplayMode NOTIFY displayModeChanged)
    Q_ENUMS(QnLayoutProperties::DisplayMode)

    using base_type = Connective<QObject>;

public:
    QnLiteClientController(QObject* parent = nullptr);
    ~QnLiteClientController();

    QString serverId() const;
    void setServerId(const QString& serverId);

    bool clientOnline() const;

    QnLayoutProperties::DisplayMode displayMode() const;
    void setDisplayMode(QnLayoutProperties::DisplayMode displayMode);

    Q_INVOKABLE QString singleCameraId() const;
    Q_INVOKABLE void setSingleCameraId(const QString& cameraIdOnCell);
    Q_INVOKABLE QString cameraIdOnCell(int x, int y) const;
    Q_INVOKABLE void setCameraIdOnCell(const QString& cameraIdOnCell, int x, int y);

    Q_INVOKABLE void startLiteClient();
    Q_INVOKABLE void stopLiteClient();

signals:
    void serverIdChanged();
    void clientOnlineChanged();
    void displayModeChanged();

    void clientStartError();

private:
    Q_DECLARE_PRIVATE(QnLiteClientController)
    QScopedPointer<QnLiteClientControllerPrivate> d_ptr;
};
