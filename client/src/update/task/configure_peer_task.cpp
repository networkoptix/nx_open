#include "configure_peer_task.h"

#include <QtNetwork/QNetworkReply>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <api/model/configure_reply.h>
#include <utils/merge_systems_tool.h>
#include <common/common_module.h>

QnConfigurePeerTask::QnConfigurePeerTask(QObject *parent) :
    QnNetworkPeerTask(parent),
    m_mergeTool(new QnMergeSystemsTool(this))
{
    connect(m_mergeTool, &QnMergeSystemsTool::mergeFinished, this, &QnConfigurePeerTask::at_mergeTool_mergeFinished);
}

QString QnConfigurePeerTask::adminPassword() const
{
    return m_adminPassword;
}

void QnConfigurePeerTask::setAdminPassword(const QString &password)
{
    m_adminPassword = password;
}

void QnConfigurePeerTask::doStart()
{
    m_error = NoError;
    m_pendingPeers.clear();

    for (const QnUuid &id: peers())
    {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id, true).dynamicCast<QnMediaServerResource>();
        if (!server)
        {
            m_failedPeers.insert(id);
            continue;
        }

        QnMediaServerResourcePtr ecServer = qnResPool->getResourceById<QnMediaServerResource>(qnCommon->remoteGUID());
        if (!ecServer)
        {
            m_failedPeers.insert(id);
            continue;
        }

        int handle = m_mergeTool->configureIncompatibleServer(ecServer, server->getApiUrl(), m_adminPassword);
        m_pendingPeers.insert(id);
        m_peerIdByHandle[handle] = id;
    }

    if (m_pendingPeers.isEmpty())
        finish(m_failedPeers.isEmpty() ? NoError : UnknownError, m_failedPeers);
}

void QnConfigurePeerTask::at_mergeTool_mergeFinished(int errorCode, const QnModuleInformation &moduleInformation, int handle)
{
    Q_UNUSED(moduleInformation)

    QnUuid id = m_peerIdByHandle.take(handle);

    if (id.isNull() || !m_pendingPeers.remove(id))
        return;

    if (errorCode != QnMergeSystemsTool::NoError)
    {
        switch (errorCode)
        {
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
