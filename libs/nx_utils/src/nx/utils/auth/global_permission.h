// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QFlags>

#include <nx/reflect/enum_instrument.h>

namespace nx::utils::auth {

/**%apidoc
 * Flags describing global User capabilities, independently of Resources.
 * %// Stored in the database. QFlags uses int internally, so we are limited to 32 bits.
 */
enum class GlobalPermission
{
    /**%apidoc
     * No global permissions.
     */
    none = 0,

    /* Administrative permissions. */

    /**%apidoc[unused]
     * Administrator permissions: full control of the VMS Site.
     * Not directly assignable, only inherited from the Administrators predefined group.
     */
    administrator = 0x20000000,

    /**%apidoc[unused]
     * Management of devices and non-power users.
     * Not directly assignable, only inherited from the Power Users predefined group.
     */
    powerUser = 0x00000001,

    /**%apidoc
     * Can access Event Log and Audit Trail.
     */
    viewLogs = 0x00000010,

    /**%apidoc
     * Can access Metrics.
     */
    viewMetrics = 0x00000020,

    /**%apidoc
     * Can generate VMS Events.
     */
    generateEvents = 0x00020000,

    /**%apidoc
     * Can view video without redaction.
     */
    viewUnredactedVideo = 0x00040000,

    /**%apidoc[unused] */
    requireFreshSession = 0x40000000,

    /**%apidoc[unused] */
    powerUserWithFreshSession = powerUser | requireFreshSession,

    /**%apidoc[unused] */
    administratorWithFreshSession = administrator | requireFreshSession,
};
NX_REFLECTION_INSTRUMENT_ENUM(GlobalPermission,
    none,
    administrator,
    powerUser,
    viewLogs,
    viewMetrics,
    generateEvents,
    viewUnredactedVideo)

Q_DECLARE_FLAGS(GlobalPermissions, GlobalPermission)
Q_DECLARE_OPERATORS_FOR_FLAGS(GlobalPermissions)

} // namespace nx::utils::auth
