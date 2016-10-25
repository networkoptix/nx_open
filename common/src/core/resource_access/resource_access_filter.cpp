#include "resource_access_filter.h"

#include <core/resource_management/resource_pool.h>

namespace {

static const QList<QnResourceAccessFilter::Filter> kAllFilters {
    QnResourceAccessFilter::MediaFilter,
    QnResourceAccessFilter::LayoutsFilter
};

}

bool QnResourceAccessFilter::isShareable(const QnResourcePtr& resource)
{
    return std::any_of(kAllFilters.cbegin(), kAllFilters.cend(),
        [resource](Filter filter)
        {
            return isShareable(filter, resource);
        });
}

bool QnResourceAccessFilter::isShareable(Filter filter, const QnResourcePtr& resource)
{
    auto flags = resource->flags();
    if (flags.testFlag(Qn::desktop_camera))
        return false;

    switch (filter)
    {
        case QnResourceAccessFilter::MediaFilter:
            return flags.testFlag(Qn::web_page)
                || flags.testFlag(Qn::server_live_cam)
                || (flags.testFlag(Qn::remote_server) && !flags.testFlag(Qn::fake));

        case QnResourceAccessFilter::LayoutsFilter:
            return flags.testFlag(Qn::layout) && !flags.testFlag(Qn::exported);

        default:
            NX_ASSERT(false);
            break;
    }

    return false;
}

QList<QnResourceAccessFilter::Filter> QnResourceAccessFilter::allFilters()
{
    return kAllFilters;
}

QnResourceList QnResourceAccessFilter::filteredResources(Filter filter, const QnResourceList& source)
{
    switch (filter)
    {
        case QnResourceAccessFilter::MediaFilter:
            return source.filtered([](const QnResourcePtr& resource)
            {
                return isShareable(resource) && !resource->hasFlags(Qn::layout);
            });

        case QnResourceAccessFilter::LayoutsFilter:
            return source.filtered([](const QnResourcePtr& resource)
            {
                return isShareable(resource) && resource->hasFlags(Qn::layout);
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
