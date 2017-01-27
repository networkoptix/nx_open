#include "direct_module_finder_helper.h"

#include <QtCore/QTimer>

#include <api/global_settings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <nx/network/url/url_builder.h>

#include "module_finder.h"
#include "multicast_module_finder.h"
#include "direct_module_finder.h"

using namespace nx::network;

namespace {
    const int checkInterval = 3000;

    QUrl makeUrl(const QString& host, int port)
    {
        return url::Builder()
            .setScheme(lit("http"))
            .setHost(host)
            .setPort(port);
    }

    QUrl clearUrl(const QUrl& url)
    {
        return url::Builder()
            .setScheme(lit("http"))
            .setHost(url.host())
            .setPort(url.port());
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
} // namespace

QnDirectModuleFinderHelper::QnDirectModuleFinderHelper(QnModuleFinder *moduleFinder, bool clientMode) :
    base_type(moduleFinder),
    m_clientMode(clientMode),
    m_moduleFinder(moduleFinder)
{
    NX_ASSERT(moduleFinder);

    connect(qnResPool,  &QnResourcePool::resourceAdded,     this,   &QnDirectModuleFinderHelper::at_resourceAdded);
    connect(qnResPool,  &QnResourcePool::resourceRemoved,   this,   &QnDirectModuleFinderHelper::at_resourceRemoved);
    connect(qnResPool,  &QnResourcePool::resourceChanged,   this,   &QnDirectModuleFinderHelper::at_resourceChanged);
    connect(qnResPool,  &QnResourcePool::statusChanged,     this,   &QnDirectModuleFinderHelper::at_resourceStatusChanged);
    connect(moduleFinder->multicastModuleFinder(), &QnMulticastModuleFinder::responseReceived, this, &QnDirectModuleFinderHelper::at_responseReceived);

    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &QnDirectModuleFinderHelper::at_timer_timeout);
    timer->start(checkInterval);

    m_elapsedTimer.start();
}

void QnDirectModuleFinderHelper::addForcedUrl(QObject* requester, const QUrl& url)
{
    NX_ASSERT(requester);
    if (!requester)
        return;

    auto& urls = m_forcedUrlsByRequester[requester];

    if (urls.isEmpty())
    {
        connect(requester, &QObject::destroyed,
            this, &QnDirectModuleFinderHelper::removeForcedUrls);
    }

    const auto cleanUrl = clearUrl(url);
    if (urls.contains(cleanUrl))
        return;

    urls.insert(cleanUrl);
    mergeForcedUrls();
}

void QnDirectModuleFinderHelper::removeForcedUrl(QObject* requester, const QUrl& url)
{
    NX_ASSERT(requester);
    if (!requester)
        return;

    auto it = m_forcedUrlsByRequester.find(requester);
    if (it == m_forcedUrlsByRequester.end())
        return;

    if (!it->remove(clearUrl(url)))
        return;

    if (it->isEmpty())
        removeForcedUrls(requester);

    mergeForcedUrls();
}

void QnDirectModuleFinderHelper::setForcedUrls(QObject* requester, const QSet<QUrl>& forcedUrls)
{
    m_forcedUrlsByRequester[requester] = forcedUrls;
    mergeForcedUrls();
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

    QnUrlSet urls;
    QnUrlSet additionalUrls;
    QnUrlSet ignoredUrls;

    if (auto cloudAddress = server->getCloudAddress()) {
        QUrl url = makeUrl(cloudAddress->address.toString(), cloudAddress->port);
        urls.insert(url);
        addUrl(url, m_urls);
    }

    for (const auto &address: server->getNetAddrList()) {
        QUrl url = makeUrl(address.address.toString(), address.port);
        urls.insert(url);
        addUrl(url, m_urls);
    }

    for (const QUrl &url: server->getAdditionalUrls()) {
        QUrl turl = makeUrl(url.host(), url.port(port));
        additionalUrls.insert(turl);
        addUrl(turl, m_urls);
    }

    if (!m_clientMode) {
        for (const QUrl &url: server->getIgnoredUrls()) {
            QUrl turl = makeUrl(url.host(), url.port(port));
            ignoredUrls.insert(turl);
            addUrl(turl, m_ignoredUrls);
        }
    }

    m_serverUrlsById[serverId] = urls;
    m_additionalServerUrlsById[serverId] = additionalUrls;
    m_ignoredServerUrlsById[serverId] = ignoredUrls;

    updateModuleFinder();
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

    if (!m_clientMode) {
        for (const QUrl &url: m_ignoredServerUrlsById.take(serverId))
            removeUrl(url, m_ignoredUrls);
    }

    updateModuleFinder();
}

void QnDirectModuleFinderHelper::at_resourceChanged(const QnResourcePtr &resource) {
    at_resourceRemoved(resource);
    at_resourceAdded(resource);
}

void QnDirectModuleFinderHelper::at_resourceAuxUrlsChanged(const QnResourcePtr &resource) {
    at_resourceRemoved(resource);
    at_resourceAdded(resource);
}

void QnDirectModuleFinderHelper::at_resourceStatusChanged(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    if (server->getStatus() == Qn::Online) {
        QnUuid id = server->getId();

        QnUrlSet urlsToCheck = m_serverUrlsById.value(id) + m_additionalServerUrlsById.value(id) - m_ignoredServerUrlsById.value(id);
        for (const QUrl &url: urlsToCheck)
            m_moduleFinder->directModuleFinder()->checkUrl(url);
    }
}

void QnDirectModuleFinderHelper::at_responseReceived(const QnModuleInformation &moduleInformation, const SocketAddress &address) {
    Q_UNUSED(moduleInformation)
    m_multicastedUrlLastPing[makeUrl(address.address.toString(), address.port)] = m_elapsedTimer.elapsed();
}

void QnDirectModuleFinderHelper::at_timer_timeout() {
    using namespace std::chrono;

    const int pingTimeout = duration_cast<milliseconds>(m_moduleFinder->pingTimeout()).count();
    qint64 currentTime = m_elapsedTimer.elapsed();

    for (auto it = m_multicastedUrlLastPing.begin(); it != m_multicastedUrlLastPing.end(); /* no inc */) {
        if (*it + pingTimeout < currentTime)
            it = m_multicastedUrlLastPing.erase(it);
        else
            ++it;
    }
    updateModuleFinder();
}

void QnDirectModuleFinderHelper::updateModuleFinder() {
    QnUrlSet urls = QnUrlSet::fromList(m_urls.keys());
    QnUrlSet ignored = QnUrlSet::fromList(m_ignoredUrls.keys());
    QnUrlSet alive = QnUrlSet::fromList(m_multicastedUrlLastPing.keys());
    m_moduleFinder->directModuleFinder()->setUrls((urls + m_forcedUrls) - ignored - alive);
}

void QnDirectModuleFinderHelper::mergeForcedUrls()
{
    m_forcedUrls.clear();
    for (const auto& urls: m_forcedUrlsByRequester)
        m_forcedUrls.unite(urls);

    updateModuleFinder();
}

void QnDirectModuleFinderHelper::removeForcedUrls(QObject* requester)
{
    m_forcedUrlsByRequester.remove(requester);
    disconnect(requester, &QObject::destroyed,
        this, &QnDirectModuleFinderHelper::removeForcedUrls);
}
