// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fisheye_preview_controller.h"

#include <core/resource/media_resource.h>
#include <nx/vms/api/data/dewarping_data.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>

namespace nx::vms::client::desktop {

FisheyePreviewController::FisheyePreviewController(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
}

void FisheyePreviewController::preview(
    const QnMediaResourcePtr& resource,
    const nx::vms::api::dewarping::MediaData& params)
{
    if (resource != m_resource)
        rollback();

    m_resource = resource;

    if (!m_resource)
        return;

    for (const auto widget: display()->widgets(resource->toResourcePtr()))
    {
        const auto mediaWidget = qobject_cast<QnMediaResourceWidget*>(widget);

        // In the showreel preview mode we can have non-media widgets with a camera.
        if (!mediaWidget)
            continue;

        if (mediaWidget->dewarpingParams() == params)
            continue;

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

       // In the showreel preview mode we can have non-media widgets with a camera.
        if (mediaWidget)
            mediaWidget->setDewarpingParams(params);
    }
    m_resource.clear();
}

} // namespace nx::vms::client::desktop
