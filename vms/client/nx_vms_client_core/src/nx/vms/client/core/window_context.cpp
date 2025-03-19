// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "window_context.h"

#include <nx/vms/client/core/application_context.h>

#include "window_context_qml_initializer.h"

namespace nx::vms::client::core {

WindowContext::WindowContext(QObject* parent):
    QObject(parent)
{
    WindowContextQmlInitializer::storeToQmlContext(this);
}

WindowContext::~WindowContext()
{
}

SystemContext* WindowContext::system() const
{
    // Later on each Window Context will have it's own current system context and be able to switch
    // between them. For now we have only one current system context available.
    return appContext()->currentSystemContext();
}

std::unique_ptr<WindowContextQmlInitializer> WindowContext::fromQmlContext(QObject* owner)
{
    return std::make_unique<WindowContextQmlInitializer>(owner);
}

} // namespace nx::vms::client::core
