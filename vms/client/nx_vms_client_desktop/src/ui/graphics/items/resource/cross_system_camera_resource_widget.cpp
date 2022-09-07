// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cross_system_camera_resource_widget.h"

#include <nx/vms/client/desktop/cross_system/cross_system_camera_resource.h>
#include <nx/vms/client/desktop/resource/resource_descriptor.h>
#include <ui/graphics/items/overlays/status_overlay_controller.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>

using nx::vms::client::desktop::CloudCrossSystemContext;
using nx::vms::client::desktop::CrossSystemCameraResource;
using nx::vms::client::desktop::CrossSystemCameraResourcePtr;

struct QnCrossSystemCameraWidget::Private
{
    CrossSystemCameraResourcePtr crossSystemCamera;

    Qn::ResourceStatusOverlay calculateStatusOverlay() const
    {
        const auto crossSystemContext = crossSystemCamera->crossSystemContext();
        const auto status = crossSystemContext->status();
        if (status == CloudCrossSystemContext::Status::uninitialized
            || (status == CloudCrossSystemContext::Status::connected
                && !crossSystemContext->isSystemReadyToUse()))
        {
            return Qn::ResourceStatusOverlay::OfflineOverlay;
        }

        if (status == CloudCrossSystemContext::Status::connecting)
            return Qn::ResourceStatusOverlay::LoadingOverlay;

        if (status == CloudCrossSystemContext::Status::connectionFailure)
            return Qn::ResourceStatusOverlay::InformationRequiredOverlay;

        return Qn::EmptyOverlay;
    }
};

QnCrossSystemCameraWidget::QnCrossSystemCameraWidget(
    nx::vms::client::desktop::SystemContext* systemContext,
    nx::vms::client::desktop::WindowContext* windowContext,
    QnWorkbenchItem* item,
    QGraphicsItem* parent)
    :
    QnMediaResourceWidget(systemContext, windowContext, item, parent),
    d(new Private{QnMediaResourceWidget::resource().dynamicCast<CrossSystemCameraResource>()})
{
    NX_ASSERT(d->crossSystemCamera);

    connect(
        statusOverlayController(),
        &QnStatusOverlayController::buttonClicked,
        this,
        [this](Qn::ResourceOverlayButton button)
        {
            if (button != Qn::ResourceOverlayButton::RequestInformation)
                return;

            if (!d->crossSystemCamera)
                return;

            const auto systemId = d->crossSystemCamera->crossSystemContext()->systemId();

            menu()->trigger(
                nx::vms::client::desktop::ui::action::ConnectToCloudSystemWithUserInteractionAction,
                {Qn::CloudSystemIdRole, systemId});
        });
}

QnCrossSystemCameraWidget::~QnCrossSystemCameraWidget() = default;

int QnCrossSystemCameraWidget::calculateButtonsVisibility() const
{
    if (d->crossSystemCamera && d->crossSystemCamera->hasFlags(Qn::fake))
        return Qn::WidgetButtons::CloseButton;

    return QnMediaResourceWidget::calculateButtonsVisibility();
}

Qn::ResourceStatusOverlay QnCrossSystemCameraWidget::calculateStatusOverlay() const
{
    if (d->crossSystemCamera && d->crossSystemCamera->hasFlags(Qn::fake))
        return d->calculateStatusOverlay();

    return QnMediaResourceWidget::calculateStatusOverlay();
}

Qn::ResourceOverlayButton QnCrossSystemCameraWidget::calculateOverlayButton(
    Qn::ResourceStatusOverlay statusOverlay) const
{
    if (statusOverlay == Qn::ResourceStatusOverlay::InformationRequiredOverlay)
        return Qn::ResourceOverlayButton::RequestInformation;

    if (d->crossSystemCamera && d->crossSystemCamera->hasFlags(Qn::fake))
        return Qn::ResourceOverlayButton::Empty;

    return QnMediaResourceWidget::calculateOverlayButton(statusOverlay);
}
