#pragma once

#include <client_core/connection_context_aware.h>

#include <core/resource/resource_fwd.h>
#include <utils/common/id.h>
#include <utils/common/connective.h>

class QnLiteClientLayoutHelperPrivate;
class QnLiteClientLayoutHelper: public Connective<QObject>, public QnConnectionContextAware
{
    Q_OBJECT

    Q_PROPERTY(QString layoutId READ layoutId WRITE setLayoutId NOTIFY layoutChanged)
    Q_PROPERTY(QPoint displayCell READ displayCell WRITE setDisplayCell NOTIFY displayCellChanged)
    Q_PROPERTY(DisplayMode displayMode READ displayMode NOTIFY displayCellChanged)
    Q_PROPERTY(QString singleCameraId READ singleCameraId WRITE setSingleCameraId NOTIFY singleCameraIdChanged)

    using base_type = Connective<QObject>;

public:
    enum class DisplayMode
    {
        SingleCamera,
        MultipleCameras
    };
    Q_ENUM(DisplayMode)

    QnLiteClientLayoutHelper(QObject* parent = nullptr);
    ~QnLiteClientLayoutHelper();

    QString layoutId() const;
    void setLayoutId(const QString& layoutId);

    QnLayoutResourcePtr layout() const;
    void setLayout(const QnLayoutResourcePtr& layout);

    QPoint displayCell() const;
    void setDisplayCell(const QPoint& cell);

    DisplayMode displayMode() const;

    QString singleCameraId() const;
    void setSingleCameraId(const QString& cameraId);

    Q_INVOKABLE QString cameraIdOnCell(int x, int y) const;
    Q_INVOKABLE void setCameraIdOnCell(int x, int y, const QString& cameraId);

    QnLayoutResourcePtr createLayoutForServer(const QnUuid& serverId) const;
    QnLayoutResourcePtr findLayoutForServer(const QnUuid& serverId) const;

signals:
    void layoutChanged();
    void displayCellChanged();
    void singleCameraIdChanged();
    void cameraIdChanged(int x, int y, const QString& resourceId);

private:
    Q_DECLARE_PRIVATE(QnLiteClientLayoutHelper)
    QScopedPointer<QnLiteClientLayoutHelperPrivate> d_ptr;
};
