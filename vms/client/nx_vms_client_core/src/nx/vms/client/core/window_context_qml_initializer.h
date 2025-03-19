// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "qml_context_initializer.h"
#include "window_context.h"

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API WindowContextQmlInitializer:
    public QmlContextInitializer<WindowContext>
{
    using base_type = QmlContextInitializer<WindowContext>;

public:
    WindowContextQmlInitializer(QObject* owner);

    static void storeToQmlContext(WindowContext* context);
};

} // namespace nx::vms::client::core
