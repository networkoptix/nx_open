#pragma once

#include <core/resource/resource_fwd.h>
#include <utils/common/id.h>
#include <utils/common/connective.h>
#include <resources/layout_properties.h>

class QnLiteClientLayoutHelperPrivate;
class QnLiteClientLayoutHelper: public Connective<QObject>
{
    Q_OBJECT

    Q_PROPERTY(QString layoutId READ layoutId WRITE setLayoutId NOTIFY layoutChanged)
    Q_PROPERTY(QnLayoutProperties::DisplayMode displayMode READ displayMode WRITE setDisplayMode NOTIFY displayModeChanged)
    Q_PROPERTY(QString singleCameraId READ singleCameraId WRITE setSingleCameraId NOTIFY singleCameraIdChanged)

    Q_ENUMS(QnLayoutProperties::DisplayMode)

    using base_type = Connective<QObject>;

public:
    QnLiteClientLayoutHelper(QObject* parent = nullptr);
    ~QnLiteClientLayoutHelper();

    QString layoutId() const;
    void setLayoutId(const QString& layoutId);

    QnLayoutResourcePtr layout() const;
    void setLayout(const QnLayoutResourcePtr& layout);

    QnLayoutProperties::DisplayMode displayMode() const;
    void setDisplayMode(QnLayoutProperties::DisplayMode displayMode);

    QString singleCameraId() const;
    void setSingleCameraId(const QString& cameraId);

    Q_INVOKABLE QString cameraIdOnCell(int x, int y) const;
    Q_INVOKABLE void setCameraIdOnCell(int x, int y, const QString& cameraId);

    static QnLayoutResourcePtr createLayoutForServer(const QnUuid& serverId);
    static QnLayoutResourcePtr findLayoutForServer(const QnUuid& serverId);

signals:
    void layoutChanged();
    void displayModeChanged();
    void singleCameraIdChanged();
    void cameraIdChanged(int x, int y, const QString& resourceId);

private:
    Q_DECLARE_PRIVATE(QnLiteClientLayoutHelper)
    QScopedPointer<QnLiteClientLayoutHelperPrivate> d_ptr;
};
