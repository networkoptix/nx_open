// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>

#include <QtCore/QString>
#include <QtCore/QUrl>

#include <nx/reflect/instrument.h>

namespace nx::vms::client::desktop {

/**
 * A set of approved hostnames that have access to a session token using jsapi for each system and
 * user separately. For cloud users only username is a key.
 */
struct AuthAllowedUrls
{
    using Origin = QString;

    std::map<QString, std::set<Origin>> items;

    bool operator==(const AuthAllowedUrls&) const = default;

    static Origin origin(const QUrl& url)
    {
        return url.toString(
            QUrl::RemoveUserInfo
            | QUrl::RemovePath
            | QUrl::RemoveQuery
            | QUrl::RemoveFragment);
    }
};

NX_REFLECTION_INSTRUMENT(AuthAllowedUrls, (items))

} // namespace nx::vms::client::desktop
