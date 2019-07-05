#pragma once

#include <QtCore/QMetatype>

#include <nx/utils/url.h>

namespace nx::vms::client::desktop {

struct LogonParameters
{
    /** Target server url. */
    nx::utils::Url url;

    /** The session should be stored into recent list and as a tile. */
    bool storeSession = true;

    /** Password should be stored. */
    bool storePassword = false;

    /** The client should automatically login in to this System on startup. */
    bool autoLogin = false;

    // TODO: Get rid of this parameter. It means almost nothing now.
    bool force = false;

    LogonParameters() = default;
    LogonParameters(const nx::utils::Url& url): url(url) { }
};

Q_DECLARE_METATYPE(LogonParameters)

} // namespace nx::vms::client::desktop
