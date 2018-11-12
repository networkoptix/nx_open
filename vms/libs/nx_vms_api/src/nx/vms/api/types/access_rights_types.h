#pragma once

#include <nx/vms/api/types_fwd.h>

namespace nx {
namespace vms {
namespace api {

/**
 * Flags describing global user capabilities, independently of resources. Stored in the database.
 * QFlags uses int internally, so we are limited to 32 bits.
 */
enum class GlobalPermission
{
    /* Generic permissions. */
    none = 0, /**< Only live video access. */

    /* Admin permissions. */
    admin = 0x00000001, /**< Admin, can edit other non-admins. */

    /* Manager permissions. */
    editCameras = 0x00000002, /**< Can edit camera settings. */
    controlVideowall = 0x00000004, /**< Can control videowalls. */

    viewLogs = 0x00000010, /**< Can access event log and audit trail. */

    /* Viewer permissions. */
    viewArchive = 0x00000100, /**< Can view archives of available cameras. */
    exportArchive = 0x00000200, /**< Can export archives of available cameras. */
    viewBookmarks = 0x00000400, /**< Can view bookmarks of available cameras. */
    manageBookmarks = 0x00000800, /**< Can modify bookmarks of available cameras. */

    /* Input permissions. */
    userInput = 0x00010000, /**< Can change camera's PTZ state, use 2-way audio, I/O buttons. */

    /* Resources access permissions. */
    accessAllMedia = 0x01000000, /**< Has access to all media resources (cameras and web pages). */

    /* Other permissions. */
    customUser = 0x10000000, /**< Flag that just mark new user as 'custom'. */

    /* Combinations. */

    /* Live viewer has access to all cameras and global layouts by default. */
    liveViewerPermissions = accessAllMedia,

    /* Viewer can additionally view archive and bookmarks and export video. */
    viewerPermissions = liveViewerPermissions | viewArchive | exportArchive | viewBookmarks,

    /* Advanced viewer can manage bookmarks and use various input methods. */
    advancedViewerPermissions = viewerPermissions | manageBookmarks | userInput | viewLogs,

    /* Admin can do everything. */
    adminPermissions = admin | advancedViewerPermissions | controlVideowall | editCameras,

    /* PTZ here is intended - for SpaceX, see VMS-2208 */
    videowallModePermissions = liveViewerPermissions | viewArchive | userInput
        | controlVideowall | viewBookmarks,

    /* Actions in ACS mode are limited. */
    acsModePermissions = viewerPermissions | userInput
};

Q_DECLARE_FLAGS(GlobalPermissions, GlobalPermission)
Q_DECLARE_OPERATORS_FOR_FLAGS(GlobalPermissions)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(GlobalPermission)

} // namespace api
} // namespace vms
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::GlobalPermission, (metatype)(lexical)(debug), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::GlobalPermissions, (metatype)(lexical)(debug), NX_VMS_API)
