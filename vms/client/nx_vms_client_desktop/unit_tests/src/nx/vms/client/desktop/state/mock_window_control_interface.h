// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QRect>

#include <nx/vms/client/desktop/state/window_geometry_manager.h>

namespace nx::vms::client::desktop {
namespace test {

class MockWindowControlInterface: public WindowControlInterface
{
public:
    virtual QRect windowRect() const override
    {
        return m_screenRect;
    }

    virtual void setWindowRect(const QRect& rect) override
    {
        m_screenRect = rect;
    }

    virtual Qt::WindowStates windowState() const override
    {
        return m_state;
    }

    virtual void setWindowState(Qt::WindowStates state) override
    {
        m_state = state;
    }

    virtual int nextFreeScreen() const override
    {
        return -1;
    }

    virtual QList<QRect> suitableSurface() const override
    {
        return QList<QRect>() << QRect{0, 0, 1920, 1080};
    }

    virtual int windowScreen() const override
    {
        return 0;
    }

private:
    QRect m_screenRect = {0, 0, 800, 600};
    Qt::WindowStates m_state = Qt::WindowNoState;
};

inline std::unique_ptr<WindowGeometryManager> makeMockWindowGeometryManager()
{
    return std::make_unique<WindowGeometryManager>(std::make_unique<MockWindowControlInterface>());
}

} // namespace test
} //namespace nx::vms::client::desktop
