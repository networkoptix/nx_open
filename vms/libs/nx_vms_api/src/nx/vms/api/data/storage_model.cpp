// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "storage_model.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

namespace {

static const QString kStorageArchiveModeKey{"storageArchiveMode"};

} // namespace

QN_FUSION_ADAPT_STRUCT(StorageModel, StorageModel_Fields)
QN_FUSION_DEFINE_FUNCTIONS(StorageModel, (json)(csv_record)(ubjson)(xml))

StorageModel::DbUpdateTypes StorageModel::toDbTypes() &&
{
    StorageData mainData;
    mainData.id = std::move(id);
    mainData.parentId = std::move(serverId);
    mainData.url = std::move(path);
    mainData.storageType = std::move(type);
    mainData.name = std::move(name);
    if (spaceLimitB)
        mainData.spaceLimit = *spaceLimitB;
    mainData.usedForWriting = isUsedForWriting;
    mainData.isBackup = isBackup;

    std::optional<ResourceStatusData> statusData;
    if (status)
        statusData = ResourceStatusData(mainData.id, *status);

    auto parameters = asList(mainData.id);
    if (storageArchiveMode
        && std::find_if(
            parameters.cbegin(), parameters.cend(),
            [](const auto& p) { return p.name == kStorageArchiveModeKey; }) == parameters.cend())
    {
        parameters.push_back({
            mainData.id,
            kStorageArchiveModeKey,
            QString::fromStdString(nx::reflect::toString(*storageArchiveMode)),
            CheckResourceExists::no});
    }

    return {std::move(mainData), std::move(statusData), std::move(parameters)};
}

std::vector<StorageModel> StorageModel::fromDbTypes(DbListTypes all)
{
    std::vector<StorageModel> result;
    auto& dataList = std::get<StorageDataList>(all);
    result.reserve(dataList.size());
    for (auto& data: dataList)
        result.push_back(fromDb(data));
    return result;
}

StorageModel StorageModel::fromDb(StorageData data)
{
    StorageModel model;
    model.id = std::move(data.id);
    model.serverId = std::move(data.parentId);
    model.path = std::move(data.url);
    model.type = std::move(data.storageType);
    model.name = std::move(data.name);
    if (data.spaceLimit != -1) model.spaceLimitB = data.spaceLimit;
    model.isUsedForWriting = data.usedForWriting;
    model.isBackup = data.isBackup;
    model.status = data.status;
    model.setFromList(data.addParams);

    for (auto it = model.parameters.begin(); it != model.parameters.end();)
    {
        if (it->first == kStorageArchiveModeKey)
        {
            QJson::deserialize(it->second, &model.storageArchiveMode);
            it = model.parameters.erase(it);
            continue;
        }

        ++it;
    }

    return model;
}

} // namespace api
} // namespace vms
} // namespace nx
