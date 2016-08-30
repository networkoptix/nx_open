#include "validate_update_peer_task.h"

#include <core/resource/media_server_resource.h>
#include <api/server_rest_connection.h>
#include <api/model/update_information_reply.h>
#include <common/common_module.h>

QnValidateUpdatePeerTask::QnValidateUpdatePeerTask(QObject* parent):
    base_type(parent)
{
}

void QnValidateUpdatePeerTask::doStart()
{
    const auto currentServer = qnCommon->currentServer();
    if (!currentServer)
    {
        finish(ValidationFailed);
        return;
    }

    auto handleReply =
        [this](bool success, rest::Handle requestId, const QnCloudHostCheckReply& reply)
        {
            if (m_requestId != requestId)
                return;

            m_requestId = -1;

            if (!success)
            {
                finish(ValidationFailed, peers());
                return;
            }

            if (reply.failedServers.isEmpty())
                finish(NoError);
            else
                finish(CloudHostConflict, QSet<QnUuid>::fromList(reply.failedServers));
        };

    m_requestId = currentServer->restConnection()->checkCloudHost(handleReply);
}

void QnValidateUpdatePeerTask::doCancel()
{
    if (m_requestId >= 0)
    {
        const auto currentServer = qnCommon->currentServer();
        if (currentServer)
            currentServer->restConnection()->cancelRequest(m_requestId);
        m_requestId = -1;
    }
}
