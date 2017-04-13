#include "check_free_space_peer_task.h"

#include <quazip/quazip.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <api/server_rest_connection.h>
#include <api/model/update_information_reply.h>
#include <common/common_module.h>

namespace {

const qreal kReserveFactor = 1.2;
const qreal kWindowsReservedFreeSpace = 500 * 1024 * 1024;
const QnSoftwareVersion kRequireFreeSpaceVersion(3, 0);

qint64 spaceRequiredForUpdate(
    const QnSystemInformation& systemInformation, const QString& fileName)
{
    QFile updateFile(fileName);

    if (!updateFile.exists())
        return 0;

    qint64 result = updateFile.size();

    QuaZip zip(fileName);
    for (const auto& info: zip.getFileInfoList64())
        result += info.uncompressedSize;

    result *= kReserveFactor;

    if (systemInformation.platform == lit("windows"))
        result += kWindowsReservedFreeSpace;

    return result;
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
        m_requiredSpaceBySystemInformation[it.key()] = spaceRequiredForUpdate(it.key(), *it);

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
                const auto server = resourcePool()->getResourceById<QnMediaServerResource>(id);
                if (!server)
                {
                    finish(CheckFailed, peers());
                    return;
                }

                const auto freeSpaceAvailable = reply.freeSpaceByServerId.value(id, -1);
                if (freeSpaceAvailable < 0)
                {
                    if (server->getVersion() >= kRequireFreeSpaceVersion)
                    {
                        finish(CheckFailed, { id });
                        return;
                    }

                    continue;
                }

                const auto systemInformation = server->getModuleInformation().systemInformation;
                const auto freeSpaceRequired =
                    m_requiredSpaceBySystemInformation.value(systemInformation);

                if (freeSpaceAvailable < freeSpaceRequired)
                    failed.insert(id);
            }

            if (failed.isEmpty())
                finish(NoError);
            else
                finish(NotEnoughFreeSpaceError, failed);
        };

    const auto currentServer = commonModule()->currentServer();
    if (!currentServer)
    {
        finish(CheckFailed);
        return;
    }

    m_requestId = currentServer->restConnection()->getFreeSpaceForUpdateFiles(handleReply);
}

void QnCheckFreeSpacePeerTask::doCancel()
{
    if (m_requestId >= 0)
    {
        const auto currentServer = commonModule()->currentServer();
        if (currentServer)
            currentServer->restConnection()->cancelRequest(m_requestId);
        m_requestId = -1;
    }
}
