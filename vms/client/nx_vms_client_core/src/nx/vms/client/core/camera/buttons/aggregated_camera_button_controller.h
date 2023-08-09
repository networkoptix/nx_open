// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>

#include "abstract_camera_button_controller.h"

namespace nx::vms::client::core {

/**
 * Aggregating camera button controller containing all avaliable buttons from the all suitable
 * controllers.
 */
class NX_VMS_CLIENT_CORE_API AggregatedCameraButtonController:
    public AbstractCameraButtonController
{
    Q_OBJECT
    using base_type = AbstractCameraButtonController;

public:
    AggregatedCameraButtonController(
        SystemContext* context,
        QObject* parent = nullptr);

    AggregatedCameraButtonController(
        std::unique_ptr<nx::vms::common::SystemContextInitializer> contextInitializer,
        QObject* parent = nullptr);

    virtual ~AggregatedCameraButtonController() override;

    using ControllerPtr = std::shared_ptr<AbstractCameraButtonController>;

    /** Adds controller with specified group. */
    void addController(int group, const ControllerPtr& controller);
    void removeController(int group);

    template<typename Controller, typename ButtonGroupType, typename... Args>
    void addController(ButtonGroupType group, SystemContext* context, Args... args);

    virtual CameraButtons buttons() const override;
    virtual OptionalCameraButton button(const QnUuid& id) const override;

    virtual bool startAction(const QnUuid& buttonId) override;
    virtual bool stopAction(const QnUuid& buttonId) override;
    virtual bool cancelAction(const QnUuid& buttonId) override;
    virtual bool actionIsActive(const QnUuid& buttonId) const override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

template<typename Controller, typename ButtonGroupType, typename... Args>
void AggregatedCameraButtonController::addController(
    ButtonGroupType group,
    SystemContext* context,
    Args... args)
{
    const auto controller = std::make_shared<Controller>(
        std::forward<Args>(args)..., static_cast<int>(group), context);
    addController(static_cast<int>(group), controller);
}

} // namespace nx::vms::client::core
