#include "resource_access_filter.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/webpage_resource.h>

QList<QnResourceAccessFilter::Filter> QnResourceAccessFilter::allFilters()
{
    return QList<Filter>() << CamerasFilter << LayoutsFilter;
}

QnResourceList QnResourceAccessFilter::filteredResources(Filter filter, const QnResourceList& source)
{
    switch (filter)
    {
    case QnResourceAccessFilter::CamerasFilter:
        return source.filtered<QnResource>([](const QnResourcePtr& resource)
        {
            if (resource->hasFlags(Qn::desktop_camera))
                return false;
            return (resource->flags().testFlag(Qn::web_page) || resource->flags().testFlag(Qn::server_live_cam));
        });

    case QnResourceAccessFilter::LayoutsFilter:
        return source.filtered<QnLayoutResource>([](const QnLayoutResourcePtr& layout)
        {
            return /*layout->isShared() && */!layout->isFile(); //TODO: #GDM #access user should not see own layouts here, admin should
        });
    }
    return QnResourceList();
}

QSet<QnUuid> QnResourceAccessFilter::filteredResources(Filter filter, const QSet<QnUuid>& source)
{
    QSet<QnUuid> result;
    for (const auto& resource : filteredResources(filter, qnResPool->getResources(source)))
        result << resource->getId();
    return result;
}
