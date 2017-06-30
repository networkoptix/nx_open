#include "reconnect_helper.h"

#include <QtNetwork/QHostAddress>
#include <QtNetwork/QHostInfo>

#include <api/app_server_connection.h>

#include <boost/range/algorithm/find_if.hpp>
#include <boost/range/algorithm/max_element.hpp>

#include <common/common_module.h>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/resource_display_info.h>

#include <client/client_settings.h>

#include <nx/utils/collection.h>
#include <nx/utils/string.h>
#include <nx/vms/discovery/manager.h>
#include <api/global_settings.h>
#include <network/system_helpers.h>

namespace {

#ifdef _DEBUG
//#define QN_RECONNECT_HELPER_LOG
#endif

    void printLog(const QByteArray &message, const QnMediaServerResourcePtr &server = QnMediaServerResourcePtr())
    {
#ifdef QN_RECONNECT_HELPER_LOG
        const QByteArray prefix("QnReconnectHelper::");
        if (server)
            qDebug() << prefix << message << QnResourceDisplayInfo(server).toString(Qn::RI_WithUrl);
        else
            qDebug() << prefix << message;
#else
        Q_UNUSED(message);
        Q_UNUSED(server);
#endif
    }

}

QnReconnectHelper::InterfaceInfo::InterfaceInfo():
    online(false),
    ignored(false),
    count(0)
{}

QnReconnectHelper::QnReconnectHelper(QObject *parent /* = NULL*/):
    QObject(parent),
    m_currentServer(commonModule()->currentServer()),
    m_currentUrl(commonModule()->currentUrl())
{
    m_userName = m_currentUrl.userName();
    m_password = m_currentUrl.password();

    /* List of all known servers. Should not be updated as we are disconnected. */
    m_allServers = resourcePool()->getResources<QnMediaServerResource>();
    if (m_currentServer && !m_allServers.contains(m_currentServer))
        m_allServers << m_currentServer;

    auto serverName = [this](const QnMediaServerResourcePtr& server)
    {
        return QnResourceDisplayInfo(server).toString(qnSettings->extraInfoInTree());
    };

    std::sort(m_allServers.begin(), m_allServers.end(), [serverName](const QnMediaServerResourcePtr &left, const QnMediaServerResourcePtr &right)
    {
        return nx::utils::naturalStringLess(serverName(left), serverName(right));
    });

    printLog("Full server list:");
    for (const QnMediaServerResourcePtr &server: m_allServers)
        printLog(QByteArray(), server);

    if (!m_currentServer && !m_allServers.isEmpty())
        m_currentServer = m_allServers.first();

    /* There are no servers in the system. */
    if (!m_currentServer)
        return;

    printLog("Starting with server", m_currentServer);

    for (const QnMediaServerResourcePtr &server: m_allServers) {
        QList<InterfaceInfo> interfaces;

        int port = server->getPort();
        QUrl defaultUrl = server->getApiUrl();
        defaultUrl.setScheme(lit("http"));
        defaultUrl.setPort(port);
        defaultUrl.setUserName(m_userName);
        defaultUrl.setPassword(m_password);

        for (const auto &addr: server->getNetAddrList()) {
            InterfaceInfo info;
            info.url = defaultUrl;
            info.url.setHost(addr.address.toString());
            info.url.setPort(addr.port);
            addInterfaceIfNotExists(interfaces, info);
        }

        for (const QUrl &url: server->getAdditionalUrls()) {
            InterfaceInfo info;
            info.url = url;
            addInterfaceIfNotExists(interfaces, info);
        }

        for (const QUrl &url: server->getIgnoredUrls()) {
            InterfaceInfo info;
            info.url = url;
            info.ignored = true;
            replaceInterface(interfaces, info);
        }

        m_interfacesByServer[server->getId()] = interfaces;

        printLog("Update interfaces for", server);
        updateInterfacesForServer(server->getId());
    }
}


QnMediaServerResourceList QnReconnectHelper::servers() const {
    return m_allServers;
}

QnMediaServerResourcePtr QnReconnectHelper::currentServer() const {
    return m_currentServer;
}

QUrl QnReconnectHelper::currentUrl() const {
    return m_currentUrl;
}

void QnReconnectHelper::next() {
    if (m_allServers.isEmpty())
        return;

    int idx = m_allServers.indexOf(m_currentServer);
    ++idx;
    /* Loop servers. */
    if (idx >= m_allServers.size())
        idx = 0;

    m_currentServer = m_allServers[idx];
    QnUuid id = m_currentServer->getId();
    printLog("Trying to connect to", m_currentServer);

    updateInterfacesForServer(id);
    m_currentUrl = bestInterfaceForServer(id);
}

void QnReconnectHelper::markServerAsInvalid(const QnMediaServerResourcePtr &server) {
    if (!m_allServers.contains(server))
        return;

    printLog("Server is incompatible", server);

    QList<InterfaceInfo> &interfaces = m_interfacesByServer[server->getId()];
    for (InterfaceInfo &item: interfaces)
        item.ignored = true;
}

void QnReconnectHelper::updateInterfacesForServer(const QnUuid &id)
{
    QList<InterfaceInfo> &interfaces = m_interfacesByServer[id];
    for (InterfaceInfo &item: interfaces)
        item.online = false;

    auto modules = commonModule()->moduleDiscoveryManager()->getAll();
    auto iter = boost::find_if(modules, [id](const QnModuleInformation &info){return info.id == id;});
    if (iter == boost::end(modules))
        return;

    if (!helpers::serverBelongsToCurrentSystem(*iter, commonModule()))
    {
        printLog("Server has another system ID: " + iter->localSystemId.toByteArray());
        for (InterfaceInfo &item: interfaces)
            item.ignored = true;
        return;
    }

    const auto remoteAddress = commonModule()->moduleDiscoveryManager()->getEndpoint(iter->id);
    bool found = false;
    auto sameUrl = [remoteAddress](const QUrl &url) -> bool {
        if (!remoteAddress)
            return false;

        return url.port() == remoteAddress->port
            && url.host() == remoteAddress->address.toString();
    };

    for (InterfaceInfo &item: interfaces) {
        if (!sameUrl(item.url))
            continue;
        item.online = true;
        found = true;
        break;
    }

    if (!found && remoteAddress) {
        InterfaceInfo info;
        info.online = true;
        info.url.setScheme(lit("http"));
        info.url.setHost(remoteAddress->address.toString());
        info.url.setPort(remoteAddress->port);
        info.url.setUserName(m_userName);
        info.url.setPassword(m_password);
        interfaces << info;
    }
}

QUrl QnReconnectHelper::bestInterfaceForServer(const QnUuid &id) {

    auto calculateItemPriority = [](const InterfaceInfo &info) {
        const int base = 1000;
        const int onlineBase = base * 2;
        if (info.ignored)
            return -1;
        return info.online
            ? onlineBase - info.count
            : base - info.count;
    };

    QList<InterfaceInfo> &interfaces = m_interfacesByServer[id];

    auto iter = boost::max_element(interfaces, [calculateItemPriority](const InterfaceInfo &left, const InterfaceInfo &right) {
        return calculateItemPriority(left) < calculateItemPriority(right);
    });

    NX_ASSERT(iter != boost::end(interfaces));
    if(iter == boost::end(interfaces))
        return QUrl();

    if (iter->ignored) {
        printLog("All server interfaces are ignored");
        return QUrl();
    }

    (*iter).count++;

    QUrl urlNoPassword(iter->url);
    urlNoPassword.setPassword(QString());
    printLog(lit("Trying %1 (%2-th time)").arg(urlNoPassword.toString()).arg(iter->count).toUtf8());
    return iter->url;
}


void QnReconnectHelper::addInterfaceIfNotExists(QList<InterfaceInfo> &interfaces, const InterfaceInfo &info) const {
    auto sameUrl = [info](const InterfaceInfo &left) {
        return left.url.host() == info.url.host() && left.url.port() == info.url.port();
    };
    if (boost::find_if(interfaces, sameUrl) != boost::end(interfaces))
        return;

    interfaces << info;
}

void QnReconnectHelper::replaceInterface(QList<InterfaceInfo> &interfaces, const InterfaceInfo &info) const {
    auto sameUrl = [info](const InterfaceInfo &left) {
        return left.url.host() == info.url.host() && left.url.port() == info.url.port();
    };
    auto iter = boost::find_if(interfaces, sameUrl);
    if (iter == boost::end(interfaces)) {
       interfaces << info;
       return;
    };
    *iter = info;
}

