// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>

#include "window_context_qml_initializer.h"

namespace nx::vms::client::core {

namespace menu { class Manager; }

class SystemContext;
class WindowContext;

class NX_VMS_CLIENT_CORE_API WindowContextAware
{
public:
    WindowContextAware(WindowContext* windowContext);
    WindowContextAware(WindowContextAware* windowContextAware);
    WindowContextAware(std::unique_ptr<WindowContextQmlInitializer>&& initializer);

    virtual ~WindowContextAware();

    WindowContext* windowContext() const;

    /** System Context, which is selected as current in the given window. */
    SystemContext* system() const;

private:
    void initializeSafetyChecks(WindowContext* context) const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
