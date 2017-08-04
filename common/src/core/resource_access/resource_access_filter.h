#pragma once

#include <core/resource/resource_fwd.h>

#include <nx/utils/uuid.h>

/**
 * Class for filtering accessible resources by global permissions.
 * We have separate global permissions for access to some kind of resources.
 * For example, Qn::GlobalAccessAllMediaPermission gives access to all
 * cameras and to all web pages.
 * This class encapsulates logic to filter such resources lists, e.g. to
 * display all accessible resources, grouped by governing permission flag.
 */
class QnResourceAccessFilter
{
public:
    enum Filter
    {
        MediaFilter,
        LayoutsFilter
    };

    static bool isShareable(const QnResourcePtr& resource);
    static bool isShareable(Filter filter, const QnResourcePtr& resource);
    static bool isShareableMedia(const QnResourcePtr& resource)
    {
        return isShareable(MediaFilter, resource);
    }
    static bool isShareableLayout(const QnResourcePtr& resource)
    {
        return isShareable(LayoutsFilter, resource);
    }

    /**
     * Check if resource can be opened in a common layout...
     * ... OR is a layout itself
     * ... OR is a videowall.
     */
    static bool isDroppable(const QnResourcePtr& resource);

    /**
    * Check if resource can be opened in a common layout...
    * ... OR is a layout itself.
    */
    static bool isOpenableInEntity(const QnResourcePtr& resource);

    // Resource can be opened in a common layout.
    static bool isOpenableInLayout(const QnResourcePtr& resource);

    static QList<QnResourceAccessFilter::Filter> allFilters();

    static QnResourceList filteredResources(Filter filter, const QnResourceList& source);
    static QSet<QnUuid> filteredResources(
        QnResourcePool* resPool,
        Filter filter,
        const QSet<QnUuid>& source);
};
