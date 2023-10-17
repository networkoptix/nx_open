// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QSet>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

class DefaultPasswordCamerasWatcher:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit DefaultPasswordCamerasWatcher(
        SystemContext* systemContext,
        QObject* parent = nullptr);
    virtual ~DefaultPasswordCamerasWatcher() override;

    QnVirtualCameraResourceSet camerasWithDefaultPassword() const;

signals:
    void cameraSetChanged();

private:
    void handleResourceAdded(const QnResourcePtr& resource);
    void handleResourceRemoved(const QnResourcePtr& resource);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
