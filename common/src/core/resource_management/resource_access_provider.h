#pragma once

#include <core/resource_management/resource_access_subject.h>

//TODO: #vkutin #GDM Need to move it to some forward declarations header
using QnIndirectAccessProviders = QMap<QnUuid /*accessible resource*/, QSet<QnResourcePtr> /*access providers*/>;

/**
 * This class governs ways of access to resources. For example, camera may be accessible
 * directly - or by shared layout where it is located.
 */
class QnResourceAccessProvider
{
public:
    QnResourceAccessProvider();

    //TODO: #GDM think about naming
    enum class Access
    {
        Forbidden,
        Directly,
        ViaLayout,
        ViaVideowall
    };

    /** Check if resource (camera, webpage or layout) is available to given user or role. */
    bool isAccessibleResource(const QnResourceAccessSubject& subject, const QnResourcePtr& resource) const;
    Access resourceAccess(const QnResourceAccessSubject& subject, const QnResourcePtr& resource) const;

    /** Finds which layouts are indirectly available (e.g. through videowall) to given user or group. */
    //TODO: #vkutin #GDM Refactoring is probably needed to merge this functionality with isAccessibleResource functions.
    QnIndirectAccessProviders indirectlyAccessibleLayouts(const QnResourceAccessSubject& subject) const;

    /** List of resources ids, the given user has access to (only given directly). */
    QSet<QnUuid> sharedResources(const QnResourceAccessSubject& subject) const;
private:
    /** Check if given desktop camera or layout is available to given user/role through videowall. */
    bool isAccessibleViaVideowall(const QnResourceAccessSubject& subject, const QnResourcePtr& resource) const;

    /** Check if camera is placed to one of shared layouts, available to given user. */
    bool isAccessibleViaLayouts(const QSet<QnUuid>& layoutIds, const QnResourcePtr& resource, bool sharedOnly) const;
};
