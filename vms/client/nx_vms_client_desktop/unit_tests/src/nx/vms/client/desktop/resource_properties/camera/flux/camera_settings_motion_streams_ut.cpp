// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>

#include <gtest/gtest.h>

#include <core/resource/resource_property_key.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/resource_properties/camera/flux/camera_settings_dialog_state.h>
#include <nx/vms/client/desktop/resource_properties/camera/flux/camera_settings_dialog_state_reducer.h>
#include <nx/vms/client/desktop/resource_properties/camera/flux/camera_settings_dialog_store.h>
#include <nx/vms/client/desktop/resource_properties/camera/utils/camera_settings_dialog_state_conversion_functions.h>
#include <nx/vms/client/desktop/resource_properties/camera/watchers/camera_settings_remote_changes_watcher.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/test_support/test_context.h>
#include <nx/vms/common/resource/camera_resource_stub.h>

namespace nx::vms::client::desktop {

std::ostream& operator<<(std::ostream& os, CameraSettingsDialogState::MotionStreamAlert value)
{
    switch (value)
    {
        case CameraSettingsDialogState::MotionStreamAlert::resolutionTooHigh:
            os << "resolutionTooHigh";
            break;
        case CameraSettingsDialogState::MotionStreamAlert::implicitlyDisabled:
            os << "implicitlyDisabled";
            break;
        case CameraSettingsDialogState::MotionStreamAlert::willBeImplicitlyDisabled:
            os << "willBeImplicitlyDisabled";
            break;
    }
    return os;
}

std::ostream& operator<<(
    std::ostream& os,
    std::optional<CameraSettingsDialogState::MotionStreamAlert> value)
{
    if (value)
        os << *value;
    else
        os << "std::nullopt";
    return os;
}

namespace test {

namespace {

using MotionStreamIndex = QnVirtualCameraResource::MotionStreamIndex;
using StreamIndex = nx::vms::api::StreamIndex;

MotionStreamIndex makeStreamIndex(StreamIndex index, bool isForced)
{
    return MotionStreamIndex{index, isForced};
}

} // namespace

using MotionType = nx::vms::api::MotionType;

class CameraSettingsMotionStreamsTest: public ContextBasedTest
{
public:
    using State = CameraSettingsDialogState;
    using Store = CameraSettingsDialogStore;
    using Reducer = CameraSettingsDialogStateReducer;
    using Applier = CameraSettingsDialogStateConversionFunctions;
    using StreamAlert = State::MotionStreamAlert;

    CameraSettingsMotionStreamsTest():
        m_store(new Store()),
        m_changesWatcher(new CameraSettingsRemoteChangesWatcher(m_store.get()))
    {
    }

    Store& store() { return *m_store; }
    const State& state() const { return m_store->state(); }

    void setCameras(const QnVirtualCameraResourceList& cameras)
    {
        m_store->loadCameras(cameras, nullptr, nullptr);
        m_changesWatcher->setCameras(cameras);
    }

    static constexpr QSize highResolution1{1920, 1080};
    static constexpr QSize highResolution2{1680, 1050};
    static constexpr QSize lowResolution1{640, 480};
    static constexpr QSize lowResolution2{320, 240};

    static_assert(highResolution1.width() * highResolution1.height()
        > QnVirtualCameraResource::kMaximumMotionDetectionPixels);
    static_assert(highResolution2.width() * highResolution2.height()
        > QnVirtualCameraResource::kMaximumMotionDetectionPixels);
    static_assert(lowResolution1.width() * lowResolution1.height()
        <= QnVirtualCameraResource::kMaximumMotionDetectionPixels);
    static_assert(lowResolution2.width() * lowResolution2.height()
        <= QnVirtualCameraResource::kMaximumMotionDetectionPixels);

protected:
    CameraResourceStubPtr makeCamera(
        const QSize& primaryResolution = highResolution1,
        const QSize& secondaryResolution = lowResolution1)
    {
        CameraResourceStubPtr camera(
            new CameraResourceStub(primaryResolution, secondaryResolution));
        systemContext()->resourcePool()->addResource(camera);
        return camera;
    }

private:
    nx::utils::ImplPtr<Store> m_store;
    nx::utils::ImplPtr<CameraSettingsRemoteChangesWatcher> m_changesWatcher;
};

TEST_F(CameraSettingsMotionStreamsTest, selectHighResolutionStream)
{
    auto camera = makeCamera();

    NX_ASSERT(camera->motionStreamIndex() == makeStreamIndex(StreamIndex::secondary, false));
    NX_ASSERT(camera->getMotionType() == MotionType::software);
    NX_ASSERT(camera->hasDualStreaming());

    // Initially secondary stream is selected, motion detection is enabled but not forced.
    setCameras({camera});
    ASSERT_EQ(state().motion.stream(), StreamIndex::secondary);
    ASSERT_EQ(state().singleCameraProperties.primaryStreamResolution, highResolution1);
    ASSERT_EQ(state().singleCameraProperties.secondaryStreamResolution, lowResolution1);
    ASSERT_FALSE(state().motion.forced());
    ASSERT_TRUE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, std::nullopt);

