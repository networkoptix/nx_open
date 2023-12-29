// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_model.h"

#include <set>

#include <nx/fusion/model_functions.h>
#include <nx/utils/std/algorithm.h>

namespace nx::vms::api {

namespace {

struct LessById
{
    using is_transparent = std::true_type;

    template<typename L, typename R>
    bool operator()(const L& lhs, const R& rhs) const
    {
        return lhs.getId() < rhs.getId();
    }
} constexpr lessById{};

std::vector<ServerModel> fromServerData(std::vector<MediaServerData> dataList)
{
    using namespace nx::utils;

    std::vector<ServerModel> result;
    result.reserve(dataList.size());

    for (auto&& d: dataList)
    {
        using namespace nx::utils;

        // Don't mind Clang-Tidy: `emplace_back` will not compile.
        result.push_back({.id = d.id,
            .name = std::move(d.name),
            .url = std::move(d.url),
            .version = std::move(d.version),
            .endpoints =
                [&]() -> std::vector<QString>
                {
                    if (d.networkAddresses.isEmpty())
                        return {};

                    auto s = d.networkAddresses.split(';');
                    return {std::make_move_iterator(s.begin()), std::make_move_iterator(s.end())};
                }(),
            .authKey = std::move(d.authKey),
            .osInfo =
                !d.osInfo.isEmpty() ? std::optional{OsInfo::fromString(d.osInfo)} : std::nullopt,
            .flags = d.flags,
            .status = ResourceStatus::undefined});
    }

    return result;
}

} // namespace

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
    attributesData.maxCameras = maxCameras;
    attributesData.allowAutoRedundancy = isFailoverEnabled;
    attributesData.backupBitrateBytesPerSecond = backupBitrateBytesPerSecond;
    attributesData.locationId = locationId;

    attributesData.checkResourceExists = CheckResourceExists::no;

    auto parameters = asList(mainData.id);
    return {
        std::move(mainData),
        std::move(attributesData),
        std::move(parameters),
    };
}

std::vector<ServerModel> ServerModel::fromDbTypes(DbListTypes all)
{
    const auto binaryFind =
        [](auto& container, const auto& model)
        {
            return nx::utils::binary_find(container, model, lessById);
        };

    // Ignore Clang-Tidy warnings about `'all' is used after being moved`.
    // To move an element from `std::tuple`, a specific overload`T&& std::get(tuple&&)`
    // must be selected.
    // `std::move(all)` casts to rvalue-reference.
    // `std::get` moves only the requested element.

    std::vector<ServerModel> servers = fromServerData(nx::utils::unique_sorted(
        std::get<std::vector<MediaServerData>>(std::move(all)), lessById));

    // This all can be done by the QueryProcessor or the CrudHandler.
    auto attributes = nx::utils::unique_sorted(
        std::get<std::vector<MediaServerUserAttributesData>>(std::move(all)), lessById);
    auto statuses = nx::utils::unique_sorted(
        std::get<std::vector<ResourceStatusData>>(std::move(all)), lessById);
    std::unordered_map<QnUuid, std::vector<ResourceParamData>> parameters =
        toParameterMap(std::get<std::vector<ResourceParamWithRefData>>(std::move(all)));
    auto storages =
        [](std::vector<StorageData> list)
        {
            std::multiset<StorageData, LessById> result;
            for (auto&& s: list)
                result.insert(std::move(s));
            return result;
        }(std::get<std::vector<StorageData>>(std::move(all)));

    for (ServerModel& s: servers)
    {
        if (auto a = binaryFind(attributes, s))
        {
            // Override, if user has specified an alias
            s.name = !a->serverName.isEmpty()
                ? std::move(a->serverName)
                : std::move(s.name);

            // Default initialization for `ServerModel::maxCameras` is `0`.
            // No need to check for `0` for the attribute.
            s.maxCameras = a->maxCameras;
            s.isFailoverEnabled = a->allowAutoRedundancy;
            s.backupBitrateBytesPerSecond = a->backupBitrateBytesPerSecond;
            s.locationId = a->locationId;
        }

        // `status` should not be nullable in the DB and this check should be an assertion
        if (auto status = binaryFind(statuses, s))
            s.status = status->status;
        else
            s.status = ResourceStatus::offline;

        if (auto f = parameters.find(s.id); f != parameters.cend())
        {
            for (ResourceParamData& r: f->second)
                static_cast<ResourceWithParameters&>(s).setFromParameter(r);

            parameters.erase(f);
        }

        {
            const auto [begin, end] = storages.equal_range(s);
            s.storages.reserve(std::distance(begin, end));
            std::transform(std::make_move_iterator(begin),
                std::make_move_iterator(end),
                std::back_inserter(s.storages),
                [](StorageData d) -> StorageModel { return StorageModel::fromDb(std::move(d)); });
        }
    }

    return servers;
}

} // namespace nx::vms::api
