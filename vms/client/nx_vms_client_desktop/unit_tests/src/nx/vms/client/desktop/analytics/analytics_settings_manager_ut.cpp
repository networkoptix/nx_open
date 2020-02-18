#include <gtest/gtest.h>

#include <common/static_common_module.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <nx/core/access/access_types.h>

#include <nx/vms/client/desktop/analytics/analytics_settings_manager.h>

namespace nx::vms::client::desktop {
namespace test {

class AnalyticsSettingsMockApiInterface: public AnalyticsSettingsServerInterface
{
public:
    AnalyticsSettingsMockApiInterface(){}

    virtual rest::Handle getSettings(
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        AnalyticsSettingsCallback callback) override
    {
        return {};
    }

    virtual rest::Handle applySettings(
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        const QJsonObject& settings,
        AnalyticsSettingsCallback callback) override
    {
        return {};
    }
};
using AnalyticsSettingsMockApiInterfacePtr = std::shared_ptr<AnalyticsSettingsMockApiInterface>;

class AnalyticsSettingsManagerTest: public ::testing::Test
{
protected:
    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp()
    {
        m_environment.reset(new Environment());
        m_serverInterfaceMock = std::make_shared<AnalyticsSettingsMockApiInterface>();
        m_manager.reset(new AnalyticsSettingsManager());
        m_manager->setResourcePool(resourcePool());
        m_manager->setServerInterface(m_serverInterfaceMock);
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown()
    {
        m_manager.reset();
        m_serverInterfaceMock.reset();
        m_environment.reset();
    }

    QnResourcePool* resourcePool() const { return m_environment->commonModule.resourcePool(); }

    struct Environment
    {
        Environment():
            staticCommonModule(),
            commonModule(
                /*clientMode*/ false,
                /*resourceAccessMode*/ nx::core::access::Mode::direct)
        {
        }

        QnStaticCommonModule staticCommonModule;
        QnCommonModule commonModule;
    };

    // Declares the variables your tests want to use.

    QScopedPointer<Environment> m_environment;
    QScopedPointer<AnalyticsSettingsManager> m_manager;
    AnalyticsSettingsMockApiInterfacePtr m_serverInterfaceMock;
};

static const QString kEmptyReply = R"json(
    {
        "analyzedStreamIndex": "primary",
        "settingsModel": {},
        "settingsValues": {}
    }
)json";

TEST_F(AnalyticsSettingsManagerTest, init)
{
}

} // namespace test
} // namespace nx::vms::client::desktop
