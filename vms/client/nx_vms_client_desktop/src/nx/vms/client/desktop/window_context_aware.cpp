// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "window_context_aware.h"

#include <ui/workbench/workbench_context.h>

#include "window_context.h"

namespace nx::vms::client::desktop {

WindowContextAware::WindowContextAware(WindowContext* windowContext):
    m_windowContext(windowContext)
{
}

WindowContextAware::~WindowContextAware()
{
}

WindowContext* WindowContextAware::windowContext() const
{
    return m_windowContext.data();;
}

Workbench* WindowContextAware::workbench() const
{
    return m_windowContext->workbenchContext()->workbench();
}

QnWorkbenchDisplay* WindowContextAware::display() const
{
    return m_windowContext->workbenchContext()->display();
}

QnWorkbenchNavigator* WindowContextAware::navigator() const
{
    return m_windowContext->workbenchContext()->navigator();
}

ui::action::Manager* WindowContextAware::menu() const
{
    return m_windowContext->workbenchContext()->menu();
}

QAction* WindowContextAware::action(const ui::action::IDType id) const
{
    return m_windowContext->workbenchContext()->action(id);
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
