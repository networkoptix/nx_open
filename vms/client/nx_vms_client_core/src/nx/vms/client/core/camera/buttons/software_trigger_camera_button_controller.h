// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>

#include "base_camera_button_controller.h"

namespace nx::vms::client::core {

/** Software triggers button controller. */
class NX_VMS_CLIENT_CORE_API SoftwareTriggerCameraButtonController:
    public BaseCameraButtonController
{
    Q_OBJECT
    using base_type = BaseCameraButtonController;

public:
    enum class HintStyle
    {
        mobile,
        desktop
    };

    SoftwareTriggerCameraButtonController(
        HintStyle hintStyle,
        CameraButtonData::Group buttonGroup,
        QObject* parent = nullptr);
    virtual ~SoftwareTriggerCameraButtonController() override;

protected:
    virtual void setResourceInternal(const QnResourcePtr& resource) override;

private:
    virtual bool setButtonActionState(const CameraButtonData& button, ActionState state) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
