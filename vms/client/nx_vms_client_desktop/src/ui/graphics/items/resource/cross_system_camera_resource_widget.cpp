// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cross_system_camera_resource_widget.h"

#include <qt_graphics_items/graphics_label.h>

#include <core/resource/user_resource.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/core/cross_system/cross_system_camera_resource.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/system_finder/system_description.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/common/system_settings.h>
#include <ui/graphics/items/overlays/hud_overlay_widget.h>
#include <ui/graphics/items/overlays/resource_title_item.h>
#include <ui/graphics/items/overlays/status_overlay_controller.h>
#include <ui/workbench/workbench_context.h>

using namespace nx::vms::client;
using namespace nx::vms::client::desktop;

struct QnCrossSystemCameraWidget::Private
{
    core::CrossSystemCameraResourcePtr crossSystemCamera;
    core::CloudCrossSystemContext* context;

    Qn::ResourceStatusOverlay calculateStatusOverlay() const
    {
        if (NX_ASSERT(context) && context->systemDescription()->isOnline())
        {
            const auto status = context->status();
            if (status == core::CloudCrossSystemContext::Status::connecting)
                return Qn::ResourceStatusOverlay::LoadingOverlay;

            if (context->systemDescription()->is2FaEnabled()
                && !appContext()->cloudStatusWatcher()->is2FaEnabledForUser())
            {
                return Qn::ResourceStatusOverlay::TwoFADisabledOverlay;
            }

            if (status == core::CloudCrossSystemContext::Status::connectionFailure)
            {
                if (const auto error = context->connectionError())
                {
                    if (error->code == core::RemoteConnectionErrorCode::certificateRejected
                        || error->code == core::RemoteConnectionErrorCode::truncatedSessionToken
                        || error->code == core::RemoteConnectionErrorCode::unauthorized)
                    {
                        return Qn::ResourceStatusOverlay::InformationRequiredOverlay;
                    }
                }

                return Qn::ResourceStatusOverlay::ConnectionLostOverlay;
            }

            return Qn::ResourceStatusOverlay::UnauthorizedOverlay;
        }

        return Qn::OfflineOverlay;
    }
};

QnCrossSystemCameraWidget::QnCrossSystemCameraWidget(
    desktop::SystemContext* systemContext,
    desktop::WindowContext* windowContext,
    QnWorkbenchItem* item,
    QGraphicsItem* parent)
    :
    QnMediaResourceWidget(systemContext, windowContext, item, parent),
    d(new Private{QnMediaResourceWidget::resource().dynamicCast<
        core::CrossSystemCameraResource>()})
{
    NX_ASSERT(d->crossSystemCamera);
    const auto systemId = d->crossSystemCamera->systemId();
    appContext()->cloudCrossSystemManager()->setPriority(
        systemId, core::CloudCrossSystemManager::Priority::high);

    d->context = appContext()->cloudCrossSystemManager()->systemContext(systemId);
    connect(
        statusOverlayController(),
        &QnStatusOverlayController::buttonClicked,
        this,
        [this](Qn::ResourceOverlayButton button)
        {
            if (button != Qn::ResourceOverlayButton::LogIn)
                return;

            if (!d->crossSystemCamera)
                return;

            const auto systemId = d->crossSystemCamera->systemId();

            menu()->trigger(
                nx::vms::client::desktop::menu::ConnectToCloudSystemWithUserInteractionAction,
                {Qn::CloudSystemIdRole, systemId});
        });

    connect(d->crossSystemCamera.get(), &QnResource::flagsChanged, this,
        [this]()
        {
            updateButtonsVisibility();
            updateOverlayButton();
        });

    updateButtonsVisibility();
    updateOverlayButton();
    updateTitleText();
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
    if (NX_ASSERT(d->context) && d->context->needsCloudAuthorization())
        return Qn::ResourceStatusOverlay::InformationRequiredOverlay;

    if (d->crossSystemCamera && d->crossSystemCamera->hasFlags(Qn::fake))
        return d->calculateStatusOverlay();

    return QnMediaResourceWidget::calculateStatusOverlay();
}

Qn::ResourceOverlayButton QnCrossSystemCameraWidget::calculateOverlayButton(
    Qn::ResourceStatusOverlay statusOverlay) const
{
    if (statusOverlay == Qn::ResourceStatusOverlay::UnauthorizedOverlay)
        return Qn::ResourceOverlayButton::Empty; // VMS-58843: Temporarily not supported.

    if (statusOverlay == Qn::ResourceStatusOverlay::TwoFADisabledOverlay)
        return Qn::ResourceOverlayButton::SetUp2FA;

    if (statusOverlay == Qn::ResourceStatusOverlay::InformationRequiredOverlay
        || statusOverlay == Qn::ResourceStatusOverlay::ConnectionLostOverlay)
    {
        return Qn::ResourceOverlayButton::LogIn;
    }

    if (d->crossSystemCamera && d->crossSystemCamera->hasFlags(Qn::fake))
        return Qn::ResourceOverlayButton::Empty;

    return QnMediaResourceWidget::calculateOverlayButton(statusOverlay);
}

QString QnCrossSystemCameraWidget::calculateTitleText() const
{
    if (d->context)
    {
        const auto resourceName = QnResourceWidget::resource()->getName();
        const auto systemName = d->context->systemDescription()->name();
        return QString("%1 / %2").arg(systemName, resourceName);
    }

    return QnMediaResourceWidget::calculateTitleText();
}