    // Selecting primary stream with high resolution, too high resolution flag is getting set.
    store().setMotionStream(StreamIndex::primary);
    ASSERT_EQ(state().motion.stream(), StreamIndex::primary);
    ASSERT_FALSE(state().motion.forced());
    ASSERT_TRUE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, StreamAlert::resolutionTooHigh);

    // Applying changes to camera results in motion detection being forced.
    Applier::applyStateToCameras(state(), {camera});
    ASSERT_TRUE(camera->motionStreamIndex() == makeStreamIndex(StreamIndex::primary, true));
    ASSERT_EQ(camera->getMotionType(), MotionType::software);

    setCameras({camera});
    ASSERT_EQ(state().motion.stream(), StreamIndex::primary);
    ASSERT_TRUE(state().motion.forced());
    ASSERT_TRUE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, StreamAlert::resolutionTooHigh);
}

TEST_F(CameraSettingsMotionStreamsTest, selectLowResolutionStream)
{
    auto camera = makeCamera(lowResolution1, lowResolution2);
    NX_ASSERT(camera->motionStreamIndex() == makeStreamIndex(StreamIndex::secondary, false));
    NX_ASSERT(camera->getMotionType() == MotionType::software);
    NX_ASSERT(camera->hasDualStreaming());

    // Initially secondary stream is selected, motion detection is enabled but not forced.
    setCameras({camera});
    ASSERT_EQ(state().motion.stream(), StreamIndex::secondary);
    ASSERT_EQ(state().singleCameraProperties.primaryStreamResolution, lowResolution1);
    ASSERT_EQ(state().singleCameraProperties.secondaryStreamResolution, lowResolution2);
    ASSERT_FALSE(state().motion.forced());
    ASSERT_TRUE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, std::nullopt);

    // Selecting primary stream with low resolution, too high resolution flag is not getting set.
    store().setMotionStream(StreamIndex::primary);
    ASSERT_EQ(state().motion.stream(), StreamIndex::primary);
    ASSERT_FALSE(state().motion.forced());
    ASSERT_TRUE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, std::nullopt);

    // Applying changes to camera results in motion detection not being forced.
    Applier::applyStateToCameras(state(), {camera});
    ASSERT_TRUE(camera->motionStreamIndex() == makeStreamIndex(StreamIndex::primary, false));
    ASSERT_EQ(camera->getMotionType(), MotionType::software);

    setCameras({camera});
    ASSERT_EQ(state().motion.stream(), StreamIndex::primary);
    ASSERT_FALSE(state().motion.forced());
    ASSERT_TRUE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, std::nullopt);
}

