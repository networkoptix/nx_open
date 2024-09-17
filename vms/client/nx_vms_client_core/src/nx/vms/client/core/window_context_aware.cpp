// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "window_context_aware.h"

#include "system_context_aware.h"
#include "window_context.h"

namespace nx::vms::client::core {

WindowContextAware::WindowContextAware(WindowContext* windowContext):
    m_windowContext(windowContext)
{
    if (auto qobject = dynamic_cast<const QObject*>(this))
    {
        QObject::connect(windowContext, &QObject::destroyed, qobject,
            [this]()
            {
                NX_ASSERT(false,
                    "Context-aware object must be destroyed before the corresponding context is.");
            });
    }
}

WindowContextAware::WindowContextAware(WindowContextAware* windowContextAware):
    WindowContextAware(windowContextAware->windowContext())
{
}

WindowContextAware::~WindowContextAware()
{
    NX_ASSERT(m_windowContext,
        "Context-aware object must be destroyed before the corresponding context is.");
}

WindowContext* WindowContextAware::windowContext() const
{
    return m_windowContext.data();
}

SystemContext* WindowContextAware::system() const
{
    NX_ASSERT(!dynamic_cast<const SystemContextAware *>(this),
        "SystemContextAware classes must use their own systemContext() method to avoid "
        "situation when current system was already changed.");
    return m_windowContext->system();
}

} // namespace nx::vms::client::core
