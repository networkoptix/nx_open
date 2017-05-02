#pragma once

#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>

class QnServerAdditionalAddressesDictionary: public QObject
{
    Q_OBJECT
public:
    QnServerAdditionalAddressesDictionary(QObject *parent = NULL);

    QList<QUrl> additionalUrls(const QnUuid &serverId) const;
    QList<QUrl> ignoredUrls(const QnUuid &serverId) const;
    void setAdditionalUrls(const QnUuid &serverId, const QList<QUrl> &additionalUrls);
    void setIgnoredUrls(const QnUuid &serverId, const QList<QUrl> &additionalUrls);
    void clear();

private:
    struct DiscoveryInfo {
        QList<QUrl> additionalUrls;
        QList<QUrl> ignoredUrls;
    };

    QMap<QnUuid, DiscoveryInfo> m_discoveryInfoById;
    mutable QnMutex m_mutex;
};
