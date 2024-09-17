// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "window_context.h"

#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/system_context.h>

namespace nx::vms::client::core {

WindowContext::WindowContext(QObject* parent):
    QObject(parent)
{
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

} // namespace nx::vms::client::core
