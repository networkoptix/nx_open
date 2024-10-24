// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/camera/buttons/base_camera_button_controller.h>

namespace nx::vms::client::mobile {

/** PTZ camera button controller. */
class PtzCameraButtonController: public core::BaseCameraButtonController
{
    Q_OBJECT
    using base_type = BaseCameraButtonController;

public:
    PtzCameraButtonController(
        core::CameraButtonData::Group buttonGroup,
        QObject* parent = nullptr);
    virtual ~PtzCameraButtonController() override;

protected:
    virtual void setResourceInternal(const QnResourcePtr& resource) override;

private:
    virtual bool setButtonActionState(
        const core::CameraButtonData& button,
        ActionState state) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::mobile
