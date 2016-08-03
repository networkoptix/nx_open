#pragma once

#include <core/resource/resource_fwd.h>
#include <utils/common/id.h>
#include <resources/layout_properties.h>

class QnLiteClientLayoutHelperPrivate;
class QnLiteClientLayoutHelper: public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString layoutId READ layoutId WRITE setLayoutId NOTIFY layoutChanged)
    Q_PROPERTY(QnLayoutProperties::DisplayMode displayMode READ displayMode WRITE setDisplayMode NOTIFY displayModeChanged)
    Q_ENUMS(QnLayoutProperties::DisplayMode)

    using base_type = QObject;

public:
    QnLiteClientLayoutHelper(QObject* parent = nullptr);
    ~QnLiteClientLayoutHelper();

    QString layoutId() const;
    void setLayoutId(const QString& layoutId);

    QnLayoutResourcePtr layout() const;
    void setLayout(const QnLayoutResourcePtr& layout);

    QnLayoutProperties::DisplayMode displayMode() const;
    void setDisplayMode(QnLayoutProperties::DisplayMode displayMode);

    Q_INVOKABLE QString singleCameraId() const;
    Q_INVOKABLE void setSingleCameraId(const QString& cameraId);
    Q_INVOKABLE QString cameraIdOnCell(int x, int y) const;
    Q_INVOKABLE void setCameraIdOnCell(const QString& cameraId, int x, int y);

    static QnLayoutResourcePtr createLayoutForServer(const QnUuid& serverId);
    static QnLayoutResourcePtr findLayoutForServer(const QnUuid& serverId);

signals:
    void layoutChanged();
    void displayModeChanged();

private:
    Q_DECLARE_PRIVATE(QnLiteClientLayoutHelper)
    QScopedPointer<QnLiteClientLayoutHelperPrivate> d_ptr;
};
