#pragma once

#include <utils/common/connective.h>
#include <nx/utils/uuid.h>
#include <core/resource/resource_fwd.h>

namespace nx {
namespace client {
namespace mobile {

class LayoutCamerasWatcher: public Connective<QObject>
{
    Q_OBJECT

    using base_type = Connective<QObject>;

public:
    explicit LayoutCamerasWatcher(QObject* parent = nullptr);

    QnLayoutResourcePtr layout() const;
    void setLayout(const QnLayoutResourcePtr& layout);

    QHash<QnUuid, QnVirtualCameraResourcePtr> cameras() const;
    int count() const;

signals:
    void layoutChanged(const QnLayoutResourcePtr& layout);
    void countChanged();
    void cameraAdded(const QnVirtualCameraResourcePtr& camera);
    void cameraRemoved(const QnVirtualCameraResourcePtr& camera);

private:
    void addCamera(const QnVirtualCameraResourcePtr& camera);
    void removeCamera(const QnUuid& cameraId);

private:
    QnLayoutResourcePtr m_layout;
    QHash<QnUuid, QnVirtualCameraResourcePtr> m_cameras;
};

} // namespace mobile
} // namespace client
} // namespace nx
