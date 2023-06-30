// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/client/core/analytics/analytics_settings_multi_listener.h>

#include "analytics_settings_test_fixture.h"

namespace nx::vms::client::desktop {
namespace test {

using namespace nx::vms::common;

class AnalyticsSettingsMultiListenerTest: public AnalyticsSettingsTestFixture
{
    using base_type = AnalyticsSettingsTestFixture;

protected:
    using ListenPolicy = core::AnalyticsSettingsMultiListener::ListenPolicy;

    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp() override
    {
        base_type::SetUp();
        m_camera = addCamera();
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown() override
    {
        m_listener.reset();
    }

    core::AnalyticsSettingsMultiListener* listener() const
    {
        return m_listener.get();
    }

    nx::CameraResourceStubPtr camera() const
    {
        return m_camera;
    }

    void whenDeviceAgentEnabled(const AnalyticsEngineResourcePtr& engine, bool value = true)
    {
        QnUuidSet enabledEngines = m_camera->enabledAnalyticsEngines();
        if (value)
            enabledEngines.insert(engine->getId());
        else
            enabledEngines.remove(engine->getId());
        m_camera->setUserEnabledAnalyticsEngines(enabledEngines);
    }

    AnalyticsEngineResourcePtr givenCompatibleEngine()
    {
        auto engine = addEngine();
        QnUuidSet compatibleEngines = m_camera->compatibleAnalyticsEngines();
        compatibleEngines.insert(engine->getId());
        m_camera->setCompatibleAnalyticsEngines(compatibleEngines);
        return engine;
    }

    void listenTo(ListenPolicy policy)
    {
        m_listener.reset(new core::AnalyticsSettingsMultiListener(manager(), m_camera, policy));
    }

private:
    nx::CameraResourceStubPtr m_camera;
    QScopedPointer<core::AnalyticsSettingsMultiListener> m_listener;
};

/** Make sure listener correctly stops listening engine when device agent is switched off. */
TEST_F(AnalyticsSettingsMultiListenerTest, agentWasSwitchedOff)
{
    auto engine = givenCompatibleEngine();
    whenDeviceAgentEnabled(engine);
    listenTo(ListenPolicy::enabledEngines);
    ASSERT_TRUE(listener()->engineIds().contains(engine->getId()));
    whenDeviceAgentEnabled(engine, false);
    ASSERT_FALSE(listener()->engineIds().contains(engine->getId()));
}

} // namespace test
} // namespace nx::vms::client::desktop
