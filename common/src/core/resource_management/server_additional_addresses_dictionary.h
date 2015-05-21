#ifndef QNSERVERADDITIONALADDRESSESDICTIONARY_H
#define QNSERVERADDITIONALADDRESSESDICTIONARY_H

#include <utils/common/singleton.h>


class QnServerAdditionalAddressesDictionary: public QObject, public Singleton<QnServerAdditionalAddressesDictionary>
{
    Q_OBJECT
public:
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
    mutable QMutex m_mutex;
};

#endif // QNSERVERADDITIONALADDRESSESDICTIONARY_H
