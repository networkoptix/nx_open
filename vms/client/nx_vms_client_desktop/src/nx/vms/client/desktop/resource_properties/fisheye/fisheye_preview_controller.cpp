#include "fisheye_preview_controller.h"

#include <core/resource/media_resource.h>
#include <core/ptz/media_dewarping_params.h>

#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>

namespace nx::vms::client::desktop {

FisheyePreviewController::FisheyePreviewController(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent, InitializationMode::lazy)
{
}

void FisheyePreviewController::preview(
    const QnMediaResourcePtr& resource,
    const QnMediaDewarpingParams& params)
{
    if (resource != m_resource)
        rollback();

    m_resource = resource;

    if (!m_resource)
        return;

    for (const auto widget: display()->widgets(resource->toResourcePtr()))
    {
        const auto mediaWidget = qobject_cast<QnMediaResourceWidget*>(widget);
        if (NX_ASSERT(mediaWidget))
            mediaWidget->setDewarpingParams(params);

        const auto item = widget->item();
        auto itemParams = item->dewarpingParams();
        itemParams.enabled = params.enabled;
        item->setDewarpingParams(itemParams);
    }
}

void FisheyePreviewController::rollback()
{
    if (!m_resource)
        return;

    const auto params = m_resource->getDewarpingParams();
    for (const auto widget: display()->widgets(m_resource->toResourcePtr()))
    {
        const auto mediaWidget = qobject_cast<QnMediaResourceWidget*>(widget);
        if (NX_ASSERT(mediaWidget))
            mediaWidget->setDewarpingParams(params);
    }
    m_resource.clear();
}

} // namespace nx::vms::client::desktop
