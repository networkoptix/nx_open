// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

/**
 * Adds event rules for intercom call and door opening.
 */
class IntercomManager:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit IntercomManager(QObject* parent = nullptr);
    virtual ~IntercomManager() override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
