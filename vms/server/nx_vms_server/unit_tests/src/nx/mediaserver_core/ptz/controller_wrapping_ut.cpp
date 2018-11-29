#include <gtest/gtest.h>

#include <memory>

#include <common/common_module.h>
#include <common/static_common_module.h>

#include <utils/math/space_mapper.h>

#include <nx/mediaserver/camera_mock.h>
#include <nx/vms/server/ptz/server_ptz_helpers.h>
#include <nx/vms/server/ptz/server_ptz_controller_pool.h>

#include <core/ptz/ptz_mapper.h>
#include <nx/core/ptz/test_support/test_ptz_controller.h>
#include <nx/core/ptz/space_mapper.h>
#include <nx/core/access/access_types.h>

namespace core_ptz = nx::core::ptz;
namespace mediaserver_ptz = nx::vms::server::ptz;

using namespace nx::vms::server::resource;

namespace nx {
namespace mediaserver_core{
namespace ptz {

class ControllerWrappingTest: public ::testing::Test
{

public:
    virtual void SetUp() override
    {
        m_staticCommonModule = std::make_unique<QnStaticCommonModule>();
        m_serverModule = std::make_unique<QnMediaServerModule>();

        m_pool = new vms::server::ptz::ServerPtzControllerPool(m_serverModule->commonModule());
        m_camera.reset(new test::CameraMock(m_serverModule.get()));
        m_camera->setCommonModule(m_serverModule->commonModule());
        m_camera->initInternal();
    }

public:
    void hasControllerWithCapabilities(Ptz::Capabilities capabilities)
    {
        auto controller = new core_ptz::test_support::TestPtzController(m_camera);
        m_controller.reset(controller);
        controller->setCapabilities(capabilities);
        m_camera->setPtzController(controller);
    }

    void wrapItUsingTheFollowingParameters(
        const mediaserver_ptz::ControllerWrappingParameters& parameters)
    {
        m_controller = mediaserver_ptz::wrapController(m_controller, parameters);
    }

    void makeSureItHasTheFollowingCapabilities(Ptz::Capabilities capabilities) const
    {
        ASSERT_EQ(
            mediaserver_ptz::capabilities(m_controller) & capabilities,
            capabilities);
    }

    void makeSureItHasNoneOfCapabilities(Ptz::Capabilities capabilities) const
    {
        ASSERT_EQ(
            mediaserver_ptz::capabilities(m_controller) & capabilities,
            Ptz::NoPtzCapabilities);
    }

    static QnPtzMapperPtr makeMapper()
    {
        QnSpaceMapperPtr<core_ptz::Vector> inputMapper(
            new core_ptz::SpaceMapper(
                QnSpaceMapperPtr<qreal>(new QnIdentitySpaceMapper<qreal>()),
                QnSpaceMapperPtr<qreal>(new QnIdentitySpaceMapper<qreal>()),
                QnSpaceMapperPtr<qreal>(new QnIdentitySpaceMapper<qreal>()),
                QnSpaceMapperPtr<qreal>(new QnIdentitySpaceMapper<qreal>())));

        QnSpaceMapperPtr<core_ptz::Vector> outputMapper(
            new core_ptz::SpaceMapper(
                QnSpaceMapperPtr<qreal>(new QnIdentitySpaceMapper<qreal>()),
                QnSpaceMapperPtr<qreal>(new QnIdentitySpaceMapper<qreal>()),
                QnSpaceMapperPtr<qreal>(new QnIdentitySpaceMapper<qreal>()),
                QnSpaceMapperPtr<qreal>(new QnIdentitySpaceMapper<qreal>())));

        return QnPtzMapperPtr(new QnPtzMapper(inputMapper, outputMapper));
    }

    mediaserver_ptz::ControllerWrappingParameters makeBaseParameters() const
    {
        mediaserver_ptz::ControllerWrappingParameters parameters;
        parameters.ptzPool = m_pool;

        return parameters;
    }

