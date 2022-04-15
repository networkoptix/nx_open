// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>

#include <nx/vms/client/core/system_context_aware.h>

class QnWorkbenchAccessController;

namespace nx::vms::client::desktop {

class SystemContext;

class SystemContextAware: public nx::vms::client::core::SystemContextAware
{
public:
    using nx::vms::client::core::SystemContextAware::SystemContextAware;

    SystemContext* systemContext() const;

protected:
    QnWorkbenchAccessController* accessController() const;
};

} // namespace nx::vms::client::desktop
