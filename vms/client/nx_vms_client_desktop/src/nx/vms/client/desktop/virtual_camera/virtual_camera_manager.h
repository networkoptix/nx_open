// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/system_context_aware.h>

#include "virtual_camera_fwd.h"

namespace nx::vms::client::desktop {

class VirtualCameraWorker;

class VirtualCameraManager:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT

public:
    VirtualCameraManager(SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~VirtualCameraManager() override;

    void setCurrentUser(const QnUserResourcePtr& user);
    const QnUserResourcePtr& currentUser() const;

    VirtualCameraState state(const QnVirtualCameraResourcePtr& camera);
    QList<VirtualCameraState> runningUploads();

    void prepareUploads(
        const QnVirtualCameraResourcePtr& camera,
        const QStringList& filePaths,
        QObject* target,
        std::function<void(const VirtualCameraUpload&)> callback);

    void updateState(const QnVirtualCameraResourcePtr& camera);

    bool addUpload(const QnVirtualCameraResourcePtr& camera, const VirtualCameraPayloadList& payloads);
    void cancelUploads(const QnVirtualCameraResourcePtr& camera);
    void cancelAllUploads();

signals:
    void stateChanged(const VirtualCameraState& state);
    void error(const VirtualCameraState& state, const QString& errorMessage);

private:
    VirtualCameraWorker* cameraWorker(const QnVirtualCameraResourcePtr& camera);
    void dropWorker(const QnResourcePtr& resource);
    void dropAllWorkers();

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
