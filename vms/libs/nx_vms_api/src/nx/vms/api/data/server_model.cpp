// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_model.h"

#include <set>

#include <nx/fusion/model_functions.h>
#include <nx/utils/crud_model.h>
#include <nx/utils/std/algorithm.h>
#include <nx/vms/api/data/resource_property_key.h>

namespace nx::vms::api {

namespace {

struct LessById
{
    using is_transparent = std::true_type;

    template<typename L, typename R>
    bool operator()(const L& lhs, const R& rhs) const
    {
        using nx::utils::model::getId;
        return getId(lhs) < getId(rhs);
    }
} constexpr lessById{};

struct LessByParentId
{
    using is_transparent = std::true_type;

    template<typename L, typename R>
    bool operator()(const L& lhs, const R& rhs) const
    {
        return lhs.parentId < rhs.parentId;
    }
};

struct ServerIdAsParentId
{
    nx::Uuid parentId;

    ServerIdAsParentId() = default;
    ServerIdAsParentId(const ServerModelBase& model): parentId(model.id) {}
};

std::vector<QString> endpointsFromServerData(MediaServerData&& d)
{
    if (d.networkAddresses.isEmpty())
        return {};

    auto s = d.networkAddresses.split(';');
    return {std::make_move_iterator(s.begin()), std::make_move_iterator(s.end())};
}

QString endpointsToServerData(std::vector<QString> endpoints)
{
    return QStringList(endpoints.begin(), endpoints.end()).join(QChar(';'));
}

std::map<QString, QString> networkInterfacesFromParameter(QString serialized)
{
    std::map<QString, QString> interfaces;
    for (auto&& item: serialized.split(", "))
    {
        auto delimPos = item.lastIndexOf(": ");
        if (delimPos < 0)
            continue;
        interfaces.emplace(item.mid(0, delimPos), item.mid(delimPos + 2));
    }
    return interfaces;
}

template<typename Model>
void extractFailoverSettings(Model* m, const MediaServerUserAttributesData& data)
{
    // Default initialization for `ServerModel::maxCameras` is `0`.
    // No need to check for `0` for the attribute.
    m->maxCameras = data.maxCameras;
    m->isFailoverEnabled = data.allowAutoRedundancy;
    m->locationId = data.locationId;
}

template<typename Model>
void saveFailoverSettings(const Model& model, MediaServerUserAttributesData* data)
{
    data->maxCameras = model.maxCameras;
    data->allowAutoRedundancy = model.isFailoverEnabled;
    data->locationId = model.locationId;
}

void extractParametersToFields(ServerModelV1* /*m*/)
{}

void extractParametersToFields(ServerModelV4* m)
{
    if (!m->network)
        m->network.emplace();

    if (auto param = m->takeParameter(server_properties::kCertificate))
    {
        if (auto value = param->toString(); !value.isEmpty())
            m->network->certificatePem = value.toStdString();
    }
    if (auto param = m->takeParameter(server_properties::kUserProvidedCertificate))
    {
        if (auto value = param->toString(); !value.isEmpty())
            m->network->userProvidedCertificatePem = value.toStdString();
    }
    if (auto param = m->takeParameter(server_properties::kPublicIp))
        m->network->publicIp = param->toString();
    if (auto param = m->takeParameter(server_properties::kNetworkInterfaces))
        m->network->networkInterfaces = networkInterfacesFromParameter(param->toString());

    if (!m->runtimeInformation)
        m->runtimeInformation.emplace();

    const auto hwInfoGet =
        [m]()
        {
            if (!m->runtimeInformation->hardwareInformation)
                m->runtimeInformation->hardwareInformation.emplace();
            return &m->runtimeInformation->hardwareInformation.value();
        };
    if (auto param = m->takeParameter(server_properties::kCpuArchitecture))
        hwInfoGet()->cpuArchitecture = param->toString();
    if (auto param = m->takeParameter(server_properties::kCpuModelName))
        hwInfoGet()->cpuModelName = param->toString();
    if (auto param = m->takeParameter(server_properties::kPhysicalMemory))
        hwInfoGet()->physicalMemoryB = param->toDouble();

    if (auto param = m->takeParameter(server_properties::kTimeZoneInformation);
        param && param->isObject())
    {
        m->runtimeInformation->timezone.emplace();
        QJson::deserialize(*param, &m->runtimeInformation->timezone.value());
    }
    if (auto param = m->takeParameter(server_properties::kGuidConflictDetected))
        m->runtimeInformation->idConflictDetected = param->toBool();

    if (!m->settings)
        m->settings.emplace();

    if (auto param = m->takeParameter(server_properties::kWebCamerasDiscoveryEnabled))
        m->settings->webCamerasDiscoveryEnabled = param->toBool();
}

void moveFieldsToParameters(ServerModelV1* /*m*/)
{}

void moveFieldsToParameters(ServerModelV4* m)
{
    m->parameters[server_properties::kCertificate] =
        m->network
            ? QString::fromStdString(m->network->certificatePem)
            : QString();

    m->parameters[server_properties::kWebCamerasDiscoveryEnabled] =
        m->settings ? m->settings->webCamerasDiscoveryEnabled : false;
}

template<typename Model>
std::vector<Model> fromServerData(std::vector<MediaServerData> dataList)
{
    using namespace nx::utils;

    std::vector<Model> result;
    result.reserve(dataList.size());

    for (auto&& d: dataList)
    {
        Model m;
        m.id = d.id;
        m.name = std::move(d.name);
        m.url = std::move(d.url);
        m.version = std::move(d.version);
        m.authKey = std::move(d.authKey);
        m.osInfo =
            !d.osInfo.isEmpty() ? std::optional{OsInfo::fromString(d.osInfo)} : std::nullopt;
        m.status = ResourceStatus::undefined;
        if constexpr (std::is_same_v<Model, ServerModelV1>)
        {
            m.flags = d.flags;
            m.endpoints = endpointsFromServerData(std::move(d));
        }
        else
        {
            m.flags = convertServerFlagsToModel(d.flags);
            m.network.emplace();
            m.network->endpoints = endpointsFromServerData(std::move(d));
        }

        result.push_back(std::move(m));
    }

    return result;
}

template<typename Model>
typename Model::DbUpdateTypes toDbTypes(Model model)
{
    MediaServerData mainData;
    mainData.id = std::move(model.id);
    mainData.name = std::move(model.name);
    mainData.url = std::move(model.url);
    mainData.version = std::move(model.version);
    mainData.authKey = std::move(model.authKey);
    if (model.osInfo)
        mainData.osInfo = model.osInfo->toString();

    if constexpr (std::is_same_v<Model, ServerModelV1>)
    {
        mainData.flags = std::move(model.flags);
        if (model.endpoints.empty())
            mainData.networkAddresses = endpointsToServerData(std::move(model.endpoints));
    }
    else
    {
        mainData.flags = convertModelToServerFlags(model.flags);
        if (model.network && !model.network->endpoints.empty())
            mainData.networkAddresses = endpointsToServerData(std::move(model.network->endpoints));
    }

    MediaServerUserAttributesData attributesData;
    attributesData.serverId = mainData.id;
    attributesData.serverName = mainData.name;
    attributesData.backupBitrateBytesPerSecond = model.backupBitrateBytesPerSecond;

    if constexpr (std::is_same_v<Model, ServerModelV1>)
    {
        saveFailoverSettings(model, &attributesData);
    }
    else
    {
        saveFailoverSettings(model.settings.value_or(ServerSettings()), &attributesData);
    }
    attributesData.checkResourceExists = CheckResourceExists::no;

    moveFieldsToParameters(&model);

    auto parameters = model.asList(mainData.id);
    return {
        std::move(mainData),
        std::move(attributesData),
        std::move(parameters),
    };
}

template<typename Model>
std::vector<Model> fromDbTypes(typename Model::DbListTypes all)
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

