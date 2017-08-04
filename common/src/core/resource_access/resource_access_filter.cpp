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
                || flags.testFlag(Qn::live_cam)
                || (flags.testFlag(Qn::remote_server) && !flags.testFlag(Qn::fake));

        case QnResourceAccessFilter::LayoutsFilter:
            return flags.testFlag(Qn::layout) && !flags.testFlag(Qn::exported);

        default:
            NX_ASSERT(false);
            break;
    }

    return false;
}

bool QnResourceAccessFilter::isDroppable(const QnResourcePtr& resource)
{
    NX_EXPECT(resource);
    if (!resource)
        return false;

    return resource->hasFlags(Qn::videowall)
        || isOpenableInEntity(resource);
}

bool QnResourceAccessFilter::isOpenableInEntity(const QnResourcePtr& resource)
{
    NX_EXPECT(resource);
    if (!resource)
        return false;

    return resource->hasFlags(Qn::layout)
        || isOpenableInLayout(resource);
}

bool QnResourceAccessFilter::isOpenableInLayout(const QnResourcePtr& resource)
{
    NX_EXPECT(resource);
    if (!resource)
        return false;

    return isShareableMedia(resource) || resource->hasFlags(Qn::local_media);
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

QSet<QnUuid> QnResourceAccessFilter::filteredResources(
    QnResourcePool* resPool,
    Filter filter,
    const QSet<QnUuid>& source)
{
    QSet<QnUuid> result;
    for (const auto& resource : filteredResources(filter, resPool->getResources(source)))
        result << resource->getId();
    return result;
}
