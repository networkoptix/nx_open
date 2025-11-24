// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/fusion/serialization/json_functions.h>
#include <nx/vms/client/desktop/state/startup_parameters.h>
#include <nx/vms/client/desktop/state/window_geometry_manager.h>

#include "mock_window_control_interface.h"

namespace nx::vms::client::desktop {
namespace test {

namespace {

const auto kDefaultGeometry = QRect(100, 200, 800, 600);
const auto kSpecificGeometry = QRect(200, 300, 800, 600);

const WindowGeometryState kNormalState = { kDefaultGeometry, false, false };
const WindowGeometryState kFullscreenState = { kDefaultGeometry, true, true };

// These coordinates should match the ones in MockWindowControlInterface.
const QRect kSecondScreen = QRect(1920, 0, 1920, 1080);

// This function is taken "as is" from WindowGeometryManager implementation.
DelegateState serialized(const WindowGeometryState& state)
{
    DelegateState result;
    QJson::deserialize(
        QString::fromStdString(nx::reflect::json::serialize(state)),
        &result["geometry"]);
    return result;
}

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

    void givenStateWithParams(const WindowGeometryState& state, const StartupParameters& params)
    {
        windowGeometryManager()->loadState(
            serialized(state), ClientStateDelegate::Substate::systemIndependentParameters, params);
    }

    WindowGeometryState resultState() const
    {
        return windowGeometryManager()->windowGeometry();
    }

private:
    std::unique_ptr<WindowGeometryManager> m_windowGeometryManager;
};

TEST_F(WindowGeometryManagerTest, readWrite)
{
    windowGeometryManager()->setWindowGeometry(kNormalState);
    ASSERT_EQ(resultState(), kNormalState);
}

TEST_F(WindowGeometryManagerTest, noParams)
{
    givenStateWithParams(kNormalState, StartupParameters());
    ASSERT_EQ(resultState(), kNormalState);

    givenStateWithParams(kFullscreenState, StartupParameters());
    ASSERT_EQ(resultState(), kFullscreenState);
}

TEST_F(WindowGeometryManagerTest, noFullscreen)
{
    StartupParameters params;
    params.noFullScreen = true;

    givenStateWithParams(kNormalState, params);
    ASSERT_FALSE(resultState().isFullscreen);
    ASSERT_FALSE(resultState().isMaximized);

    givenStateWithParams(kFullscreenState, params);
    ASSERT_FALSE(resultState().isFullscreen);
    ASSERT_FALSE(resultState().isMaximized);
}

TEST_F(WindowGeometryManagerTest, geometry)
{
    StartupParameters params;
    params.windowGeometry = kSpecificGeometry;

    givenStateWithParams(kNormalState, params);
    ASSERT_EQ(resultState().geometry, kSpecificGeometry);
    ASSERT_FALSE(resultState().isFullscreen);
    ASSERT_FALSE(resultState().isMaximized);

    givenStateWithParams(kFullscreenState, params);
    ASSERT_EQ(resultState().geometry, kSpecificGeometry);

    // By now, only the normalized window geometry is updated.
    ASSERT_TRUE(resultState().isFullscreen || resultState().isMaximized);
}

TEST_F(WindowGeometryManagerTest, geometryWithNoFullscreen)
{
    StartupParameters params;
    params.noFullScreen = true;
    params.windowGeometry = kSpecificGeometry;

    givenStateWithParams(kNormalState, params);
    ASSERT_EQ(resultState().geometry, kSpecificGeometry);
    ASSERT_FALSE(resultState().isFullscreen);
    ASSERT_FALSE(resultState().isMaximized);

    givenStateWithParams(kFullscreenState, params);
    ASSERT_EQ(resultState().geometry, kSpecificGeometry);
    ASSERT_FALSE(resultState().isFullscreen);
    ASSERT_FALSE(resultState().isMaximized);
}

TEST_F(WindowGeometryManagerTest, screen)
{
    {
        StartupParameters params;
        params.screen = 1;

        givenStateWithParams(kNormalState, params);
        ASSERT_TRUE(kSecondScreen.contains(resultState().geometry));
        ASSERT_TRUE(resultState().isFullscreen || resultState().isMaximized);

        givenStateWithParams(kFullscreenState, params);
        ASSERT_TRUE(kSecondScreen.contains(resultState().geometry));
        ASSERT_TRUE(resultState().isFullscreen || resultState().isMaximized);
    }

    {
        StartupParameters params;
        params.screen = 1;
        params.noFullScreen = true;

        givenStateWithParams(kNormalState, params);
        ASSERT_TRUE(kSecondScreen.contains(resultState().geometry));
        ASSERT_FALSE(resultState().isFullscreen);
        ASSERT_FALSE(resultState().isMaximized);

        givenStateWithParams(kFullscreenState, params);
        ASSERT_TRUE(kSecondScreen.contains(resultState().geometry));
        ASSERT_FALSE(resultState().isFullscreen);
        ASSERT_FALSE(resultState().isMaximized);
    }

    {
        StartupParameters params;
        params.screen = 1;
        params.windowGeometry = kSpecificGeometry;

        givenStateWithParams(kNormalState, params);
        ASSERT_TRUE(kSecondScreen.contains(resultState().geometry));
        ASSERT_FALSE(resultState().isFullscreen);
        ASSERT_FALSE(resultState().isMaximized);

        givenStateWithParams(kFullscreenState, params);
        ASSERT_TRUE(kSecondScreen.contains(resultState().geometry));
        ASSERT_FALSE(resultState().isFullscreen);
        ASSERT_FALSE(resultState().isMaximized);
    }
}

} // namespace test
} // namespace nx::vms::client::desktop
