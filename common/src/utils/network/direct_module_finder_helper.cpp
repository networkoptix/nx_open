#include "direct_module_finder_helper.h"

#include <QtCore/QTimer>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <utils/network/module_finder.h>
#include <utils/network/multicast_module_finder.h>
#include <utils/network/direct_module_finder.h>

namespace {
    const int checkInterval = 3000;

    QUrl makeUrl(const QString &host, int port) {
        QUrl url;
        url.setScheme(lit("http"));
        url.setHost(host);
        url.setPort(port);
        return url;
    }

    void addUrl(const QUrl &url, QHash<QUrl, int> &hash) {
        hash[url]++;
    }

    void removeUrl(const QUrl &url, QHash<QUrl, int> &hash) {
        auto it = hash.find(url);
        if (it == hash.end())
            return;

        int &count = *it;
        if (--count == 0)
            hash.erase(it);
    }
}

QnDirectModuleFinderHelper::QnDirectModuleFinderHelper(QnModuleFinder *moduleFinder) :
    base_type(moduleFinder),
    m_moduleFinder(moduleFinder)
{
    Q_ASSERT(moduleFinder);

    connect(qnResPool,  &QnResourcePool::resourceAdded,     this,   &QnDirectModuleFinderHelper::at_resourceAdded);
    connect(qnResPool,  &QnResourcePool::resourceRemoved,   this,   &QnDirectModuleFinderHelper::at_resourceRemoved);
    connect(qnResPool,  &QnResourcePool::resourceChanged,   this,   &QnDirectModuleFinderHelper::at_resourceChanged);
    connect(moduleFinder->multicastModuleFinder(), &QnMulticastModuleFinder::responseReceived, this, &QnDirectModuleFinderHelper::at_responseReceived);

    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &QnDirectModuleFinderHelper::at_timer_timeout);
    timer->start(checkInterval);

    m_elapsedTimer.start();
}

void QnDirectModuleFinderHelper::at_resourceAdded(const QnResourcePtr &resource) {
    if (resource->getId() == qnCommon->moduleGUID())
        return;

    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    connect(server, &QnMediaServerResource::auxUrlsChanged, this, &QnDirectModuleFinderHelper::at_resourceAuxUrlsChanged);

    QnUuid serverId = server->getId();
    quint16 port = server->getPort();

    QnUrlSet urls = m_serverUrlsById[serverId];
    QnUrlSet additionalUrls = m_additionalServerUrlsById[serverId];
    QnUrlSet ignoredUrls = m_ignoredServerUrlsById[serverId];

    for (const QHostAddress &address: server->getNetAddrList()) {
        QUrl url = makeUrl(address.toString(), port);
        urls.insert(url);
        addUrl(url, m_urls);
    }

    for (const QUrl &url: server->getAdditionalUrls()) {
        QUrl turl = makeUrl(url.host(), port);
        additionalUrls.insert(turl);
        addUrl(turl, m_urls);
    }

    for (const QUrl &url: server->getIgnoredUrls()) {
        QUrl turl = makeUrl(url.host(), port);
        ignoredUrls.insert(turl);
        addUrl(turl, m_ignoredUrls);
    }

    m_serverUrlsById[serverId] = urls;
    m_additionalServerUrlsById[serverId] = additionalUrls;
    m_ignoredServerUrlsById[serverId] = ignoredUrls;
}

void QnDirectModuleFinderHelper::at_resourceRemoved(const QnResourcePtr &resource) {
    if (resource->getId() == qnCommon->moduleGUID())
        return;

    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    disconnect(server, &QnMediaServerResource::auxUrlsChanged, this, &QnDirectModuleFinderHelper::at_resourceAuxUrlsChanged);

    QnUuid serverId = server->getId();

    for (const QUrl &url: m_serverUrlsById.take(serverId))
        removeUrl(url, m_urls);

    for (const QUrl &url: m_additionalServerUrlsById.take(serverId))
        removeUrl(url, m_urls);

    for (const QUrl &url: m_ignoredServerUrlsById.take(serverId))
        removeUrl(url, m_ignoredUrls);
}

void QnDirectModuleFinderHelper::at_resourceChanged(const QnResourcePtr &resource) {
    at_resourceRemoved(resource);
    at_resourceAdded(resource);
}

void QnDirectModuleFinderHelper::at_resourceAuxUrlsChanged(const QnResourcePtr &resource) {
    at_resourceRemoved(resource);
    at_resourceAdded(resource);
}

void QnDirectModuleFinderHelper::at_responseReceived(const QnModuleInformation &moduleInformation, const QUrl &url) {
    Q_UNUSED(moduleInformation)
    m_multicastedUrlLastPing[url] = m_elapsedTimer.elapsed();
}

void QnDirectModuleFinderHelper::at_timer_timeout() {
    int pingTimeout = m_moduleFinder->pingTimeout();
    qint64 currentTime = m_elapsedTimer.elapsed();

    for (auto it = m_multicastedUrlLastPing.begin(); it != m_multicastedUrlLastPing.end(); /* no inc */) {
        if (*it + pingTimeout < currentTime)
            it = m_multicastedUrlLastPing.erase(it);
        else
            ++it;
    }
}

void QnDirectModuleFinderHelper::updateModuleFinder() {
    QnUrlSet urls = QnUrlSet::fromList(m_urls.keys());
    QnUrlSet ignored = QnUrlSet::fromList(m_ignoredUrls.keys());
    QnUrlSet alive = QnUrlSet::fromList(m_multicastedUrlLastPing.keys());
    m_moduleFinder->directModuleFinder()->setUrls(urls - ignored - alive);
}
