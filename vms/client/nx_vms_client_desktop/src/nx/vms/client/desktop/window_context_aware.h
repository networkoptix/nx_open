// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>

#include <nx/vms/client/desktop/ui/actions/actions.h>

class QAction;
class QWidget;

class QnWorkbenchDisplay;
class QnWorkbenchNavigator;

namespace nx::vms::client::desktop {

namespace ui::action { class Manager; }

class MainWindow;
class WindowContext;
class Workbench;

class WindowContextAware
{
public:
    WindowContextAware(WindowContext* windowContext);
    ~WindowContextAware();

    WindowContext* windowContext() const;

protected:
    Workbench* workbench() const;

    QnWorkbenchDisplay* display() const;

    QnWorkbenchNavigator* navigator() const;

    ui::action::Manager* menu() const;

    QAction* action(const ui::action::IDType id) const;

    MainWindow* mainWindow() const;

    /**
     * @return The same as mainWindow() but casted to QWidget*, so caller don't need to include
     * MainWindow header.
     */
    QWidget* mainWindowWidget() const;

private:
    QPointer<WindowContext> m_windowContext;
};

} // namespace nx::vms::client::desktop
