// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QRect>

#include <nx/vms/client/desktop/state/window_geometry_manager.h>

namespace nx::vms::client::desktop {

class MainWindow;

class WindowController: public WindowControlInterface
{
public:
    WindowController(MainWindow* window);

    virtual QRect windowRect() const override;
    virtual void setWindowRect(const QRect& rect) override;

    virtual Qt::WindowStates windowState() const override;
    virtual void setWindowState(Qt::WindowStates state) override;

    virtual int nextFreeScreen() const override;
    virtual QList<QRect> suitableSurface() const override;
    virtual int windowScreen() const override;

private:
    MainWindow* const m_window;
};

} //namespace nx::vms::client::desktop
