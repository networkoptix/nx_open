#include "multiserver_update_request_helpers.h"
#include <nx/update/update_information.h>
#include <media_server/media_server_module.h>
#include <common/common_module.h>
#include <rest/server/json_rest_result.h>

using namespace nx::vms::server;

namespace detail {

void checkUpdateStatusRemotely(
    const IfParticipantPredicate& ifParticipantPredicate,
    QnMediaServerModule* serverModule,
    const QString& path,
    QList<nx::update::Status>* reply,
    QnMultiserverRequestContext<QnEmptyRequestData>* context)
{
    auto mergeFunction =
        [serverModule, reply](
            const QnUuid& serverId, bool success, QnJsonRestResult& result, QnJsonRestResult&)
        {
            if (success)
            {
                const auto remotePeerUpdateStatus =
                    result.deserialized<QList<nx::update::Status>>()[0];

                reply->append(remotePeerUpdateStatus.serverId.isNull()
                    ? nx::update::Status(serverId, nx::update::Status::Code::offline)
                    : remotePeerUpdateStatus);
            }
            else
            {
                reply->append(serverId != serverModule->commonModule()->moduleGUID()
                    ? nx::update::Status(serverId, nx::update::Status::Code::offline)
                    : serverModule->updateManager()->status());
            }
        };

    QnJsonRestResult result;
    requestRemotePeers(
        serverModule->commonModule(), path, result, context, mergeFunction,
        ifParticipantPredicate);
    auto offlineServers = QSet<QnMediaServerResourcePtr>::fromList(
        serverModule->commonModule()->resourcePool()->getAllServers(Qn::Offline));

    for (const auto& offlineServer: offlineServers)
    {
        if (ifParticipantPredicate)
        {
            const auto participationStatus = ifParticipantPredicate(
                offlineServer->getId(),
                offlineServer->getModuleInformation().version);

            if (participationStatus != ParticipationStatus::participant)
                continue;
        }

        if (std::find_if(reply->cbegin(), reply->cend(),
            [&offlineServer](const nx::update::Status& updateStatus)
            {
                return updateStatus.serverId == offlineServer->getId();
            }) != reply->cend())
        {
            continue;
        }

        reply->append(nx::update::Status(
            offlineServer->getId(), nx::update::Status::Code::offline));
    }
}

IfParticipantPredicate makeIfParticipantPredicate(
    UpdateManager* updateManager, const QList<QnUuid>& forcedParticipants)
{
    try
    {
        const auto updateInfo = updateManager->updateInformation(
            UpdateManager::InformationCategory::target);

        const auto participants =
            forcedParticipants.isEmpty() ? updateInfo.participants : forcedParticipants;

        return
            [updateInfo, participants](
                const QnUuid& id,
                const nx::vms::api::SoftwareVersion& version)
            {
                if (!participants.isEmpty() && !participants.contains(id))
                    return ParticipationStatus::notInList;

                return version <= nx::vms::api::SoftwareVersion(updateInfo.version)
                    ? ParticipationStatus::participant : ParticipationStatus::incompatibleVersion;
            };
    }
    catch (const std::exception& e)
    {
        NX_DEBUG(nx::utils::log::Tag(QString("makeIfParticipantPredicate")), e.what());
        return nullptr;
    }
}

} // namespace detail
