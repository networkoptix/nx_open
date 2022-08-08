// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_widget_factory.h"

#include <core/resource/resource.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/log.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/workbench/resource/layout_tour_item_widget.h>
#include <ui/graphics/items/resource/cross_system_camera_resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/resource/server_resource_widget.h>
#include <ui/graphics/items/resource/videowall_screen_widget.h>
#include <ui/graphics/items/resource/web_resource_widget.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>

namespace nx::vms::client::desktop {
namespace ui {
namespace workbench {

QnResourceWidget* ResourceWidgetFactory::createWidget(QnWorkbenchItem* item)
{
    if (!NX_ASSERT(item))
        return nullptr;

    // Invalid items may lead to very strange behavior bugs.
    if (item->uuid().isNull())
    {
        NX_DEBUG(typeid(ResourceWidgetFactory), "ResourceWidgetFactory: null item uuid");
        return nullptr;
    }

    const auto resource = item->resource();
    if (!resource)
        return nullptr;

    auto systemContext = SystemContext::fromResource(resource);

    const auto requiredPermission = QnResourceAccessFilter::isShareableMedia(resource)
        ? Qn::ViewContentPermission
        : Qn::ReadPermission;

    if (!systemContext->accessController()->hasPermissions(resource, requiredPermission))
    {
        NX_DEBUG(typeid(ResourceWidgetFactory), lit("ResourceWidgetFactory: insufficient permissions"));
        return nullptr;
    }

    // TODO: #sivanov Take Window Context from the workbench item.
    auto windowContext = appContext()->mainWindowContext();

    auto layout = item->layout();
    const bool isLayoutTourReview = layout && layout->isLayoutTourReview();
    if (isLayoutTourReview)
        return new LayoutTourItemWidget(systemContext, windowContext, item);

    if (resource->hasFlags(Qn::server))
        return new QnServerResourceWidget(systemContext, windowContext, item);

    if (resource->hasFlags(Qn::videowall))
        return new QnVideowallScreenWidget(systemContext, windowContext, item);

    if (resource->hasFlags(Qn::cross_system))
        return new QnCrossSystemCameraWidget(systemContext, windowContext, item);

    if (resource->hasFlags(Qn::media))
        return new QnMediaResourceWidget(systemContext, windowContext, item);

    if (resource->hasFlags(Qn::web_page))
        return new QnWebResourceWidget(systemContext, windowContext, item);

    NX_ASSERT(false, "ResourceWidgetFactory: unsupported resource type %1",
        resource->flags());
    return nullptr;
}

} // namespace workbench
} // namespace ui
} // namespace nx::vms::client::desktop
