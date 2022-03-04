// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>
#include <nx/utils/url.h>

class NX_VMS_COMMON_API QnServerAdditionalAddressesDictionary
{
public:
    QList<nx::utils::Url> additionalUrls(const QnUuid &serverId) const;
    QList<nx::utils::Url> ignoredUrls(const QnUuid &serverId) const;
    void setAdditionalUrls(const QnUuid &serverId, const QList<nx::utils::Url> &additionalUrls);
    void setIgnoredUrls(const QnUuid &serverId, const QList<nx::utils::Url> &additionalUrls);
    void clear();

private:
    struct DiscoveryInfo {
        QList<nx::utils::Url> additionalUrls;
        QList<nx::utils::Url> ignoredUrls;
    };

    QMap<QnUuid, DiscoveryInfo> m_discoveryInfoById;
    mutable nx::Mutex m_mutex;
};
