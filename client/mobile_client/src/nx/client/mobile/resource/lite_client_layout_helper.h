#pragma once

#include <nx/client/core/resource/layout_accessor.h>
#include <utils/common/id.h>

namespace nx {
namespace client {
namespace mobile {
namespace resource {

class LiteClientLayoutHelper: public nx::client::core::resource::LayoutAccessor
{
    Q_OBJECT

    Q_PROPERTY(QPoint displayCell READ displayCell WRITE setDisplayCell NOTIFY displayCellChanged)
    Q_PROPERTY(DisplayMode displayMode READ displayMode NOTIFY displayCellChanged)
    Q_PROPERTY(QString singleCameraId READ singleCameraId WRITE setSingleCameraId NOTIFY singleCameraIdChanged)

    using base_type = nx::client::core::resource::LayoutAccessor;

public:
    enum class DisplayMode
    {
        SingleCamera,
        MultipleCameras
    };
    Q_ENUM(DisplayMode)

    LiteClientLayoutHelper(QObject* parent = nullptr);
    ~LiteClientLayoutHelper();

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
    void displayCellChanged();
    void singleCameraIdChanged();
    void cameraIdChanged(int x, int y, const QString& resourceId);

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace resource
} // namespace mobile
} // namespace client
} // namespace nx