TEST_F(CameraSettingsMotionStreamsTest, enableMotionWithHighResolutionStream)
{
    auto camera = makeCamera();
    camera->setMotionType(MotionType::none);
    NX_ASSERT(camera->getMotionType() == MotionType::none);
    NX_ASSERT(camera->motionStreamIndex() == makeStreamIndex(StreamIndex::secondary, false));
    NX_ASSERT(camera->hasDualStreaming());

    // Initially secondary stream is selected, motion detection is disabled.
    setCameras({camera});
    ASSERT_EQ(state().motion.stream(), StreamIndex::secondary);
    ASSERT_EQ(state().singleCameraProperties.primaryStreamResolution, highResolution1);
    ASSERT_EQ(state().singleCameraProperties.secondaryStreamResolution, lowResolution1);
    ASSERT_FALSE(state().motion.forced());
    ASSERT_FALSE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, std::nullopt);

    // Selecting primary stream with high resolution, too high resolution flag is getting set.
    store().setMotionStream(StreamIndex::primary);
    ASSERT_EQ(state().motion.stream(), StreamIndex::primary);
    ASSERT_FALSE(state().motion.forced());
    ASSERT_FALSE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, std::nullopt);

    // Applying changes to camera doesn't result in motion detection being forced.
    Applier::applyStateToCameras(state(), {camera});
    ASSERT_TRUE(camera->motionStreamIndex() == makeStreamIndex(StreamIndex::primary, false));
    ASSERT_EQ(camera->getMotionType(), MotionType::none);

    setCameras({camera});
    ASSERT_EQ(state().motion.stream(), StreamIndex::primary);
    ASSERT_FALSE(state().motion.forced());
    ASSERT_FALSE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, std::nullopt);

    // Turning motion detection on.
    store().setMotionDetectionEnabled(true);
    ASSERT_EQ(state().motion.stream(), StreamIndex::primary);
    ASSERT_FALSE(state().motion.forced());
    ASSERT_TRUE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, StreamAlert::resolutionTooHigh);

    // Applying changes to camera results in motion detection being forced.
    Applier::applyStateToCameras(state(), {camera});
    ASSERT_TRUE(camera->motionStreamIndex() == makeStreamIndex(StreamIndex::primary, true));
    ASSERT_EQ(camera->getMotionType(), MotionType::software);

    setCameras({camera});
    ASSERT_EQ(state().motion.stream(), StreamIndex::primary);
    ASSERT_TRUE(state().motion.forced());
    ASSERT_TRUE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, StreamAlert::resolutionTooHigh);
}

TEST_F(CameraSettingsMotionStreamsTest, enableMotionWithLowResolutionStream)
{
    auto camera = makeCamera(lowResolution1, lowResolution2);
    camera->setMotionType(MotionType::none);
    NX_ASSERT(camera->getMotionType() == MotionType::none);
    NX_ASSERT(camera->motionStreamIndex() == makeStreamIndex(StreamIndex::secondary, false));
    NX_ASSERT(camera->hasDualStreaming());

    // Initially secondary stream is selected, motion detection is disabled.
    setCameras({camera});
    ASSERT_EQ(state().motion.stream(), StreamIndex::secondary);
    ASSERT_EQ(state().singleCameraProperties.primaryStreamResolution, lowResolution1);
    ASSERT_EQ(state().singleCameraProperties.secondaryStreamResolution, lowResolution2);
    ASSERT_FALSE(state().motion.forced());
    ASSERT_FALSE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, std::nullopt);

    // Selecting primary stream with high resolution, too high resolution flag is not getting set.
    store().setMotionStream(StreamIndex::primary);
    ASSERT_EQ(state().motion.stream(), StreamIndex::primary);
    ASSERT_FALSE(state().motion.forced());
    ASSERT_FALSE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, std::nullopt);

    // Applying changes to camera doesn't result in motion detection being forced.
    Applier::applyStateToCameras(state(), {camera});
    ASSERT_TRUE(camera->motionStreamIndex() == makeStreamIndex(StreamIndex::primary, false));
    ASSERT_EQ(camera->getMotionType(), MotionType::none);

    setCameras({camera});
    ASSERT_EQ(state().motion.stream(), StreamIndex::primary);
    ASSERT_FALSE(state().motion.forced());
    ASSERT_FALSE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, std::nullopt);

    // Turning motion detection on.
    store().setMotionDetectionEnabled(true);
    ASSERT_EQ(state().motion.stream(), StreamIndex::primary);
    ASSERT_FALSE(state().motion.forced());
    ASSERT_TRUE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, std::nullopt);

    // Applying changes to camera still doesn't result in motion detection being forced.
    Applier::applyStateToCameras(state(), {camera});
    ASSERT_TRUE(camera->motionStreamIndex() == makeStreamIndex(StreamIndex::primary, false));
    ASSERT_EQ(camera->getMotionType(), MotionType::software);

    setCameras({camera});
    ASSERT_EQ(state().motion.stream(), StreamIndex::primary);
    ASSERT_FALSE(state().motion.forced());
    ASSERT_TRUE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, std::nullopt);
}

