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

    int calculateButtonsVisibility() const
    {
        return Qn::WidgetButtons::CloseButton;
    }

    Qn::ResourceStatusOverlay calculateStatusOverlay() const
    {
        switch (crossSystemCamera->crossSystemContext()->status())
        {
            case CloudCrossSystemContext::Status::uninitialized:
                return Qn::ResourceStatusOverlay::OfflineOverlay;
            case CloudCrossSystemContext::Status::connecting:
                return Qn::ResourceStatusOverlay::LoadingOverlay;
            case CloudCrossSystemContext::Status::connectionFailure:
                return Qn::InformationRequiredOverlay;
            default:
                return Qn::EmptyOverlay;
        }
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
        return d->calculateButtonsVisibility();

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
