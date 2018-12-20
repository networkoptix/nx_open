#pragma once

#include <gtest/gtest.h>
#include <nx/vms/server/resource/camera.h>
#include <nx/utils/std/cpp14.h>
#include <providers/live_stream_provider.h>

namespace nx {
namespace vms::server {
namespace resource {
namespace test {

class LiveStreamProviderMock: public QnLiveStreamProvider
{
    using base_type = QnLiveStreamProvider;
public:
    LiveStreamProviderMock(const CameraPtr& camera) : base_type(camera) {}

    virtual void pleaseReopenStream() override {}
    virtual bool isCameraControlDisabled() const override { return false;  }
    virtual void updateSoftwareMotion() override {}
};

} // namespace test
} // namespace resource
} // namespace vms::server
} // namespace nx
