// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>

class QnAvailableCamerasWatcherPrivate;

class NX_VMS_CLIENT_CORE_API QnAvailableCamerasWatcher:
    public QObject,
    public nx::vms::client::core::CommonModuleAware
{
    Q_OBJECT

    using base_type = QObject;

public:
    QnAvailableCamerasWatcher(QObject* parent = nullptr);
    ~QnAvailableCamerasWatcher();

    QnUserResourcePtr user() const;
    void setUser(const QnUserResourcePtr& user);

    QnVirtualCameraResourceList availableCameras() const;

    bool compatibilityMode() const;
    void setCompatiblityMode(bool compatibilityMode);

signals:
    void cameraAdded(const QnResourcePtr& resource);
    void cameraRemoved(const QnResourcePtr& resource);

private:
    QScopedPointer<QnAvailableCamerasWatcherPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(QnAvailableCamerasWatcher)
};
