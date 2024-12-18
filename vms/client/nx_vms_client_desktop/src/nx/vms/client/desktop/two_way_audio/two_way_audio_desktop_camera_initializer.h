// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/window_context_aware.h>

namespace nx::vms::client::desktop {

class WindowContext;

class NX_VMS_CLIENT_DESKTOP_API TwoWayAudioDesktopCameraInitializer:
    public QObject,
    public WindowContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    TwoWayAudioDesktopCameraInitializer(WindowContext* context, QObject* parent = nullptr);
    virtual ~TwoWayAudioDesktopCameraInitializer();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
