#include "workbench_text_overlays_handler.h"

#include <business/actions/abstract_business_action.h>
#include <business/business_strings_helper.h>

#include <client/client_message_processor.h>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/graphics/items/overlays/text_overlay_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_access_controller.h>

QnWorkbenchTextOverlaysHandler::QnWorkbenchTextOverlaysHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(qnClientMessageProcessor, &QnClientMessageProcessor::businessActionReceived,
        this, &QnWorkbenchTextOverlaysHandler::at_businessActionReceived);
}

QnWorkbenchTextOverlaysHandler::~QnWorkbenchTextOverlaysHandler()
{
}

void QnWorkbenchTextOverlaysHandler::at_businessActionReceived(
    const QnAbstractBusinessActionPtr& businessAction)
{
    if (businessAction->actionType() != QnBusiness::ShowTextOverlayAction)
        return;

    if (!context()->user())
        return;

    const auto& actionParams = businessAction->getParams();

    const auto state = businessAction->getToggleState();
    const bool isProlongedAction = businessAction->isProlonged();
    const bool couldBeInstantEvent = (state == QnBusiness::UndefinedState);

    /* Do not accept instant events for prolonged actions. */
    if (isProlongedAction && couldBeInstantEvent)
        return;

    auto cameras = qnResPool->getResources<QnVirtualCameraResource>(
        businessAction->getResources());

    if (actionParams.useSource)
    {
        cameras << qnResPool->getResources<QnVirtualCameraResource>(
            businessAction->getSourceResources());
    }

    /* Remove duplicates. */
    std::sort(cameras.begin(), cameras.end());
    cameras.erase(std::unique(cameras.begin(), cameras.end()), cameras.end());

    const int timeout = isProlongedAction
        ? QnOverlayTextItemData::kInfinite
        : actionParams.durationMs;

    const auto id = businessAction->getBusinessRuleId();
    const bool finished = isProlongedAction && (state == QnBusiness::InactiveState);

    for (const auto& camera: cameras)
    {
        for (auto widget: display()->widgets(camera))
        {
            auto mediaWidget = dynamic_cast<QnMediaResourceWidget*>(widget);
            if (!mediaWidget)
                continue;

            QString caption;
            QString description = actionParams.text.trimmed();

            if (description.isEmpty())
            {
                const auto runtimeParams = businessAction->getRuntimeParams();
                caption = QnBusinessStringsHelper::eventAtResource(runtimeParams, Qn::RI_WithUrl);
                description = QnBusinessStringsHelper::eventDetails(runtimeParams).join(L'\n');
                mediaWidget->showTextOverlay(id, !finished, caption, description, timeout);
            }

            mediaWidget->showTextOverlay(id, !finished, caption, description, timeout);
        }
    }
}
