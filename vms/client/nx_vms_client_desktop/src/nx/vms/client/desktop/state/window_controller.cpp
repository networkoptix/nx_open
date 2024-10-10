// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "window_controller.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>

#include <QtWidgets/QWidget>

#include <nx/build_info.h>
#include <nx/vms/client/desktop/state/screen_manager.h>
#include <ui/widgets/main_window.h>

namespace nx::vms::client::desktop {

WindowController::WindowController(MainWindow* window):
    m_window(window)
{
}

QRect WindowController::windowRect() const
{
    return m_window->normalGeometry();
}

void WindowController::setWindowRect(const QRect& rect)
{
    qApp->processEvents();
    m_window->setGeometry(rect);
}

Qt::WindowStates WindowController::windowState() const
{
    return m_window->windowState();
}

void WindowController::setWindowState(Qt::WindowStates state)
{
    m_window->show();
    qApp->processEvents();
    m_window->setWindowState(state);
    if (nx::build_info::isMacOsX())
    {
        if (state == Qt::WindowNoState)
            m_window->action(menu::FullscreenAction)->setChecked(false);
        else if (state.testFlag(Qt::WindowFullScreen))
            m_window->action(menu::FullscreenAction)->setChecked(true);
    }
}

int WindowController::nextFreeScreen() const
{
    return m_window->screenManager()->nextFreeScreen();
}

QList<QRect> WindowController::suitableSurface() const
{
    QList<QRect> result;

    for (const auto screen: QGuiApplication::screens())
    {
        result << screen->geometry(); // TODO: #spanasenko Is it the right method to call?
    }

    return result;
}

int WindowController::windowScreen() const
{
    return QGuiApplication::screens().indexOf(m_window->screen());
}

} //namespace nx::vms::client::desktop