TEST_F(CameraSettingsMotionStreamsTest, disableMotionWithForcedHighResolution)
{
    auto camera = makeCamera();
    camera->setMotionStreamIndex({StreamIndex::primary, true});
    NX_ASSERT(camera->getMotionType() == MotionType::software);
    NX_ASSERT(camera->motionStreamIndex() == makeStreamIndex(StreamIndex::primary, true));
    NX_ASSERT(camera->hasDualStreaming());

    // Initially primary stream is selected, motion detection is enabled and forced.
    setCameras({camera});
    ASSERT_EQ(state().motion.stream(), StreamIndex::primary);
    ASSERT_EQ(state().singleCameraProperties.primaryStreamResolution, highResolution1);
    ASSERT_EQ(state().singleCameraProperties.secondaryStreamResolution, lowResolution1);
    ASSERT_TRUE(state().motion.forced());
    ASSERT_TRUE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, StreamAlert::resolutionTooHigh);

    // Disabling motion detection.
    store().setMotionDetectionEnabled(false);
    ASSERT_EQ(state().motion.stream(), StreamIndex::primary);
    ASSERT_TRUE(state().motion.forced());
    ASSERT_FALSE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, std::nullopt);

    // Applying changes to camera results in motion detection stopping being forced.
    Applier::applyStateToCameras(state(), {camera});
    ASSERT_TRUE(camera->motionStreamIndex() == makeStreamIndex(StreamIndex::primary, false));
    ASSERT_EQ(camera->getMotionType(), MotionType::none);

    setCameras({camera});
    ASSERT_EQ(state().motion.stream(), StreamIndex::primary);
    ASSERT_FALSE(state().motion.forced());
    ASSERT_FALSE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, std::nullopt);
}

TEST_F(CameraSettingsMotionStreamsTest, implicitlyDisabledMotionDetection)
{
    auto camera = makeCamera();
    camera->setMotionStreamIndex({StreamIndex::primary, false});
    NX_ASSERT(camera->getMotionType() == MotionType::software);
    NX_ASSERT(camera->motionStreamIndex() == makeStreamIndex(StreamIndex::primary, false));
    NX_ASSERT(camera->hasDualStreaming());

    // Initially primary stream is selected, motion detection is not forced and implicitly disabled.
    setCameras({camera});
    ASSERT_EQ(state().motion.stream(), StreamIndex::primary);
    ASSERT_EQ(state().singleCameraProperties.primaryStreamResolution, highResolution1);
    ASSERT_EQ(state().singleCameraProperties.secondaryStreamResolution, lowResolution1);
    ASSERT_FALSE(state().motion.forced());
    ASSERT_TRUE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, StreamAlert::implicitlyDisabled);
}

TEST_F(CameraSettingsMotionStreamsTest, forceMotionDetectionWhenImplicitlyDisabled)
{
    auto camera = makeCamera();
    camera->setMotionStreamIndex({StreamIndex::primary, false});
    NX_ASSERT(camera->getMotionType() == MotionType::software);
    NX_ASSERT(camera->motionStreamIndex() == makeStreamIndex(StreamIndex::primary, false));
    NX_ASSERT(camera->hasDualStreaming());

    setCameras({camera});

    // Force motion detection.
    store().forceMotionDetection();
    ASSERT_EQ(state().motion.stream(), StreamIndex::primary);
    ASSERT_TRUE(state().motion.forced());
    ASSERT_TRUE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, StreamAlert::resolutionTooHigh);

    // Applying changes to camera results in motion detection starting to work.
    Applier::applyStateToCameras(state(), {camera});
    ASSERT_TRUE(camera->motionStreamIndex() == makeStreamIndex(StreamIndex::primary, true));
    ASSERT_EQ(camera->getMotionType(), MotionType::software);

    setCameras({camera});
    ASSERT_EQ(state().motion.stream(), StreamIndex::primary);
    ASSERT_TRUE(state().motion.forced());
    ASSERT_TRUE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, StreamAlert::resolutionTooHigh);
}

TEST_F(CameraSettingsMotionStreamsTest, selectLowStreamWhenImplicitlyDisabled)
{
    auto camera = makeCamera();
    camera->setMotionStreamIndex({StreamIndex::primary, false});
    NX_ASSERT(camera->getMotionType() == MotionType::software);
    NX_ASSERT(camera->motionStreamIndex() == makeStreamIndex(StreamIndex::primary, false));
    NX_ASSERT(camera->hasDualStreaming());

    setCameras({camera});

    // Selecting secondary stream with resolution below threshold.
    store().setMotionStream(StreamIndex::secondary);
    ASSERT_EQ(state().motion.stream(), StreamIndex::secondary);
    ASSERT_FALSE(state().motion.forced());
    ASSERT_TRUE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, std::nullopt);

    // Applying changes to camera results in motion detection starting to work.
    Applier::applyStateToCameras(state(), {camera});
    ASSERT_TRUE(camera->motionStreamIndex() == makeStreamIndex(StreamIndex::secondary, false));
    ASSERT_EQ(camera->getMotionType(), MotionType::software);

    setCameras({camera});
    ASSERT_EQ(state().motion.stream(), StreamIndex::secondary);
    ASSERT_FALSE(state().motion.forced());
    ASSERT_TRUE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, std::nullopt);
}

