// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "window_controller.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>

#include <QtWidgets/QWidget>

#include <nx/build_info.h>
#include <nx/vms/client/desktop/menu/action.h>
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

QSize WindowController::minimumSize() const
{
    return m_window->minimumSize();
}

void WindowController::setMinimumSize(const QSize& size)
{
    m_window->setMinimumSize(size);
}

Qt::WindowStates WindowController::windowState() const
{
    return m_window->windowState();
}

void WindowController::setWindowState(Qt::WindowStates state)
{
    // Do not call m_window->show() + qApp->processEvents() here. State restoration runs from
    // ClientStateHandler::clientStarted before QApplication::exec(); a synchronous show + event
    // flush forces the QQuickWidget hosting the main scene to render its first frame before
    // QQuickWidget's QRhi has been propagated to the offscreen QQuickWindow. On configurations
    // with slow Vulkan device enumeration (multi-ICD hosts, NVIDIA hybrid GPUs) that produced a
    // null QRhi at QQuickWindow::beforeSynchronizing and crashed RhiRenderingItem.
    // Setting the desired state before the first show lets Qt apply it natively when the
    // window is realized.
    if (m_window->windowState().testFlag(Qt::WindowFullScreen)
        != state.testFlag(Qt::WindowFullScreen))
    {
        m_window->action(menu::FullscreenAction)->setChecked(state.testFlag(Qt::WindowFullScreen));
    }

    m_window->setWindowState(state);
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
