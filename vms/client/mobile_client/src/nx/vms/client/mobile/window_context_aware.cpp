// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "window_context_aware.h"

#include <nx/vms/client/mobile/window_context.h>

namespace nx::vms::client::mobile {

WindowContextAware::WindowContextAware(WindowContext* context):
    base_type(context)
{
}

WindowContextAware::WindowContextAware(
    std::unique_ptr<core::WindowContextQmlInitializer>&& initializer)
    :
    base_type(std::move(initializer))
{
}

WindowContext* WindowContextAware::windowContext() const
{
    return core::WindowContextAware::windowContext()->as<WindowContext>();
}

SystemContext* WindowContextAware::mainSystemContext()
{
    return windowContext()->mainSystemContext();
}

SessionManager* WindowContextAware::sessionManager() const
{
    return windowContext()->sessionManager();
}

UiController* WindowContextAware::uiController() const
{
    return windowContext()->uiController();
}

} // namespace nx::vms::client::mobile