TEST_F(CameraSettingsMotionStreamsTest, disableMotionWhenImplicitlyDisabled)
{
    auto camera = makeCamera();
    camera->setMotionStreamIndex({StreamIndex::primary, false});
    NX_ASSERT(camera->getMotionType() == MotionType::software);
    NX_ASSERT(camera->motionStreamIndex() == makeStreamIndex(StreamIndex::primary, false));
    NX_ASSERT(camera->hasDualStreaming());

    setCameras({camera});

    // Selecting secondary stream with resolution below threshold.
    store().setMotionDetectionEnabled(false);
    ASSERT_EQ(state().motion.stream(), StreamIndex::primary);
    ASSERT_FALSE(state().motion.forced());
    ASSERT_FALSE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, std::nullopt);

    // Applying changes to camera.
    Applier::applyStateToCameras(state(), {camera});
    ASSERT_TRUE(camera->motionStreamIndex() == makeStreamIndex(StreamIndex::primary, false));
    ASSERT_EQ(camera->getMotionType(), MotionType::none);

    setCameras({camera});
    ASSERT_EQ(state().motion.stream(), StreamIndex::primary);
    ASSERT_FALSE(state().motion.forced());
    ASSERT_FALSE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, std::nullopt);
}

TEST_F(CameraSettingsMotionStreamsTest, disableDualStreaming)
{
    auto camera = makeCamera();
    NX_ASSERT(camera->getMotionType() == MotionType::software);
    NX_ASSERT(camera->motionStreamIndex() == makeStreamIndex(StreamIndex::secondary, false));
    NX_ASSERT(camera->hasDualStreaming());

    setCameras({camera});

    // Disabling dual-streaming causes motion stream to implicitly change.
    store().setDualStreamingDisabled(true);
    ASSERT_EQ(state().motion.stream(), StreamIndex::secondary);
    ASSERT_EQ(state().effectiveMotionStream(), StreamIndex::primary);
    ASSERT_TRUE(state().motion.enabled.equals(true));
    ASSERT_FALSE(state().motion.forced());
    ASSERT_EQ(state().motion.streamAlert, StreamAlert::willBeImplicitlyDisabled);
    ASSERT_EQ(state().devicesDescription.hasMotion, CombinedValue::All);

    // Applying changes to camera saves implicitly selected stream.
    Applier::applyStateToCameras(state(), {camera});
    ASSERT_TRUE(camera->motionStreamIndex() == makeStreamIndex(StreamIndex::primary, false));
    ASSERT_EQ(camera->getMotionType(), MotionType::software);

    setCameras({camera});
    ASSERT_EQ(state().motion.stream(), StreamIndex::primary);
    ASSERT_EQ(state().effectiveMotionStream(), StreamIndex::primary);
    ASSERT_TRUE(state().motion.enabled.equals(true));
    ASSERT_FALSE(state().motion.forced());
    ASSERT_EQ(state().motion.streamAlert, StreamAlert::implicitlyDisabled);

    // Check that we can force motion detection when dual streaming is disabled.
    store().forceMotionDetection();
    Applier::applyStateToCameras(state(), {camera});

    setCameras({camera});
    ASSERT_TRUE(state().motion.forced());
    ASSERT_EQ(camera->motionStreamIndex(), makeStreamIndex(StreamIndex::primary, true));
}

