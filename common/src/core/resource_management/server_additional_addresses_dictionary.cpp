#include "server_additional_addresses_dictionary.h"

#include <nx/utils/log/assert.h>

QnServerAdditionalAddressesDictionary::QnServerAdditionalAddressesDictionary(QObject *parent):
        QObject(parent)
{
}

QList<QUrl> QnServerAdditionalAddressesDictionary::additionalUrls(const QnUuid &serverId) const {
    QnMutexLocker lock(&m_mutex);
    return m_discoveryInfoById.value(serverId).additionalUrls;
}

QList<QUrl> QnServerAdditionalAddressesDictionary::ignoredUrls(const QnUuid &serverId) const {
    QnMutexLocker lock(&m_mutex);
    return m_discoveryInfoById.value(serverId).ignoredUrls;
}

void QnServerAdditionalAddressesDictionary::setAdditionalUrls(const QnUuid &serverId, const QList<QUrl> &additionalUrls) {
    NX_ASSERT(!serverId.isNull());
    QnMutexLocker lock(&m_mutex);
    m_discoveryInfoById[serverId].additionalUrls = additionalUrls;
}

void QnServerAdditionalAddressesDictionary::setIgnoredUrls(const QnUuid &serverId, const QList<QUrl> &additionalUrls) {
    NX_ASSERT(!serverId.isNull());
    QnMutexLocker lock(&m_mutex);
    m_discoveryInfoById[serverId].ignoredUrls = additionalUrls;
}

void QnServerAdditionalAddressesDictionary::clear() {
    QnMutexLocker lock(&m_mutex);
    m_discoveryInfoById.clear();
}
