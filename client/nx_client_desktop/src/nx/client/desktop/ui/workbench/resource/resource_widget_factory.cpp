#include "resource_widget_factory.h"

#include <core/resource_access/resource_access_filter.h>
#include <core/resource_management/resource_pool.h>

#include <ui/graphics/items/resource/server_resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/resource/videowall_screen_widget.h>
#include <ui/graphics/items/resource/web_resource_widget.h>
#include <nx/client/desktop/ui/workbench/resource/layout_tour_item_widget.h>

#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>

#include <nx/utils/log/log.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

QnResourceWidget* ResourceWidgetFactory::createWidget(QnWorkbenchContext* context,
    QnWorkbenchItem* item)
{
    NX_EXPECT(context);
    NX_EXPECT(item);
    if (!context || !item)
        return nullptr;

    // Invalid items may lead to very strange behavior bugs.
    if (item->uuid().isNull())
    {
        NX_LOG(lit("ResourceWidgetFactory: null item uuid"), cl_logDEBUG1);
        return nullptr;
    }

    const auto resource = item->resource();
    if (!resource)
        return nullptr;

    const auto requiredPermission = QnResourceAccessFilter::isShareableMedia(resource)
        ? Qn::ViewContentPermission
        : Qn::ReadPermission;

    if (!context->accessController()->hasPermissions(resource, requiredPermission))
    {
        NX_LOG(lit("ResourceWidgetFactory: insufficient permissions"), cl_logDEBUG1);
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

    if (resource->hasFlags(Qn::media))
        return new QnMediaResourceWidget(context, item);

    if (resource->hasFlags(Qn::web_page))
        return new QnWebResourceWidget(context, item);

    NX_EXPECT(false, lit("ResourceWidgetFactory: unsupported resource type %1")
        .arg(resource->flags()));
    return nullptr;
}

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx


