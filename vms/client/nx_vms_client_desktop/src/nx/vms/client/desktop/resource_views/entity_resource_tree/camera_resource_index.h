// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

class QnResourcePool;

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

class CameraResourceIndex: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    CameraResourceIndex(const QnResourcePool* resourcePool);

    QVector<QnResourcePtr> allCameras() const;
    QVector<QnResourcePtr> camerasOnServer(const QnResourcePtr& server) const;

signals:
    void cameraAdded(const QnResourcePtr& camera);
    void cameraRemoved(const QnResourcePtr& camera);

    void cameraAddedToServer(const QnResourcePtr& camera, const QnResourcePtr& server);
    void cameraRemovedFromServer(const QnResourcePtr& camera, const QnResourcePtr& server);

private:
    void onResourceAdded(const QnResourcePtr& resource);
    void onResourceRemoved(const QnResourcePtr& resource);
    void onCameraParentIdChanged(const QnResourcePtr& resource, const nx::Uuid& previousParentId);

private:
    void indexAllCameras();
    void indexCamera(const QnVirtualCameraResourcePtr& camera);

private:
    const QnResourcePool* m_resourcePool;
    QHash<nx::Uuid, QSet<QnResourcePtr>> m_camerasByServer;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
