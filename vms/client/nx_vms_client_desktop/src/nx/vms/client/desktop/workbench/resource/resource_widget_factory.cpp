// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_widget_factory.h"

#include <core/resource/client_camera.h>
#include <core/resource/layout_resource.h>
#include <core/resource_access/resource_access_filter.h>

#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/intercom/intercom_resource_widget.h>
#include <nx/vms/common/intercom/utils.h>
#include <ui/graphics/items/resource/server_resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/resource/videowall_screen_widget.h>
#include <ui/graphics/items/resource/web_resource_widget.h>
#include <nx/vms/client/desktop/workbench/resource/layout_tour_item_widget.h>

#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>

#include <nx/utils/log/log.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {
namespace ui {
namespace workbench {

QnResourceWidget* ResourceWidgetFactory::createWidget(QnWorkbenchContext* context,
    QnWorkbenchItem* item)
{
    NX_ASSERT(context);
    NX_ASSERT(item);
    if (!context || !item)
        return nullptr;

    // Invalid items may lead to very strange behavior bugs.
    if (item->uuid().isNull())
    {
        NX_DEBUG(typeid(ResourceWidgetFactory), lit("ResourceWidgetFactory: null item uuid"));
        return nullptr;
    }

    const auto resource = item->resource();
    if (!resource)
        return nullptr;

    const bool itemIsIntercomLayout = resource->hasFlags(Qn::media)
        && nx::vms::common::isIntercomLayout(item->layout()->resource());

    const auto requiredPermission = QnResourceAccessFilter::isShareableMedia(resource)
        ? Qn::ViewContentPermission
        : Qn::ReadPermission;

    // Intercom cameras can be placed on layout even if there are no permissions.
    if (!itemIsIntercomLayout
            && !context->accessController()->hasPermissions(resource, requiredPermission))
    {
        NX_DEBUG(typeid(ResourceWidgetFactory), lit("ResourceWidgetFactory: insufficient permissions"));
        return nullptr;
    }

    auto layout = item->layout();
    const bool isLayoutTourReview = layout && layout->isLayoutTourReview();
    if (isLayoutTourReview)
        return new LayoutTourItemWidget(context, item);

    if (resource->hasFlags(Qn::server))
        return new QnServerResourceWidget(context, item);

    if (resource->hasFlags(Qn::videowall))
        return new QnVideowallScreenWidget(context, item);

    QnClientCameraResourcePtr camera = resource.dynamicCast<QnClientCameraResource>();
    if (itemIsIntercomLayout && camera && camera->isIntercom())
        return new IntercomResourceWidget(context, item);

    if (resource->hasFlags(Qn::media))
        return new QnMediaResourceWidget(context, item);

    if (resource->hasFlags(Qn::web_page))
        return new QnWebResourceWidget(context, item);

    NX_ASSERT(false, lit("ResourceWidgetFactory: unsupported resource type %1")
        .arg(resource->flags()));
    return nullptr;
}

} // namespace workbench
} // namespace ui
} // namespace nx::vms::client::desktop


