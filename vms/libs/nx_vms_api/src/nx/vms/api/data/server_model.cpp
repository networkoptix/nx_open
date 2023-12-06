// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_model.h"

#include <nx/fusion/model_functions.h>
#include <nx/utils/std/algorithm.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ServerModel, (json), ServerModel_Fields)

ServerModel::DbUpdateTypes ServerModel::toDbTypes() &&
{
    MediaServerData mainData;
    mainData.id = std::move(id);
    mainData.name = std::move(name);
    mainData.url = std::move(url);
    mainData.version = std::move(version);
    mainData.flags = std::move(flags);
    mainData.authKey = std::move(authKey);
    if (!endpoints.empty())
    {
        mainData.networkAddresses =
            QStringList(endpoints.begin(), endpoints.end()).join(QChar(';'));
    }
    if (osInfo)
        mainData.osInfo = osInfo->toString();

    MediaServerUserAttributesData attributesData;
    attributesData.serverId = mainData.id;
    attributesData.serverName = mainData.name;
    attributesData.maxCameras = maxCameras.value_or(0);
    attributesData.allowAutoRedundancy = isFailoverEnabled;
    attributesData.backupBitrateBytesPerSecond = backupBitrateBytesPerSecond;
    attributesData.locationId = locationId;

    attributesData.checkResourceExists = CheckResourceExists::no;

    std::optional<ResourceStatusData> statusData;
    if (status)
        statusData = ResourceStatusData(mainData.id, *status);

    auto parameters = asList(mainData.id);
    return {
        std::move(mainData),
        std::move(attributesData),
        std::move(statusData),
        std::move(parameters),
    };
}

std::vector<ServerModel> ServerModel::fromDbTypes(DbListTypes all)
{
    auto& allAttributes = std::get<MediaServerUserAttributesDataList>(all);
    auto& allStatuses = std::get<ResourceStatusDataList>(all);
    auto& allStorages = std::get<StorageDataList>(all);
    return ResourceWithParameters::fillListFromDbTypes<ServerModel, MediaServerData>(
        &all,
        [&allAttributes, &allStatuses, &allStorages](MediaServerData data)
        {
            ServerModel model;
            model.id = std::move(data.id);
            model.name = std::move(data.name);
            model.url = std::move(data.url);
            model.version = std::move(data.version);
            if (!data.networkAddresses.isEmpty())
            {
                for (const auto& endpoint: data.networkAddresses.split(QChar(';')))
                    model.endpoints.push_back(endpoint);
            }
            if (!data.osInfo.isEmpty())
                model.osInfo = nx::utils::OsInfo::fromString(data.osInfo);
            model.flags = std::move(data.flags);
            model.authKey = std::move(data.authKey);

            if (auto attrs = nx::utils::find_if(
                allAttributes, [id = model.getId()](const auto& a) { return a.serverId == id; }))
            {
                if (!attrs->serverName.isEmpty())
                    model.name = std::move(attrs->serverName);
                if (attrs->maxCameras)
                    model.maxCameras = attrs->maxCameras;
                model.isFailoverEnabled = attrs->allowAutoRedundancy;
                model.backupBitrateBytesPerSecond = attrs->backupBitrateBytesPerSecond;
                model.locationId = attrs->locationId;
            }

            const auto status = nx::utils::find_if(
                allStatuses, [id = model.getId()](const auto& s) { return s.id == id; });
            model.status = status ? status->status : ResourceStatus::offline;

            nx::utils::erase_if(
                allStorages,
                [&model](const auto& storage)
                {
                    if (storage.parentId != model.getId())
                        return false;
                    if (!model.storages)
                        model.storages = std::vector<StorageModel>();
                    model.storages->push_back(StorageModel::fromDb(storage));
                    return true;
                });

            return model;
        });
}

} // namespace nx::vms::api
