#pragma once

#include <core/resource/resource_fwd.h>

#include <nx/utils/uuid.h>

/**
 * Class for filtering accessible resources by global permissions.
 * We have separate global permissions for access to some kind of resources.
 * For example, Qn::GlobalAccessAllCamerasPermission gives access to all
 * cameras and to all web pages.
 * This class encapsulates logic to filter such resources lists, e.g. to
 * display all accessible resources, grouped by governing permission flag.
 */
class QnResourceAccessFilter
{
public:
    enum Filter
    {
        CamerasFilter,
        LayoutsFilter,
        ServersFilter
    };

    static QList<QnResourceAccessFilter::Filter> allFilters();

    static QnResourceList filteredResources(Filter filter, const QnResourceList& source);
    static QSet<QnUuid> filteredResources(Filter filter, const QSet<QnUuid>& source);

    static Qn::GlobalPermission accessPermission(Filter filter);
    static QnResourceAccessFilter::Filter filterByPermission(Qn::GlobalPermission permission);
};
