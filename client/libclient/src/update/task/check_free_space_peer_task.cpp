#include "check_free_space_peer_task.h"

#include <quazip/quazip.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <api/server_rest_connection.h>
#include <api/model/update_information_reply.h>
#include <common/common_module.h>

namespace {

const qreal kReserveFactor = 1.2;

qint64 spaceRequiredForUpdate(const QString& fileName)
{
    QFile updateFile(fileName);

    if (!updateFile.exists())
        return 0;

    qint64 result = updateFile.size();

    QuaZip zip(fileName);
    for (const auto info: zip.getFileInfoList64())
        result += info.uncompressedSize;

    return result * kReserveFactor;
}

} // namespace

QnCheckFreeSpacePeerTask::QnCheckFreeSpacePeerTask(QObject* parent):
    base_type(parent)
{
}

void QnCheckFreeSpacePeerTask::setUpdateFiles(
    const QHash<QnSystemInformation, QString>& fileBySystemInformation)
{
    m_files = fileBySystemInformation;
}

void QnCheckFreeSpacePeerTask::doStart()
{
    m_requiredSpaceBySystemInformation.clear();
    for (auto it = m_files.begin(); it != m_files.end(); ++it)
        m_requiredSpaceBySystemInformation[it.key()] = spaceRequiredForUpdate(it.value());

    auto handleReply =
        [this](bool success, rest::Handle requestId, const QnUpdateFreeSpaceReply& reply)
        {
            if (m_requestId != requestId)
                return;

            m_requestId = -1;

            if (!success)
            {
                finish(CheckFailed, peers());
                return;
            }

            QSet<QnUuid> failed;
            for (const auto& id: peers())
            {
                const auto server = qnResPool->getResourceById<QnMediaServerResource>(id);
                if (!server)
                {
                    finish(CheckFailed, peers());
                    return;
                }

                const auto systemInformation = server->getModuleInformation().systemInformation;
                const auto freeSpaceAvailable = reply.freeSpaceByServerId.value(id, -1);
                const auto freeSpaceRequired =
                    m_requiredSpaceBySystemInformation.value(systemInformation);

                if (freeSpaceAvailable >= 0 && freeSpaceAvailable < freeSpaceRequired)
                    failed.insert(id);
            }

            if (failed.isEmpty())
                finish(NoError);
            else
                finish(NotEnoughFreeSpaceError, failed);
        };

    // TODO: #dklychkov Revise this fix: currentServer() returns null if connected to bpi.
    auto currentServer = qnCommon->currentServer();
    if (currentServer)
    {
        m_requestId = currentServer->restConnection()->getFreeSpaceForUpdateFiles(handleReply);
    }
    else
    {
        m_requestId = -1;
        finish(NoError);
    }
}

void QnCheckFreeSpacePeerTask::doCancel()
{
    if (m_requestId >= 0)
    {
        auto currentServer = qnCommon->currentServer();
        if (currentServer)
            currentServer->restConnection()->cancelRequest(m_requestId);
        m_requestId = -1;
    }
}
