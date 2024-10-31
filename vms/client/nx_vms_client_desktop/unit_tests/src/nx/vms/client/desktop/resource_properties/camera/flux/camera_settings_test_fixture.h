// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/common/flux/flux_state_store.h>
#include <nx/vms/client/desktop/resource_properties/camera/flux/camera_settings_dialog_state.h>
#include <nx/vms/client/desktop/resource_properties/camera/flux/camera_settings_dialog_state_reducer.h>
#include <nx/vms/client/desktop/test_support/test_context.h>
#include <nx/vms/common/resource/camera_resource_stub.h>

namespace nx::vms::client::desktop {
namespace test {

class CameraSettingsTestFixture:
    public ContextBasedTest,
    public FluxStateStore<CameraSettingsDialogState>
{
public:
    using State = CameraSettingsDialogState;
    using Reducer = CameraSettingsDialogStateReducer;

    CameraSettingsTestFixture();
    virtual ~CameraSettingsTestFixture();

protected:
    enum class CameraFeature
    {
        none = 0,
        motion = 1 << 0,
        objects = 1 << 1,
    };
    Q_DECLARE_FLAGS(CameraFeatures, CameraFeature)

    CameraResourceStubPtr givenCamera(CameraFeatures features =
        CameraFeatures(CameraFeature::motion) | CameraFeature::objects);

    StubCameraResourceList cameras() const;

    void whenChangesAreSaved();

    void whenCameraRemoved(CameraResourceStubPtr camera);

    void whenCameraStatusChangedTo(CameraResourceStubPtr camera,
        nx::vms::api::ResourceStatus status);
    void whenCameraStatusChangedTo(nx::vms::api::ResourceStatus status);

    void whenCameraFeaturesEnabled(
        CameraResourceStubPtr camera,
        CameraFeatures features,
        bool on = true);

    /** Process all cameras at once. */
    void whenCameraFeaturesEnabled(
        CameraFeatures features,
        bool on = true);

    void whenMotionEnabled(bool on = true)
    {
        whenCameraFeaturesEnabled(CameraFeature::motion, on);
    }

    void whenObjectsEnabled(bool on = true)
    {
        whenCameraFeaturesEnabled(CameraFeature::objects, on);
    }

    void whenRecordingEnabled(bool on = true);
    void whenDualStreamingEnabled(bool on = true);
    void whenSecondaryStreamRecordingEnabled(bool on = true);
    void whenCamerasAreLoaded();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace test
} // namespace nx::vms::client::desktop
