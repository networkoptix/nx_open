#pragma once

#include <QtCore/QMetaType>
#include <QtCore/QHash>

namespace nx::vms::client::desktop {

/**
 * Type of a node in resource tree. Ordered as it should be displayed.
 */
enum class ResourceTreeNodeType
{
    // Single-occurrence nodes.

    // Root node for the tree (current system node).
    root,
    // Root node, displaying current system name.
    currentSystem,
    // Root node, displaying current user.
    currentUser,
    // Root node for spacing between header and main part of the tree.
    separator,
    // Root node for servers for admin user.
    servers,
    // Root node for filtered servers.
    filteredServers,
    // Root node for cameras, i/o modules and statistics for non-admin user.
    userResources,
    // Root node for filtered cameras, i/o modules and other devices. Used for filtering purposes.
    filteredCameras,
    // Root node for current user's layouts and shared layouts.
    layouts,
    // Root node for filtered layouts.
    filteredLayouts,
    // Root node for the layout tours.
    layoutTours,
    // Root node for filtered videowalls.
    filteredVideowalls,
    // Root node for web pages.
    webPages,
    // Root node for user resources.
    users,
    // Root node for filtered users.
    filteredUsers,
    // Root node for analytics engines.
    analyticsEngines,
    // Root node for filtered analytics engines.
    filteredAnalyticsEngines,
    // Root node for remote systems.
    otherSystems,
    // Root node for local resources.
    localResources,
    // Root node for spacing between local resources header and resources.
    localSeparator,

    // Root node for hidden resources.
    bastard,

    // Per-user placeholder nodes.

    // 'Users' node, displayed under roles.
    roleUsers,
    // 'All Cameras' placeholder, displayed under users and roles with full access.
    allCamerasAccess,
    // 'All Shared Layouts' placeholder, displayed under admins.
    allLayoutsAccess,
    // 'Cameras & Resources' node, displayed under users and roles with custom access.
    sharedResources,
    // 'Layouts' node, displayed under users and roles with custom access.
    sharedLayouts,

    // Repeating nodes.

    // Custom role.
    role,
    // Shared layout link, displayed under user. Has only resource - shared layout.
    sharedLayout,
    // Recorder (VMAX, etc). Has both guid and resource (parent server).
    recorder,
    // Accessible resource link, displayed under user. Has only resource - camera or web page.
    sharedResource,
    // Generic resource. Has only resource.
    resource,
    // Layout item. Has both guid and resource.
    layoutItem,
    // EDGE server with a camera. Has only resource - server's only camera.
    edge,

    // Layout tour. Has a guid.
    layoutTour,

    // Videowall item. Has a guid and can have resource.
    videoWallItem,
    // Videowall saved matrix. Has a guid.
    videoWallMatrix,

    // Available cloud system.
    cloudSystem,
    // Local system but the current.
    localSystem,
};

inline uint qHash(ResourceTreeNodeType value, uint seed)
{
    return ::qHash(static_cast<int>(value), seed);
}

bool isSeparatorNode(ResourceTreeNodeType t);

} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::ResourceTreeNodeType)
