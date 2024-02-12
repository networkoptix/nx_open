// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <set>

#include <QtCore/QString>
#include <QtCore/QUrl>

#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

/**
 * A set of approved urls that have access to a session token using jsapi for each system and user
 * separately.
 */
struct AuthAllowedUrls
{
    using Key = QString;
    using Urls = std::set<QString>;

    std::map<Key, Urls> items;

    bool operator==(const AuthAllowedUrls&) const = default;

    static Key key(const nx::Uuid& systemId, const QString& username)
    {
        return systemId.toString() + username;
    }
};

NX_REFLECTION_INSTRUMENT(AuthAllowedUrls, (items))

} // namespace nx::vms::client::desktop
