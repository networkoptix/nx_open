#include "start_update_rest_handler.h"
#include <media_server/media_server_module.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <nx/vms/server/server_update_manager.h>
#include <api/global_settings.h>
#include <nx/vms/server/settings.h>

QnStartUpdateRestHandler::QnStartUpdateRestHandler(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{
}

static void applyPersistentUpdateStorageSelection(QnMediaServerModule* serverModule)
{
    // #TODO Check if target or installed is not set and apply below only if needed
    // #TODO Implement application itself: settings->setUpdateStorage()
    const auto allServers =
        serverModule->commonModule()->resourcePool()->getAllServers(Qn::Online);
    using ServerToStorage = std::vector<std::pair<QnMediaServerResourcePtr, QnStorageResourcePtr>>;
    ServerToStorage serverWithLargestStorage;

    for (const auto& server: allServers)
    {
        auto storages = server->getStorages();
        if (storages.isEmpty())
            continue;

        std::sort(
            storages.begin(), storages.end(),
            [](const auto & s1, const auto & s2) { return s1->getTotalSpace() > s2->getTotalSpace(); });

        const int64_t kMinimumTotalSpaceOfTheLargestDrive = 1024 * 1024 * 1024LL; // 1 GB
        if (storages[0]->getTotalSpace() < kMinimumTotalSpaceOfTheLargestDrive
            || storages[0]->getFreeSpace() < storages[0]->getSpaceLimit() * 0.9)
        {
            continue;
        }

        serverWithLargestStorage.push_back(std::make_pair(server, storages[0]));
    }

    if (serverWithLargestStorage.empty())
        return;

    std::sort(
        serverWithLargestStorage.begin(), serverWithLargestStorage.end(),
        [](const auto& entry1, const auto& entry2)
        {
            return entry1.second->getFreeSpace() > entry2.second->getFreeSpace();
        });

    const size_t oneTenth = std::max<size_t>(1, serverWithLargestStorage.size() / 10);
    QList<QnUuid> serverIdList;
    for (size_t i = 0; i < oneTenth; ++i)
        serverIdList.push_back(serverWithLargestStorage[i].first->getId());
}

int QnStartUpdateRestHandler::executePost(
    const QString& /*path*/,
    const QnRequestParamList& /*params*/,
    const QByteArray& body,
    const QByteArray& /*srcBodyContentType*/,
    QByteArray& /*result*/,
    QByteArray& /*resultContentType*/,
    const QnRestConnectionProcessor* /*processor*/)
{
    serverModule()->updateManager()->startUpdate(body);
    return nx::network::http::StatusCode::ok;
}