TEST_F(CameraSettingsMotionStreamsTest, disableDualStreamingMultiple)
{
    auto camera1 = makeCamera();
    auto camera2 = makeCamera();
    camera1->setMotionType(MotionType::none);
    camera2->setMotionType(MotionType::none);
    NX_ASSERT(camera1->getMotionType() == MotionType::none);
    NX_ASSERT(camera1->motionStreamIndex() == makeStreamIndex(StreamIndex::secondary, false));
    NX_ASSERT(camera1->hasDualStreaming());
    NX_ASSERT(camera2->getMotionType() == MotionType::none);
    NX_ASSERT(camera2->motionStreamIndex() == makeStreamIndex(StreamIndex::secondary, false));
    NX_ASSERT(camera2->hasDualStreaming());

    QnVirtualCameraResourceList cameras{camera1, camera2};

    // Cameras without motion detection do not depend on dual streaming.
    setCameras(cameras);
    ASSERT_EQ(state().motion.dependingOnDualStreaming, CombinedValue::None);
    ASSERT_TRUE(state().motion.enabled.equals(false));
    ASSERT_TRUE(state().expert.dualStreamingDisabled.equals(false));

    store().setDualStreamingDisabled(true);
    ASSERT_EQ(state().motion.streamAlert, std::nullopt);
    ASSERT_TRUE(state().expert.dualStreamingDisabled.equals(true));

    // Enabling motion detection on one camera makes it depending on dual streaming.
    camera1->setMotionType(MotionType::software);
    ASSERT_FALSE(state().motion.enabled.hasValue());
    ASSERT_EQ(state().motion.dependingOnDualStreaming, CombinedValue::Some);
    ASSERT_EQ(state().motion.streamAlert, StreamAlert::willBeImplicitlyDisabled);

    // Forcing motion detection.
    store().forceMotionDetection();
    ASSERT_TRUE(state().expert.dualStreamingDisabled.equals(true));
    ASSERT_TRUE(state().motion.forced());
    ASSERT_EQ(state().motion.streamAlert, std::nullopt);

    // Re-enabling dual-streaming.
    store().setDualStreamingDisabled(false);
    ASSERT_TRUE(state().expert.dualStreamingDisabled.equals(false));
    ASSERT_TRUE(state().motion.forced()); //< Still stays true.
    ASSERT_EQ(state().motion.streamAlert, std::nullopt);

    // Applying changes to cameras doesn't force motion detection as dual-streaming is not disabled.
    Applier::applyStateToCameras(state(), cameras);
    ASSERT_TRUE(camera1->motionStreamIndex() == makeStreamIndex(StreamIndex::secondary, false));
    ASSERT_TRUE(camera2->motionStreamIndex() == makeStreamIndex(StreamIndex::secondary, false));

    setCameras({cameras});
    ASSERT_FALSE(state().motion.enabled.hasValue());
    ASSERT_TRUE(state().expert.dualStreamingDisabled.equals(false));
    ASSERT_FALSE(state().motion.forced());
    ASSERT_EQ(state().motion.streamAlert, std::nullopt);

    // Disabling dual-streaming and forcing motion detection.
    store().setDualStreamingDisabled(true);
    ASSERT_TRUE(state().expert.dualStreamingDisabled.equals(true));
    ASSERT_FALSE(state().motion.forced());
    ASSERT_EQ(state().motion.streamAlert, StreamAlert::willBeImplicitlyDisabled);
    store().forceMotionDetection();
    ASSERT_TRUE(state().motion.forced());
    ASSERT_EQ(state().motion.streamAlert, std::nullopt);

    // Applying changes to cameras forces motion detection only on affected camera.
    Applier::applyStateToCameras(state(), cameras);
    ASSERT_TRUE(camera1->motionStreamIndex() == makeStreamIndex(StreamIndex::primary, true));
    ASSERT_TRUE(camera2->motionStreamIndex() == makeStreamIndex(StreamIndex::primary, false));

    setCameras({cameras});
    ASSERT_FALSE(state().motion.enabled.hasValue());
    ASSERT_EQ(state().motion.dependingOnDualStreaming, CombinedValue::None);
    ASSERT_TRUE(state().expert.dualStreamingDisabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, std::nullopt);
}

TEST_F(CameraSettingsMotionStreamsTest, disableDualStreamingAndForceDetection)
{
    auto camera = makeCamera();
    NX_ASSERT(camera->getMotionType() == MotionType::software);
    NX_ASSERT(camera->motionStreamIndex() == makeStreamIndex(StreamIndex::secondary, false));
    NX_ASSERT(camera->hasDualStreaming());

    setCameras({camera});

    // Disabling dual-streaming causes motion stream to implicitly change.
    store().setDualStreamingDisabled(true);
    ASSERT_EQ(state().motion.stream(), StreamIndex::secondary);
    ASSERT_EQ(state().effectiveMotionStream(), StreamIndex::primary);
    ASSERT_FALSE(state().motion.forced());
    ASSERT_EQ(state().motion.streamAlert, StreamAlert::willBeImplicitlyDisabled);

    store().forceMotionDetection();
    ASSERT_EQ(state().motion.stream(), StreamIndex::secondary);
    ASSERT_EQ(state().effectiveMotionStream(), StreamIndex::primary);
    ASSERT_TRUE(state().motion.forced());
    ASSERT_EQ(state().motion.streamAlert, StreamAlert::resolutionTooHigh);

    // Applying changes to camera saves implicitly selected stream.
    Applier::applyStateToCameras(state(), {camera});
    ASSERT_TRUE(camera->motionStreamIndex() == makeStreamIndex(StreamIndex::primary, true));

    setCameras({camera});
    ASSERT_EQ(state().motion.stream(), StreamIndex::primary);
    ASSERT_EQ(state().effectiveMotionStream(), StreamIndex::primary);
    ASSERT_TRUE(state().motion.forced());
    ASSERT_EQ(state().motion.streamAlert, StreamAlert::resolutionTooHigh);
}

