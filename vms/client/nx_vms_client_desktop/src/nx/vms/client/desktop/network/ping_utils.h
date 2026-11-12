// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

namespace nx::vms::client::desktop {

/**
 * @return Whether the host is safe to pass to the external ping utility: it consists only of the
 *     characters allowed in an IP address or DNS name and cannot be taken for an option. The
 *     address syntax itself is not validated.
 */
NX_VMS_CLIENT_DESKTOP_API bool isPingableHost(const QString& host);

} // namespace nx::vms::client::desktop
