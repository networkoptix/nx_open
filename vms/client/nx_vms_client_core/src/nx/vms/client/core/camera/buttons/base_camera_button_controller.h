// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>

#include "abstract_camera_button_controller.h"

namespace nx::vms::client::core {

/**
 * Base camera button controller which implements basic functionality:
 * 1. Required permissions change handling - needed permissions can be specified as a parameter.
 * 2. Buttons additions/removals - buttons lists management.
 * 3. Basic action state change implementations - current active actions management, etc.
 * 4. Button group setting - can be soecified at creation and automatically set for each button.
 */
class NX_VMS_CLIENT_CORE_API BaseCameraButtonController: public AbstractCameraButtonController
{
    Q_OBJECT
    using base_type = AbstractCameraButtonController;

public:
    /**
     * Constructs controller with specified parameters.
     * @param buttonGroup Buttons grouping number.
     * @param requiredPermissions Permissions reqiured for all the buttons of the controller.
     * @param parent Parent class.
     */
    BaseCameraButtonController(
        CameraButtonData::Group buttonGroup,
        Qn::Permissions requiredPermissions = {},
        QObject* parent = nullptr);

    virtual ~BaseCameraButtonController() override;

    virtual CameraButtonDataList buttonsData() const override;
    virtual OptionalCameraButtonData buttonData(const nx::Uuid& buttonId) const override;

    virtual bool startAction(const nx::Uuid& buttonId) override;
    virtual bool stopAction(const nx::Uuid& buttonId) override;
    virtual bool cancelAction(const nx::Uuid& buttonId) override;
    virtual bool actionIsActive(const nx::Uuid& buttonId) const override;

protected:
    enum class ActionState
    {
        inactive,
        active
    };

    virtual void setResourceInternal(const QnResourcePtr& resource) override;

    bool hasRequiredPermissions() const;

    QnVirtualCameraResourcePtr camera() const;

    /**
     * Adds or updates button record.
     * Returns 'true' if button is added or 'false' if is updated.
     */
    bool addOrUpdateButton(const CameraButtonData& button);

    /** Removes specified button. */
    bool removeButton(const nx::Uuid& buttonId);

    /** Convenient function to return first button (if any).*/
    OptionalCameraButtonData firstButton() const;

    /** Adds specified action in the active actions list. */
    void addActiveAction(const nx::Uuid& buttonId);

    /** Removes specified action from the active actions list. */
    void removeActiveAction(const nx::Uuid& buttonId);

    /**
     *  Virtual function to be imple,emted in subclasses which is used to activate or deactivate
     *  action. Returns 'true' if state could be changed, otherwise 'false'.
     */
    virtual bool setButtonActionState(const CameraButtonData& button, ActionState state);

signals:
    void hasRequiredPermissionsChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
