// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/log/assert.h>

#include "window_context_aware.h"

namespace nx::vms::client::core {

/**
 * Context of the application window. Created once per each window in scope of the application
 * process. Each Window Context has access to all System Contexts, but only one System Context is
 * considered as "current" - which is used to display Resource Tree and other system-dependent UI
 * elements.
 */
class NX_VMS_CLIENT_CORE_API WindowContext: public QObject
{
    Q_OBJECT

public:
    /**
     * Initialize core client-level Window Context.
     * Destruction order must be handled by the caller.
     */
    WindowContext(QObject* parent = nullptr);
    virtual ~WindowContext() override;

    template<typename Target>
    Target* as()
    {
        if (const auto result = qobject_cast<Target*>(this))
            return result;

        NX_ASSERT(false, "Can't cast window context.");
        return nullptr;
    }

    /** System Context, which is selected as current in the given window. */
    SystemContext* system() const;
};

} // namespace nx::vms::client::core

Q_DECLARE_METATYPE(nx::vms::client::core::WindowContext)
Q_DECLARE_OPAQUE_POINTER(nx::vms::client::core::SystemContext*);
