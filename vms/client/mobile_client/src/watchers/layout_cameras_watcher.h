#pragma once

#include <client_core/connection_context_aware.h>

#include <utils/common/connective.h>
#include <utils/common/counter_hash.h>
#include <nx/utils/uuid.h>
#include <core/resource/resource_fwd.h>

namespace nx {
namespace client {
namespace mobile {

class LayoutCamerasWatcher: public Connective<QObject>, public QnConnectionContextAware
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
    QnCounterHash<QnUuid> m_countByCameraId;
};

} // namespace mobile
} // namespace client
} // namespace nx
