#include "server_additional_addresses_dictionary.h"

QList<QUrl> QnServerAdditionalAddressesDictionary::additionalUrls(const QnUuid &serverId) const {
    QMutexLocker lock(&m_mutex);
    return m_discoveryInfoById.value(serverId).additionalUrls;
}

QList<QUrl> QnServerAdditionalAddressesDictionary::ignoredUrls(const QnUuid &serverId) const {
    QMutexLocker lock(&m_mutex);
    return m_discoveryInfoById.value(serverId).ignoredUrls;
}

void QnServerAdditionalAddressesDictionary::setAdditionalUrls(const QnUuid &serverId, const QList<QUrl> &additionalUrls) {
    Q_ASSERT(!serverId.isNull());
    QMutexLocker lock(&m_mutex);
    m_discoveryInfoById[serverId].additionalUrls = additionalUrls;
}

void QnServerAdditionalAddressesDictionary::setIgnoredUrls(const QnUuid &serverId, const QList<QUrl> &additionalUrls) {
    Q_ASSERT(!serverId.isNull());
    QMutexLocker lock(&m_mutex);
    m_discoveryInfoById[serverId].ignoredUrls = additionalUrls;
}

void QnServerAdditionalAddressesDictionary::clear() {
    QMutexLocker lock(&m_mutex);
    m_discoveryInfoById.clear();
}
