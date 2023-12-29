// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/fusion/model_functions.h>
#include <nx/utils/std/algorithm.h>

#include "resource_data.h"

namespace nx {
namespace vms {
namespace api {

QnUuid ResourceData::getFixedTypeId(const QString& typeName)
{
    return QnUuid::fromArbitraryData(typeName.toUtf8() + QByteArray("-"));
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ResourceParamData, (ubjson)(xml)(json)(sql_record)(csv_record), ResourceParamData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ResourceParamWithRefData,
    (ubjson)(xml)(json)(sql_record)(csv_record), ResourceParamWithRefData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ResourceData, (ubjson)(xml)(json)(sql_record)(csv_record), ResourceData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ResourceStatusData, (ubjson)(xml)(json)(sql_record)(csv_record), ResourceStatusData_Fields)

ResourceParamWithRefDataList ResourceWithParameters::asList(const QnUuid& id) const
{
    ResourceParamWithRefDataList list;
    if (!parameters.empty())
    {
        list.reserve(parameters.size());
        for (const auto& p: parameters)
        {
            list.push_back(ResourceParamWithRefData(
                id,
                p.first,
                p.second.isString() ? p.second.toString() : QJson::serialized(p.second),
                CheckResourceExists::no));
        }
    }
    return list;
}

void ResourceWithParameters::setFromList(const ResourceParamDataList& list)
{
    for (const auto& parameter: list)
        setFromParameter(parameter);
}

void ResourceWithParameters::setFromList(const ResourceParamWithRefDataList& list)
{
    for (const auto& parameter: list)
        setFromParameter(parameter);
}

void ResourceWithParameters::extractFromList(const QnUuid& id, ResourceParamWithRefDataList* list)
{
    nx::utils::erase_if(
        *list,
        [this, id](const auto& parameter)
        {
            if (parameter.resourceId != id)
                return false;
            setFromParameter(parameter);
            return true;
        });
}

void ResourceWithParameters::setFromParameter(const ResourceParamData& parameter)
{
    QJsonValue& value = parameters[parameter.name];
    if (!QJson::deserialize(parameter.value, &value))
        value = parameter.value;
}

std::optional<QJsonValue> ResourceWithParameters::parameter(const QString& key) const
{
    if (const auto it = parameters.find(key); it != parameters.end())
        return it->second;

    return std::nullopt;
}

std::unordered_map<QnUuid, std::vector<ResourceParamData>> toParameterMap(
    std::vector<ResourceParamWithRefData> parametersWithIds)
{
    std::unordered_map<QnUuid, std::vector<ResourceParamData>> result;
    std::sort(parametersWithIds.begin(),
        parametersWithIds.end(),
        [](const auto& lhs, const auto& rhs) { return lhs.getId() < rhs.getId(); });

    for (auto l = parametersWithIds.begin(); l != parametersWithIds.end();)
    {
        auto r = l;
        while (r != parametersWithIds.end() && r->getId() == l->getId())
            ++r;

        auto& resources = result[l->getId()];
        resources.reserve(std::distance(l, r));
        std::transform(std::make_move_iterator(l),
            std::make_move_iterator(r),
            std::back_inserter(resources),
            [](ResourceParamWithRefData data) -> ResourceParamData { return std::move(data); });

        l = r;
    }

    return result;
}

} // namespace api
} // namespace vms
} // namespace nx
