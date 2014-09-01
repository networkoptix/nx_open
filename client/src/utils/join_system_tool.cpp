#include "join_system_tool.h"

#include <QtCore/QTimer>
#include <QtNetwork/QHostInfo>
#include <QtNetwork/QNetworkReply>

#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/user_resource.h"
#include "api/app_server_connection.h"
#include "nx_ec/dummy_handler.h"
#include "common/common_module.h"
#include "utils/network/module_finder.h"

namespace {

    const int checkTimeout = 60 * 1000;
    const int defaultRtspPort = 7001;

    ec2::AbstractECConnectionPtr connection2() {
        return QnAppServerConnectionFactory::getConnection2();
    }

} // anonymous namespace

QnJoinSystemTool::QnJoinSystemTool(QObject *parent) :
    QObject(parent),
    m_running(false),
    m_timer(new QTimer(this))
{
    m_timer->setInterval(checkTimeout);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &QnJoinSystemTool::at_timer_timeout);
}

bool QnJoinSystemTool::isRunning() const {
    return m_running;
}

void QnJoinSystemTool::start(const QUrl &url, const QString &password) {
    // we need only scheme, hostname and port from the url
    m_targetUrl.setScheme(url.scheme());
    m_targetUrl.setHost(url.host());
    m_targetUrl.setPort(url.port(defaultRtspPort));

    m_password = password;

    m_running = true;

    QHostAddress address(url.host());
    if (address.isNull()) {
        QHostInfo::lookupHost(url.host(), this, SLOT(at_hostLookedUp(QHostInfo)));
        return;
    }

    m_possibleAddresses.append(address);

    findResource();
}

void QnJoinSystemTool::findResource() {
    if (m_possibleAddresses.isEmpty()) {
        finish(HostLookupError);
        return;
    }

    /* For the first try to find it in the resource pool. */

    m_targetServer.clear();

    QSet<QHostAddress> addresses = QSet<QHostAddress>::fromList(m_possibleAddresses);
    QnMediaServerResourceList servers = qnResPool->getAllServers();
    foreach (const QnResourcePtr &resource, qnResPool->getAllIncompatibleResources()) {
        if (!resource->hasFlags(Qn::server))
            continue;
        servers.append(resource.staticCast<QnMediaServerResource>());
    }

    foreach (const QnMediaServerResourcePtr &server, servers) {
        if (QUrl(server->getApiUrl()).port() != m_targetUrl.port())
            continue;

        foreach (const QHostAddress &address, server->getNetAddrList()) {
            if (addresses.contains(address)) {
                m_targetServer = server;
                break;
            }
        }

        if (m_targetServer)
            break;
    }

    if (m_targetServer) {
        joinResource();
        return;
    }

    /* If there is no such server in the resource pool, try to discover it over the our network. */

    connect(qnResPool, &QnResourcePool::resourceAdded, this, &QnJoinSystemTool::at_resource_added);

    connection2()->getDiscoveryManager()->discoverPeer(m_targetUrl, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
    m_timer->start();
}

void QnJoinSystemTool::joinResource() {
    if (!isCompatible(m_targetServer->getVersion(), qnCommon->engineVersion())) {
        finish(VersionError);
        return;
    }

    if (m_targetServer->getSystemName() == qnCommon->localSystemName()) {
        rediscoverPeer();
        return;
    }

    QByteArray hash;
    QByteArray digest;
    foreach (const QnResourcePtr &resource, qnResPool->getResourcesWithFlag(Qn::user)) {
        QnUserResourcePtr user = resource.staticCast<QnUserResource>();
        if (user->getName() == lit("admin")) {
            hash = user->getHash();
            digest = user->getDigest();
            break;
        }
    }

    if (hash.isEmpty() || digest.isEmpty()) {
        finish(JoinError);
        return;
    }

    m_oldApiUrl = m_targetServer->apiConnection()->url();

    // temporary add the auth info to the api url
    QUrl apiUrl = m_oldApiUrl;
    apiUrl.setScheme(lit("https"));
    apiUrl.setUserName(lit("admin"));
    apiUrl.setPassword(m_password);
    m_targetServer->apiConnection()->setUrl(apiUrl);

    m_targetServer->apiConnection()->configureAsync(true, qnCommon->localSystemName(), QString(), hash, digest, 0, this, SLOT(at_targetServer_configured(int,int)));
}

void QnJoinSystemTool::rediscoverPeer() {
    if (m_targetServer->getStatus() == Qn::Online) {
        updateDiscoveryInformation();
        return;
    }

    connection2()->getDiscoveryManager()->discoverPeer(m_targetUrl, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);

    connect(qnResPool, &QnResourcePool::statusChanged, this, &QnJoinSystemTool::at_resource_statusChanged);
    m_timer->start();
}

void QnJoinSystemTool::updateDiscoveryInformation() {
    QHostAddress address(m_targetUrl.host());
    // there is no need to add manual address if it's already in the database
    if (!address.isNull() && m_targetServer->getNetAddrList().contains(address))
        return;

    connection2()->getDiscoveryManager()->addDiscoveryInformation(m_targetServer->getId(), QList<QUrl>() << m_targetUrl, false, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
    finish(NoError);
}

void QnJoinSystemTool::finish(int errorCode) {
    m_running = false;
    m_targetServer.clear();
    m_possibleAddresses.clear();
    m_targetUrl.clear();
    m_password.clear();
    emit finished(errorCode);
}

void QnJoinSystemTool::at_resource_added(const QnResourcePtr &resource) {
    if (!m_running)
        return;

    if (!resource->hasFlags(Qn::server))
        return;

    QnMediaServerResourcePtr server = resource.staticCast<QnMediaServerResource>();

    if (QUrl(server->getApiUrl()).port() != m_targetUrl.port())
        return;

    bool match = false;
    foreach (const QHostAddress &address, server->getNetAddrList()) {
        if (m_possibleAddresses.contains(address)) {
            match = true;
            break;
        }
    }
    if (!match)
        return;

    m_targetServer = server;
    m_timer->stop();
    joinResource();
}

void QnJoinSystemTool::at_resource_statusChanged(const QnResourcePtr &resource) {
    if (m_targetServer.isNull() || resource->getId() != m_targetServer->getId())
        return;

    if (resource->getStatus() == Qn::Online) {
        disconnect(qnResPool, &QnResourcePool::statusChanged, this, &QnJoinSystemTool::at_resource_statusChanged);
        m_timer->stop();
        updateDiscoveryInformation();
    }
}

void QnJoinSystemTool::at_timer_timeout() {
    if (!m_running)
        return;

    if (m_targetServer) {
        disconnect(qnResPool, &QnResourcePool::statusChanged, this, &QnJoinSystemTool::at_resource_statusChanged);
        finish(JoinError);
    } else {
        disconnect(qnResPool, &QnResourcePool::resourceAdded, this, &QnJoinSystemTool::at_resource_added);
        finish(Timeout);
    }
}

void QnJoinSystemTool::at_hostLookedUp(const QHostInfo &hostInfo) {
    if (hostInfo.error() != QHostInfo::NoError) {
        finish(HostLookupError);
        return;
    }

    m_possibleAddresses = hostInfo.addresses();
}

void QnJoinSystemTool::at_targetServer_configured(int status, int handle) {
    Q_UNUSED(handle)

    if (status != 0) {
        if (status == QNetworkReply::AuthenticationRequiredError)
            finish(AuthentificationError);
        else
            finish(JoinError);
        return;
    }

    // now revert to the default api url because the password has been reset to our own
    m_targetServer->apiConnection()->setUrl(m_oldApiUrl);

    rediscoverPeer();
}
