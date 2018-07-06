#pragma once

#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>
#include <nx/utils/url.h>

class QnServerAdditionalAddressesDictionary: public QObject
{
    Q_OBJECT
public:
    QnServerAdditionalAddressesDictionary(QObject *parent = NULL);

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
    mutable QnMutex m_mutex;
};