    void launch(
        const mediaserver_ptz::ControllerWrappingParameters& parameters,
        Ptz::Capabilities initialCapabilities,
        Ptz::Capabilities capabilitiesThatMustBePresent,
        Ptz::Capabilities capabilitiesThatMustBeAbsent = Ptz::NoPtzCapabilities)
    {
        hasControllerWithCapabilities(initialCapabilities);
        wrapItUsingTheFollowingParameters(parameters);
        makeSureItHasTheFollowingCapabilities(capabilitiesThatMustBePresent);
        makeSureItHasNoneOfCapabilities(capabilitiesThatMustBeAbsent);
    }

private:
    std::unique_ptr<QnStaticCommonModule> m_staticCommonModule;
    std::unique_ptr<QnMediaServerModule> m_serverModule;
    QnPtzControllerPool* m_pool;
    QnSharedResourcePointer<test::CameraMock> m_camera;
    QnPtzControllerPtr m_controller;
};

TEST_F(ControllerWrappingTest, wrapToMappedController)
{
    auto parameters = makeBaseParameters();
    parameters.absoluteMoveMapper = makeMapper();

    launch(
        parameters,
        Ptz::AbsolutePtrzCapabilities | Ptz::DevicePositioningPtzCapability,
        Ptz::LogicalPositioningPtzCapability);

    // No mapper.
    launch(
        makeBaseParameters(),
        Ptz::AbsolutePtrzCapabilities | Ptz::DevicePositioningPtzCapability,
        Ptz::NoPtzCapabilities,
        Ptz::LogicalPositioningPtzCapability);
}

TEST_F(ControllerWrappingTest, wrapToViewportController)
{
    auto parameters = makeBaseParameters();
    launch(
        parameters,
        Ptz::AbsolutePtzCapabilities | Ptz::LogicalPositioningPtzCapability,
        Ptz::ViewportPtzCapability);

    parameters.absoluteMoveMapper = makeMapper();
    launch(
        parameters,
        Ptz::AbsolutePtzCapabilities | Ptz::DevicePositioningPtzCapability,
        Ptz::ViewportPtzCapability);
}

TEST_F(ControllerWrappingTest, wrapToPresetController)
{
    launch(
        makeBaseParameters(),
        Ptz::AbsolutePtrzCapabilities | Ptz::DevicePositioningPtzCapability,
        Ptz::PresetsPtzCapability);

    launch(
        makeBaseParameters(),
        Ptz::AbsolutePtrzCapabilities | Ptz::LogicalPositioningPtzCapability,
        Ptz::PresetsPtzCapability);

    // No positioning capability.
    launch(
        makeBaseParameters(),
        Ptz::AbsolutePtrzCapabilities,
        Ptz::NoPtzCapabilities,
        Ptz::PresetsPtzCapability);

    // No absolute capabilities.
    launch(
        makeBaseParameters(),
        Ptz::ContinuousPtrzCapabilities
            | Ptz::LogicalPositioningPtzCapability
            | Ptz::DevicePositioningPtzCapability,
        Ptz::NoPtzCapabilities,
        Ptz::PresetsPtzCapability);
}

TEST_F(ControllerWrappingTest, wrapToTourController)
{
    launch(
        makeBaseParameters(),
        Ptz::PresetsPtzCapability,
        Ptz::ToursPtzCapability);
}

TEST_F(ControllerWrappingTest, wrapToActivityController)
{
    launch(
        makeBaseParameters(),
        Ptz::PresetsPtzCapability,
        Ptz::ActivityPtzCapability);

    launch(
        makeBaseParameters(),
        Ptz::ToursPtzCapability,
        Ptz::ActivityPtzCapability);
}

TEST_F(ControllerWrappingTest, wrapToHomeController)
{
    launch(
        makeBaseParameters(),
        Ptz::PresetsPtzCapability,
        Ptz::ActivityPtzCapability);

    launch(
        makeBaseParameters(),
        Ptz::ToursPtzCapability,
        Ptz::ActivityPtzCapability);
}


TEST_F(ControllerWrappingTest, wrapToRelativeMoveController)
{
    launch(
        makeBaseParameters(),
        Ptz::AbsolutePtzCapabilities,
        Ptz::RelativePtzCapabilities);

    launch(
        makeBaseParameters(),
        Ptz::ContinuousPtzCapabilities,
        Ptz::RelativePtzCapabilities);
}

} // namespace ptz
} // namespace vms::server
} // namespace nx
