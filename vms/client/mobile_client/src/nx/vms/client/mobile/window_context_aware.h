// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/window_context_aware.h>

namespace nx::vms::client::mobile {

class SessionManager;
class SystemContext;
class WindowContext;

class WindowContextAware: public core::WindowContextAware
{
    using base_type = core::WindowContextAware;

public:
    WindowContextAware(WindowContext* context);
    WindowContextAware(std::unique_ptr<core::WindowContextQmlInitializer>&& initializer);

    WindowContext* windowContext() const;

    SystemContext* mainSystemContext();

    SessionManager* sessionManager() const;

    SystemContext* createSystemContext();
    void deleteSystemContext(SystemContext* context);
};

} // namespace nx::vms::client::mobile
