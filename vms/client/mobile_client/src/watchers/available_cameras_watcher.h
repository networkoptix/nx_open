// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/core/system_context_aware.h>

class QnAvailableCamerasWatcherPrivate;

class QnAvailableCamerasWatcher: public QObject, public nx::vms::client::core::SystemContextAware
{
    Q_OBJECT

    using base_type = QObject;

public:
    QnAvailableCamerasWatcher(
        nx::vms::client::core::SystemContext* systemContext,
        QObject* parent = nullptr);
    virtual ~QnAvailableCamerasWatcher() override;

    QnUserResourcePtr user() const;
    void setUser(const QnUserResourcePtr& user);

    QnVirtualCameraResourceList availableCameras() const;

    bool compatibilityMode() const;
    void setCompatibilityMode(bool compatibilityMode);

signals:
    void cameraAdded(const QnResourcePtr& resource);
    void cameraRemoved(const QnResourcePtr& resource);

private:
    QScopedPointer<QnAvailableCamerasWatcherPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(QnAvailableCamerasWatcher)
};
