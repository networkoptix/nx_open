// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "current_system_context_aware.h"

#include <QtCore/QObject>
#include <QtQml/QtQml>
#include <QtWidgets/QGraphicsItem>

#include <nx/vms/client/desktop/utils/context_utils.h>
#include <ui/graphics/view/graphics_scene.h>
#include <ui/widgets/main_window.h>
#include <ui/workbench/workbench_context.h>

#include "application_context.h"
#include "system_context.h"
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

nx::core::Watermark CurrentSystemContextAware::watermark() const
{
    return context()->watermark();
}

} // namespace nx::vms::client::desktop
