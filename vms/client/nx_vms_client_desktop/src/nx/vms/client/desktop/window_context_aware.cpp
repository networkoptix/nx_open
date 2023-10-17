// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "window_context_aware.h"

#include <nx/vms/client/desktop/menu/action.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <ui/workbench/workbench_context.h>

#include "current_system_context_aware.h"
#include "window_context.h"

namespace nx::vms::client::desktop {

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
    return m_windowContext.data();;
}

SystemContext* WindowContextAware::system() const
{
    if (auto systemContextAware = dynamic_cast<const CurrentSystemContextAware*>(this))
    {
        NX_ASSERT(systemContextAware->systemContext() == m_windowContext->system(),
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
    return m_windowContext->system();
}

QnWorkbenchContext* WindowContextAware::workbenchContext() const
{
    return m_windowContext->workbenchContext();
}

Workbench* WindowContextAware::workbench() const
{
    return m_windowContext->workbench();
}

QnWorkbenchDisplay* WindowContextAware::display() const
{
    return m_windowContext->workbenchContext()->display();
}

QnWorkbenchNavigator* WindowContextAware::navigator() const
{
    return m_windowContext->navigator();
}

menu::Manager* WindowContextAware::menu() const
{
    return m_windowContext->menu();
}

QAction* WindowContextAware::action(const menu::IDType id) const
{
    return menu()->action(id);
}

MainWindow* WindowContextAware::mainWindow() const
{
    return m_windowContext->workbenchContext()->mainWindow();
}

QWidget* WindowContextAware::mainWindowWidget() const
{
    return m_windowContext->workbenchContext()->mainWindowWidget();
}

} // namespace nx::vms::client::desktop