TEST_F(CameraSettingsMotionStreamsTest, reEnableDualStreaming)
{
    auto camera = makeCamera();
    NX_ASSERT(camera->getMotionType() == MotionType::software);
    NX_ASSERT(camera->motionStreamIndex() == makeStreamIndex(StreamIndex::secondary, false));
    NX_ASSERT(camera->hasDualStreaming());

    setCameras({camera});

    // Disabling dual-streaming causes motion stream to implicitly change.
    store().setDualStreamingDisabled(true);
    ASSERT_EQ(state().motion.stream(), StreamIndex::secondary);
    ASSERT_EQ(state().effectiveMotionStream(), StreamIndex::primary);
    ASSERT_FALSE(state().motion.forced());
    ASSERT_EQ(state().motion.streamAlert, StreamAlert::willBeImplicitlyDisabled);

    // Re-enabling dual-streaming causes motion stream to implicitly change back.
    store().setDualStreamingDisabled(false);
    ASSERT_EQ(state().motion.stream(), StreamIndex::secondary);
    ASSERT_EQ(state().effectiveMotionStream(), StreamIndex::secondary);
    ASSERT_FALSE(state().motion.forced());
    ASSERT_EQ(state().motion.streamAlert, std::nullopt);
}

TEST_F(CameraSettingsMotionStreamsTest, reEnableDualStreamingAfterChangingStream)
{
    auto camera = makeCamera();
    NX_ASSERT(camera->getMotionType() == MotionType::software);
    NX_ASSERT(camera->motionStreamIndex() == makeStreamIndex(StreamIndex::secondary, false));
    NX_ASSERT(camera->hasDualStreaming());

    setCameras({camera});
    store().setMotionStream(StreamIndex::primary);

    // Disabling dual-streaming doesn't cause motion stream to change as it's already primary.
    store().setDualStreamingDisabled(true);
    ASSERT_EQ(state().motion.stream(), StreamIndex::primary);
    ASSERT_EQ(state().effectiveMotionStream(), StreamIndex::primary);
    ASSERT_FALSE(state().motion.forced());
    ASSERT_EQ(state().motion.streamAlert, StreamAlert::resolutionTooHigh);

    // After re-enabling dual-streaming the motion stream is still primary.
    store().setDualStreamingDisabled(false);
    ASSERT_EQ(state().motion.stream(), StreamIndex::primary);
    ASSERT_EQ(state().effectiveMotionStream(), StreamIndex::primary);
    ASSERT_FALSE(state().motion.forced());
    ASSERT_EQ(state().motion.streamAlert, StreamAlert::resolutionTooHigh);
}

TEST_F(CameraSettingsMotionStreamsTest, resolutionChangesExternally)
{
    auto camera = makeCamera();
    NX_ASSERT(camera->getMotionType() == MotionType::software);
    NX_ASSERT(camera->motionStreamIndex() == makeStreamIndex(StreamIndex::secondary, false));
    NX_ASSERT(camera->hasDualStreaming());

    setCameras({camera});

    // Externally increasing secondary stream resolution above threshold.
    // Motion detection is not forced and gets implicitly disabled.
    camera->setStreamResolution(StreamIndex::secondary, highResolution2);
    ASSERT_EQ(state().motion.stream(), StreamIndex::secondary);
    ASSERT_EQ(state().singleCameraProperties.primaryStreamResolution, highResolution1);
    ASSERT_EQ(state().singleCameraProperties.secondaryStreamResolution, highResolution2);
    ASSERT_FALSE(state().motion.forced());
    ASSERT_TRUE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, StreamAlert::implicitlyDisabled);

    // Externally decreasing secondary stream resolution below threshold.
    // Motion detection starts working again.
    camera->setStreamResolution(StreamIndex::secondary, lowResolution2);
    ASSERT_EQ(state().motion.stream(), StreamIndex::secondary);
    ASSERT_EQ(state().singleCameraProperties.primaryStreamResolution, highResolution1);
    ASSERT_EQ(state().singleCameraProperties.secondaryStreamResolution, lowResolution2);
    ASSERT_FALSE(state().motion.forced());
    ASSERT_TRUE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, std::nullopt);
}

