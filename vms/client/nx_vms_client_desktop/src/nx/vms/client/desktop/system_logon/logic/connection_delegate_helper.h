// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/vms/client/core/system_logon/remote_connection_user_interaction_delegate.h>

class QWidget;

namespace nx::vms::client::desktop {

/** Create and initialize remote connection user interaction delegate. */
std::unique_ptr<core::RemoteConnectionUserInteractionDelegate>
    createConnectionUserInteractionDelegate(QPointer<QWidget> mainWindow);

} // namespace nx::vms::client::desktop