    std::vector<Model> servers = fromServerData<Model>(nx::utils::unique_sorted(
        std::get<std::vector<MediaServerData>>(std::move(all)), lessById));

    // This all can be done by the QueryProcessor or the CrudHandler.
    auto attributes = nx::utils::unique_sorted(
        std::get<std::vector<MediaServerUserAttributesData>>(std::move(all)), lessById);
    auto statuses = nx::utils::unique_sorted(
        std::get<std::vector<ResourceStatusData>>(std::move(all)), lessById);
    std::unordered_map<nx::Uuid, std::vector<ResourceParamData>> parametersPerServer =
        toParameterMap(std::get<std::vector<ResourceParamWithRefData>>(std::move(all)));
    auto storages =
        [](std::vector<StorageData> list)
        {
            std::multiset<StorageData, LessByParentId> result;
            for (auto&& s: list)
                result.insert(std::move(s));
            return result;
        }(std::get<std::vector<StorageData>>(std::move(all)));

    for (Model& s: servers)
    {
        if (auto a = binaryFind(attributes, s))
        {
            // Override, if user has specified an alias
            s.name = !a->serverName.isEmpty()
                ? std::move(a->serverName)
                : std::move(s.name);

            s.backupBitrateBytesPerSecond = a->backupBitrateBytesPerSecond;
            if constexpr (std::is_same_v<Model, ServerModelV1>)
            {
                extractFailoverSettings(&s, std::move(*a));
            }
            else
            {
                s.settings.emplace();
                extractFailoverSettings(&s.settings.value(), std::move(*a));
            }
        }

        // `status` should not be nullable in the DB and this check should be an assertion
        if (auto status = binaryFind(statuses, s))
            s.status = status->status;
        else
            s.status = ResourceStatus::offline;

        if (auto parameters = parametersPerServer.find(s.id);
            parameters != parametersPerServer.cend())
        {
            for (const auto& parameter: parameters->second)
            {
                if (parameter.name == "portForwardingConfigurations")
                {
                    NX_ASSERT(QJson::deserialize(parameter.value, &s.portForwardingConfigurations),
                        "%1", parameter.value);
                }
                else
                {
                    static_cast<ResourceWithParameters&>(s).setFromParameter(parameter);
                }
            }
            parametersPerServer.erase(parameters);
        }

        if (!storages.empty())
        {
            auto [begin, end] = storages.equal_range(ServerIdAsParentId(s));
            s.storages.reserve(std::distance(begin, end));
            std::transform(std::make_move_iterator(std::move(begin)),
                std::make_move_iterator(std::move(end)),
                std::back_inserter(s.storages),
                [](StorageData d) { return StorageModel::fromDb(std::move(d)); });
        }
        extractParametersToFields(&s);
    }

    return servers;
}

} // namespace

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ServerModelBase, (json), ServerModelBase_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ServerModelV1, (json), ServerModelV1_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ServerModelV4, (json), ServerModelV4_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ServerHardwareInformation, (json), ServerHardwareInformation_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ServerTimeZoneInformation, (json), ServerTimeZoneInformation_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ServerNetwork, (json), ServerNetwork_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ServerSettings, (json), ServerSettings_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ServerRuntimeInfo, (json), ServerRuntimeInfo_Fields)

ServerModelV1::DbUpdateTypes ServerModelV1::toDbTypes() &&
{
    return api::toDbTypes(std::move(*this));
}

std::vector<ServerModelV1> ServerModelV1::fromDbTypes(DbListTypes all)
{
    return api::fromDbTypes<ServerModelV1>(std::move(all));
}

ServerModelV4::DbUpdateTypes ServerModelV4::toDbTypes() &&
{
    return api::toDbTypes(std::move(*this));
}

std::vector<ServerModelV4> ServerModelV4::fromDbTypes(DbListTypes all)
{
    return api::fromDbTypes<ServerModelV4>(std::move(all));
}

} // namespace nx::vms::api
