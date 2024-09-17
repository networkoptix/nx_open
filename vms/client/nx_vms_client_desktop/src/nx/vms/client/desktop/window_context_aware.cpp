// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "window_context_aware.h"

#include <nx/vms/client/desktop/menu/action.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <ui/workbench/workbench_context.h>

#include "current_system_context_aware.h"
#include "window_context.h"

namespace nx::vms::client::desktop {

WindowContextAware::WindowContextAware(WindowContext* windowContext):
    base_type(windowContext)
{
}

WindowContextAware::WindowContextAware(WindowContextAware* windowContextAware):
    WindowContextAware(windowContextAware->windowContext())
{
}

WindowContextAware::~WindowContextAware()
{
}

WindowContext* WindowContextAware::windowContext() const
{
    return base_type::windowContext()->as<WindowContext>();
}

SystemContext* WindowContextAware::system() const
{
    if (auto systemContextAware = dynamic_cast<const CurrentSystemContextAware*>(this))
    {
        NX_ASSERT(systemContextAware->systemContext() == windowContext()->system(),
            "Current system differs from the one class was created with. This class should have "
            "been destroyed before current system is changed. Otherwise implement separate "
            "inheritance from SystemContextAware like in CameraSettingsDialog class.");
    }
    else
    {
        NX_ASSERT(!dynamic_cast<const SystemContextAware *>(this),
            "SystemContextAware classes must use their own systemContext() method to avoid "
            "situation when current system was already changed. If your class should always work "
            "with the current system context only, consider to use CurrentSystemContextAware "
            "helper class instead.");
    }
    return windowContext()->system();
}

QnWorkbenchContext* WindowContextAware::workbenchContext() const
{
    return windowContext()->workbenchContext();
}

Workbench* WindowContextAware::workbench() const
{
    return windowContext()->workbench();
}

QnWorkbenchDisplay* WindowContextAware::display() const
{
    return windowContext()->workbenchContext()->display();
}

QnWorkbenchNavigator* WindowContextAware::navigator() const
{
    return windowContext()->navigator();
}

menu::Manager* WindowContextAware::menu() const
{
    return windowContext()->menu();
}

QAction* WindowContextAware::action(const menu::IDType id) const
{
    return menu()->action(id);
}

MainWindow* WindowContextAware::mainWindow() const
{
    return windowContext()->workbenchContext()->mainWindow();
}

QWidget* WindowContextAware::mainWindowWidget() const
{
    return windowContext()->workbenchContext()->mainWindowWidget();
}

} // namespace nx::vms::client::desktop
