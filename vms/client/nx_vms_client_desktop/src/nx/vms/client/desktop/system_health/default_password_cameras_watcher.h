// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSet>

#include <core/resource/resource_fwd.h>
#include <common/common_module_aware.h>

namespace nx::vms::client::desktop {

class DefaultPasswordCamerasWatcher:
    public QObject,
    public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit DefaultPasswordCamerasWatcher(QObject* parent = nullptr);
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
