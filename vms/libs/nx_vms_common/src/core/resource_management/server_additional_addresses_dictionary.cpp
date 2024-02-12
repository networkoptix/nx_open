// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_additional_addresses_dictionary.h"

#include <nx/utils/log/assert.h>

QList<nx::utils::Url> QnServerAdditionalAddressesDictionary::additionalUrls(const nx::Uuid &serverId) const {
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_discoveryInfoById.value(serverId).additionalUrls;
}

QList<nx::utils::Url> QnServerAdditionalAddressesDictionary::ignoredUrls(const nx::Uuid &serverId) const {
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_discoveryInfoById.value(serverId).ignoredUrls;
}

void QnServerAdditionalAddressesDictionary::setAdditionalUrls(const nx::Uuid &serverId, const QList<nx::utils::Url> &additionalUrls) {
    NX_ASSERT(!serverId.isNull());
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_discoveryInfoById[serverId].additionalUrls = additionalUrls;
}

void QnServerAdditionalAddressesDictionary::setIgnoredUrls(const nx::Uuid &serverId, const QList<nx::utils::Url> &additionalUrls) {
    NX_ASSERT(!serverId.isNull());
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_discoveryInfoById[serverId].ignoredUrls = additionalUrls;
}

void QnServerAdditionalAddressesDictionary::clear() {
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_discoveryInfoById.clear();
}
