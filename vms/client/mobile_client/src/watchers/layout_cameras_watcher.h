// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/mobile/current_system_context_aware.h>
#include <utils/common/counter_hash.h>

namespace nx {
namespace client {
namespace mobile {


class LayoutCamerasWatcher:
    public QObject,
    public nx::vms::client::mobile::CurrentSystemContextAware
{
    Q_OBJECT

    using base_type = QObject;

public:
    explicit LayoutCamerasWatcher(QObject* parent = nullptr);

    QnLayoutResourcePtr layout() const;
    void setLayout(const QnLayoutResourcePtr& layout);

    QHash<nx::Uuid, QnVirtualCameraResourcePtr> cameras() const;
    int count() const;

signals:
    void layoutChanged(const QnLayoutResourcePtr& layout);
    void countChanged();
    void cameraAdded(const QnVirtualCameraResourcePtr& camera);
    void cameraRemoved(const QnVirtualCameraResourcePtr& camera);

private:
    void addCamera(const QnVirtualCameraResourcePtr& camera);
    void removeCamera(const nx::Uuid& cameraId);

private:
    QnLayoutResourcePtr m_layout;
    QHash<nx::Uuid, QnVirtualCameraResourcePtr> m_cameras;
    QnCounterHash<nx::Uuid> m_countByCameraId;
};

} // namespace mobile
} // namespace client
} // namespace nx
