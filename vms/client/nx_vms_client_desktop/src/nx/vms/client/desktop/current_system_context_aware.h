// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/core/watermark/watermark.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <nx/vms/client/desktop/window_context_aware.h>

namespace nx::vms::client::desktop {

/**
 * Convinience class for access to both Window Context and System Context. Can inherit access from
 * the QObject parentship chain. It strongly depends on both contexts and must be destroyed before
 * any of contexts is.
 */
class CurrentSystemContextAware: public SystemContextAware, public WindowContextAware
{
public:
    /**
    * Constructor.
    *
    * Technical debt, this constructor should be removed in the future.
    *
    * @param parent Parent for this object. Must itself be a child of a CurrentSystemContextAware
    *     object or WindowContextAware object. Must not be null.
    */
    CurrentSystemContextAware(QObject* parent);

    /**
     * Constructor.
     *
     * @param parent Parent for this object. Must itself be a child of a CurrentSystemContextAware
     *     object or WindowContextAware object. Must not be null.
     */
    CurrentSystemContextAware(QWidget* parent);

    /**
     * Constructor.
     *
     * @param windowContext Actual window context. Must not be null.
     */
    CurrentSystemContextAware(WindowContext* windowContext);

    /**
     * Virtual destructor.
     *
     * We do dynamic_casts to CurrentSystemContextAware, so this class must have a vtable.
     */
    virtual ~CurrentSystemContextAware();

    /**
     * @returns Context associated with this context-aware object.
     */
    QnWorkbenchContext* context() const;

    nx::core::Watermark watermark() const;
};

} // namespace nx::vms::client::desktop

using QnWorkbenchContextAware = nx::vms::client::desktop::CurrentSystemContextAware;
