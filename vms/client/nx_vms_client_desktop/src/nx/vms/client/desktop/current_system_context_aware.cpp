// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "current_system_context_aware.h"

#include <QtWidgets/QWidget>

#include <nx/vms/client/desktop/utils/context_utils.h>

#include "window_context.h"

namespace nx::vms::client::desktop {

CurrentSystemContextAware::CurrentSystemContextAware(QObject* parent):
    CurrentSystemContextAware(utils::windowContextFromObject(parent))
{
}

CurrentSystemContextAware::CurrentSystemContextAware(QWidget* parent):
    CurrentSystemContextAware(utils::windowContextFromObject(parent))
{
}

CurrentSystemContextAware::CurrentSystemContextAware(
    WindowContext* windowContext)
    :
    SystemContextAware(windowContext->system()),
    WindowContextAware(windowContext)
{
}

CurrentSystemContextAware::~CurrentSystemContextAware()
{
}

QnWorkbenchContext* CurrentSystemContextAware::context() const
{
    return windowContext()->workbenchContext();
}

} // namespace nx::vms::client::desktop
