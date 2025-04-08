// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "window_context_aware.h"

#include "system_context_aware.h"
#include "window_context.h"

namespace nx::vms::client::core {

struct WindowContextAware::Private
{
    mutable std::unique_ptr<WindowContextQmlInitializer> delayedInitializer;
    mutable QPointer<WindowContext> windowContext;
};

//-------------------------------------------------------------------------------------------------

WindowContextAware::WindowContextAware(WindowContext* windowContext):
    d(new Private{.windowContext = windowContext})
{
    initializeSafetyChecks(windowContext);
}

WindowContextAware::WindowContextAware(WindowContextAware* windowContextAware):
    WindowContextAware(windowContextAware->windowContext())
{
}

WindowContextAware::WindowContextAware(std::unique_ptr<WindowContextQmlInitializer>&& initializer):
    d(new Private{.delayedInitializer = std::move(initializer)})
{
}

WindowContextAware::~WindowContextAware()
{
    NX_ASSERT(d->windowContext || d->delayedInitializer,
        "Context-aware object must be destroyed before the corresponding context is.");
}

WindowContext* WindowContextAware::windowContext() const
{
    if (!d->windowContext && NX_ASSERT(d->delayedInitializer))
    {
        d->windowContext = d->delayedInitializer->context();
        d->delayedInitializer.reset();
        NX_ASSERT(d->windowContext, "Initialization failed");

        initializeSafetyChecks(d->windowContext.data());
    }
    return d->windowContext.data();
}

SystemContext* WindowContextAware::system() const
{
    NX_ASSERT(!dynamic_cast<const SystemContextAware *>(this),
        "SystemContextAware classes must use their own systemContext() method to avoid "
        "situation when current system was already changed.");
    return d->windowContext->system();
}

void WindowContextAware::initializeSafetyChecks(WindowContext* context) const
{
    if (auto qobject = dynamic_cast<const QObject*>(this))
    {
        QObject::connect(context, &QObject::destroyed, qobject,
            [qobject]()
            {
                NX_ASSERT(false,
                    "Context-aware object must be destroyed before the corresponding context is. %1",
                    qobject);
            });
    }
}

} // namespace nx::vms::client::core