TEST_F(CameraSettingsMotionStreamsTest, motionStreamChangesExternally)
{
    auto camera = makeCamera();
    NX_ASSERT(camera->getMotionType() == MotionType::software);
    NX_ASSERT(camera->motionStreamIndex() == makeStreamIndex(StreamIndex::secondary, false));
    NX_ASSERT(camera->hasDualStreaming());

    setCameras({camera});

    // Externally setting primary stream for forced motion detection.
    camera->setMotionStreamIndex({StreamIndex::primary, true});
    ASSERT_EQ(state().motion.stream(), StreamIndex::primary);
    ASSERT_TRUE(state().motion.forced());
    ASSERT_TRUE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, StreamAlert::resolutionTooHigh);

    // Externally un-forcing motion detection, it gets implicitly disabled.
    camera->setMotionStreamIndex({StreamIndex::primary, false});
    ASSERT_EQ(state().motion.stream(), StreamIndex::primary);
    ASSERT_FALSE(state().motion.forced());
    ASSERT_TRUE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, StreamAlert::implicitlyDisabled);
}

TEST_F(CameraSettingsMotionStreamsTest, motionDetectionChangesExternally)
{
    auto camera = makeCamera();
    camera->setMotionType(MotionType::none);
    camera->setMotionStreamIndex({StreamIndex::primary, false});
    NX_ASSERT(camera->getMotionType() == MotionType::none);
    NX_ASSERT(camera->motionStreamIndex() == makeStreamIndex(StreamIndex::primary, false));
    NX_ASSERT(camera->hasDualStreaming());

    setCameras({camera});

    // Externally enabling motion detection on primary stream doesn't force it.
    camera->setMotionType(MotionType::software);
    ASSERT_EQ(state().motion.stream(), StreamIndex::primary);
    ASSERT_FALSE(state().motion.forced());
    ASSERT_TRUE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, StreamAlert::implicitlyDisabled);

    // Externally forcing it.
    camera->setMotionStreamIndex({StreamIndex::primary, true});
    ASSERT_EQ(state().motion.stream(), StreamIndex::primary);
    ASSERT_TRUE(state().motion.forced());
    ASSERT_TRUE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, StreamAlert::resolutionTooHigh);

    // Externally disabling motion detection doesn't clear force flag.
    camera->setMotionType(MotionType::none);
    ASSERT_EQ(state().motion.stream(), StreamIndex::primary);
    ASSERT_TRUE(state().motion.forced());
    ASSERT_FALSE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, std::nullopt);
}

TEST_F(CameraSettingsMotionStreamsTest, dualStreamingChangesExternally)
{
    auto camera = makeCamera();
    NX_ASSERT(camera->getMotionType() == MotionType::software);
    NX_ASSERT(camera->motionStreamIndex() == makeStreamIndex(StreamIndex::secondary, false));
    NX_ASSERT(camera->hasDualStreaming());

    setCameras({camera});

    // Externally disabling dual-streaming causes motion stream to implicitly change.
    camera->setDisableDualStreaming(true);
    ASSERT_TRUE(camera->motionStreamIndex() == makeStreamIndex(StreamIndex::primary, false));

    ASSERT_EQ(state().motion.stream(), StreamIndex::primary);
    ASSERT_FALSE(state().motion.forced());
    ASSERT_TRUE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, StreamAlert::implicitlyDisabled);

    // Externally re-enabling dual-streaming causes motion stream to implicitly change back.
    camera->setDisableDualStreaming(false);
    ASSERT_TRUE(camera->motionStreamIndex() == makeStreamIndex(StreamIndex::secondary, false));

    ASSERT_EQ(state().motion.stream(), StreamIndex::secondary);
    ASSERT_FALSE(state().motion.forced());
    ASSERT_TRUE(state().motion.enabled.equals(true));
    ASSERT_EQ(state().motion.streamAlert, std::nullopt);
}

} // namespace test
} // namespace nx::vms::client::desktop
