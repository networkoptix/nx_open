// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/client/core/ptz/remote_ptz_controller.h>
#include <nx/vms/common/resource/camera_resource_stub.h>

namespace nx::vms::client::core::ptz::test {

TEST(RemotePtzController, supportedTypesFollowCameraCapabilities)
{
    using namespace common::ptz;
    const CameraResourceStubPtr camera(new CameraResourceStub());
    const RemotePtzController controller(camera);

    for (const Types types:
        {Types{}, Types(Type::operational), Types(Type::configurational), Types(Type::any)})
    {
        for (const auto type: kAllTypes)
        {
            camera->setPtzCapabilities(types.testFlag(type)
                    ? Ptz::Capability::continuousPanTiltZoom
                    : Ptz::Capability::none,
                type);
        }
        EXPECT_EQ(controller.getSupportedPtzTypes(), types);
    }

    camera->setPtzCapabilities(
        Ptz::Capability::virtual_ | Ptz::Capability::continuousPanTiltZoom, Type::operational);
    camera->setPtzCapabilities(Ptz::Capability::none, Type::configurational);
    EXPECT_EQ(controller.getSupportedPtzTypes(), Types{});
}

} // namespace nx::vms::client::core::ptz::test
