#ifndef DIRECT_MODULE_FINDER_HELPER_H
#define DIRECT_MODULE_FINDER_HELPER_H

#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QElapsedTimer>

#include <core/resource/resource_fwd.h>
#include <utils/common/id.h>
#include <utils/common/connective.h>

typedef QSet<QUrl> QnUrlSet;

class QnModuleFinder;
class QnMulticastModuleFinder;
class SocketAddress;
struct QnModuleInformation;

class QnDirectModuleFinderHelper: public Connective<QObject>
{
    Q_OBJECT
    typedef Connective<QObject> base_type;

public:
    explicit QnDirectModuleFinderHelper(QnModuleFinder *moduleFinder, bool clientMode = false);

    void addForcedUrl(QObject* requester, const QUrl& url);
    void removeForcedUrl(QObject* requester, const QUrl& url);
    void setForcedUrls(QObject* requester, const QSet<QUrl>& forcedUrls);

private slots:
    void at_resourceAdded(const QnResourcePtr &resource);
    void at_resourceRemoved(const QnResourcePtr &resource);
    void at_resourceChanged(const QnResourcePtr &resource);
    void at_resourceAuxUrlsChanged(const QnResourcePtr &resource);
    void at_resourceStatusChanged(const QnResourcePtr &resource);
    void at_responseReceived(const QnModuleInformation &moduleInformation, const SocketAddress &address);
    void at_timer_timeout();

private:
    void updateModuleFinder();
    void mergeForcedUrls();
    void removeForcedUrls(QObject* requester);

private:
    /** When the helper works in client mode it does not use ignoredUrls from database. */
    bool m_clientMode;

    QnModuleFinder *m_moduleFinder;
    QElapsedTimer m_elapsedTimer;

    QHash<QUrl, int> m_urls;
    QHash<QUrl, int> m_ignoredUrls;
    QSet<QUrl> m_forcedUrls;
    QHash<QObject*, QSet<QUrl>> m_forcedUrlsByRequester;

    QHash<QnUuid, QnUrlSet> m_serverUrlsById;
    QHash<QnUuid, QnUrlSet> m_additionalServerUrlsById;
    QHash<QnUuid, QnUrlSet> m_ignoredServerUrlsById;

    QHash<QUrl, qint64> m_multicastedUrlLastPing;
};

#endif // DIRECT_MODULE_FINDER_HELPER_H
