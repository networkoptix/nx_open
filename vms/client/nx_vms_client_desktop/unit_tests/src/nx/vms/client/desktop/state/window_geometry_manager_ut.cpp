// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/client/desktop/state/window_geometry_manager.h>

#include "mock_window_control_interface.h"

namespace nx::vms::client::desktop {
namespace test {

namespace {

static const WindowGeometryState kExampleState =
    []
    {
        WindowGeometryState result;
        result.geometry = QRect(100, 200, 800, 600);
        return result;
    }();

} // namespace

class WindowGeometryManagerTest:
    public testing::Test
{
public:
    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp() override
    {
        m_windowGeometryManager = std::make_unique<WindowGeometryManager>(
            std::make_unique<MockWindowControlInterface>());
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown() override
    {
        m_windowGeometryManager.reset();
    }

    WindowGeometryManager* windowGeometryManager() const
    {
        return m_windowGeometryManager.get();
    }

private:
    std::unique_ptr<WindowGeometryManager> m_windowGeometryManager;
};

TEST_F(WindowGeometryManagerTest, readWrite)
{
    windowGeometryManager()->setWindowGeometry(kExampleState);
    ASSERT_EQ(windowGeometryManager()->windowGeometry(), kExampleState);
}

} // namespace test
} // namespace nx::vms::client::desktop
