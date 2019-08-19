#pragma once

#include <QtCore/QMetaType>

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

    /** The client was run as a secondary instance - using "Open in New Window" action. */
    bool secondaryInstance = false;

    LogonParameters() = default;
    LogonParameters(const nx::utils::Url& url): url(url) { }
};

} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::LogonParameters)
