// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/thread/mutex.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>

class NX_VMS_COMMON_API QnServerAdditionalAddressesDictionary
{
public:
    QList<nx::Url> additionalUrls(const nx::Uuid& serverId) const;
    QList<nx::Url> ignoredUrls(const nx::Uuid& serverId) const;
    void setAdditionalUrls(const nx::Uuid& serverId, const QList<nx::Url>& additionalUrls);
    void setIgnoredUrls(const nx::Uuid& serverId, const QList<nx::Url>& additionalUrls);
    void clear();

private:
    struct DiscoveryInfo {
        QList<nx::Url> additionalUrls;
        QList<nx::Url> ignoredUrls;
    };

    QMap<nx::Uuid, DiscoveryInfo> m_discoveryInfoById;
    mutable nx::Mutex m_mutex;
};
