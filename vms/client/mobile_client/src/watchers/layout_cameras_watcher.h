// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <utils/common/counter_hash.h>

namespace nx {
namespace client {
namespace mobile {


class LayoutCamerasWatcher: public QObject
{
    Q_OBJECT

    using base_type = QObject;

public:
    explicit LayoutCamerasWatcher(QObject* parent = nullptr);

    QnLayoutResourcePtr layout() const;
    void setLayout(const QnLayoutResourcePtr& layout);

    int count() const;
    QnVirtualCameraResourceList cameras() const;

signals:
    void layoutChanged(const QnLayoutResourcePtr& layout);
    void countChanged();
    void cameraAdded(const QnVirtualCameraResourcePtr& camera);
    void cameraRemoved(const QnVirtualCameraResourcePtr& camera);

private:
    void addCamera(const QnVirtualCameraResourcePtr& camera);
    void removeCamera(const QnVirtualCameraResourcePtr& camera);

private:
    QnLayoutResourcePtr m_layout;
    QnCounterHash<QnVirtualCameraResourcePtr> m_cameras;
};

} // namespace mobile
} // namespace client
} // namespace nx
