#pragma once

#include <gtest/gtest.h>
#include <nx/mediaserver/resource/camera.h>
#include <nx/utils/std/cpp14.h>
#include <providers/live_stream_provider.h>

namespace nx {
namespace mediaserver {
namespace resource {
namespace test {

class LiveStreamProviderMock: public QnLiveStreamProvider
{
    using base_type = QnLiveStreamProvider;
public:
    LiveStreamProviderMock(const QnResourcePtr& resource) : base_type(resource) {}

    virtual void pleaseReopenStream() override {}
    virtual bool isCameraControlDisabled() const override { return false;  }
    virtual void updateSoftwareMotion() override {}
};

} // namespace test
} // namespace resource
} // namespace mediaserver
} // namespace nx
