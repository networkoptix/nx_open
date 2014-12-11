#include "configure_peer_task.h"

#include <QtNetwork/QNetworkReply>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <utils/network/global_module_finder.h>
#include <utils/network/router.h>
#include <api/model/configure_reply.h>
#include <utils/merge_systems_tool.h>
#include <common/common_module.h>

QnConfigurePeerTask::QnConfigurePeerTask(QObject *parent) :
    QnNetworkPeerTask(parent),
    m_mergeTool(new QnMergeSystemsTool(this))
{
    connect(m_mergeTool, &QnMergeSystemsTool::mergeFinished, this, &QnConfigurePeerTask::at_mergeTool_mergeFinished);
}

QString QnConfigurePeerTask::user() const {
    return m_user;
}

void QnConfigurePeerTask::setUser(const QString &user) {
    m_user = user;
}

QString QnConfigurePeerTask::password() const {
    return m_password;
}

void QnConfigurePeerTask::setPassword(const QString &password) {
    m_password = password;
}

void QnConfigurePeerTask::doStart() {
    m_error = NoError;
    m_pendingPeers.clear();

    QnRouter *router = QnRouter::instance();
    if (!router) {
        finish(UnknownError, m_failedPeers);
        return;
    }

    foreach (const QnUuid &id, peers()) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id, true).dynamicCast<QnMediaServerResource>();
        if (!server) {
            m_failedPeers.insert(id);
            continue;
        }

        QnUuid realId = QnUuid::fromStringSafe(server->getProperty(lit("guid")));
        if (realId.isNull())
            realId = id;

        /* Find the server to call marge and URL of the peer to be merged.
           We need to guarantee the route will go through a server from our system.
           For that we build route via our EC */
        QnRoute route = router->routeTo(realId, qnCommon->remoteGUID());
        if (!route.isValid()) {
            m_failedPeers.insert(id);
            continue;
        }
        /* Route will always has at least two points: EC and target */
        QnMediaServerResourcePtr proxy = qnResPool->getResourceById(route.points[route.points.size() - 2].peerId).dynamicCast<QnMediaServerResource>();
        if (!proxy) {
            m_failedPeers.insert(id);
            continue;
        }

        QUrl url;
        url.setScheme(lit("http"));
        url.setHost(route.points.last().host);
        url.setPort(route.points.last().port);

        int handle = m_mergeTool->configureIncompatibleServer(proxy, url, m_user, m_password);
        m_pendingPeers.insert(id);
        m_peerIdByHandle[handle] = id;
    }

    if (m_pendingPeers.isEmpty())
        finish(m_failedPeers.isEmpty() ? NoError : UnknownError, m_failedPeers);
}

void QnConfigurePeerTask::at_mergeTool_mergeFinished(int errorCode, const QnModuleInformation &moduleInformation, int handle) {
    Q_UNUSED(moduleInformation)

    QnUuid id = m_peerIdByHandle.take(handle);

    if (id.isNull() || !m_pendingPeers.remove(id))
        return;

    if (errorCode != QnMergeSystemsTool::NoError) {
        switch (errorCode) {
        case QnMergeSystemsTool::AuthentificationError:
            m_error = AuthentificationFailed;
            break;
        default:
            m_error = UnknownError;
            break;
        }
        m_failedPeers.insert(id);
    }

    if (m_pendingPeers.isEmpty())
        finish(m_error, m_failedPeers);
}
