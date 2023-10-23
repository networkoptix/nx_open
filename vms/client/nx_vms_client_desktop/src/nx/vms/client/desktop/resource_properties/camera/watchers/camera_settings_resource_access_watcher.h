// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

namespace nx::vms::client::desktop {
namespace menu {
class Manager;
} // namespace menu

class CameraSettingsDialogStore;
class SystemContext;

class CameraSettingsResourceAccessWatcher: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    CameraSettingsResourceAccessWatcher(CameraSettingsDialogStore* store,
        SystemContext* systemContext,
        menu::Manager* menu,
        QObject* parent);
    virtual ~CameraSettingsResourceAccessWatcher() override;

    void setCamera(const QnVirtualCameraResourcePtr& camera);

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace nx::vms::client::desktop
