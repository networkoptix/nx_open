// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <unordered_map>

#include <QtCore/QJsonValue>
#include <QtCore/QString>

#include <nx/utils/serialization/qt_core_types.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/types/resource_types.h>

#include "check_resource_exists.h"
#include "data_macros.h"
#include "id_data.h"

namespace nx::vms::api {

struct NX_VMS_API ResourceData: IdData
{
    using base_type = IdData;

    nx::Uuid parentId;
    QString name;
    QString url;
    nx::Uuid typeId;

    ResourceData() = default;
    ResourceData(const nx::Uuid& typeId): typeId(typeId) {}

    bool operator==(const ResourceData& other) const = default;

    static nx::Uuid getFixedTypeId(const QString& typeName);
};
#define ResourceData_Fields IdData_Fields (parentId)(name)(url)(typeId)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(ResourceData)
NX_REFLECTION_INSTRUMENT(ResourceData, ResourceData_Fields);

struct NX_VMS_API ResourceStatusData: IdData
{
    ResourceStatusData() = default;
    ResourceStatusData(const nx::Uuid& id, ResourceStatus status): IdData(id), status(status) {}

    ResourceStatus status = ResourceStatus::offline;

    bool operator==(const ResourceStatusData& other) const = default;
};
#define ResourceStatusData_Fields IdData_Fields (status)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(ResourceStatusData)

struct NX_VMS_API ResourceParamData
{
    ResourceParamData() = default;

    ResourceParamData(const QString& name, const QString& value):
        value(value),
        name(name)
    {
    }

    bool operator==(const ResourceParamData& other) const = default;

    QString value;
    QString name;
};
#define ResourceParamData_Fields (value)(name)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(ResourceParamData)
NX_REFLECTION_INSTRUMENT(ResourceParamData, ResourceParamData_Fields)

// REFACTORME:
// Apparently, this type exists for mapping named parameters to ids.
// Instead of a list of {.id, .value, .name} it can be done with a simple `std::map<nx::Uuid, ResourceParamData>`.
// In this case, mapping between API and DB types can be done in a straightforward loop without convoluted indirections.
struct NX_VMS_API ResourceParamWithRefData: ResourceParamData
{
    ResourceParamWithRefData() = default;

    ResourceParamWithRefData(
        const nx::Uuid& resourceId,
        const QString& name,
        const QString& value,
        CheckResourceExists checkResourceExists = CheckResourceExists::yes)
        :
        ResourceParamData(name, value),
        resourceId(resourceId),
        checkResourceExists(checkResourceExists)
    {
    }

    bool operator==(const ResourceParamWithRefData& other) const = default;
    nx::Uuid getId() const { return resourceId; }

    nx::Uuid resourceId;

    /** Used by ...Model::toDbTypes() and transaction-description-modify checkers. */
    CheckResourceExists checkResourceExists = CheckResourceExists::yes; /**<%apidoc[unused] */
};
#define ResourceParamWithRefData_Fields ResourceParamData_Fields (resourceId)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(ResourceParamWithRefData)

// This can and should be done by QueryProcessor.
std::unordered_map<nx::Uuid, std::vector<ResourceParamData>> NX_VMS_API toParameterMap(
    std::vector<ResourceParamWithRefData> parametersWithIds);

struct NX_VMS_API ResourceWithParameters
{
    ResourceParamWithRefDataList asList(const nx::Uuid& id) const;
    void setFromList(const ResourceParamDataList& list);
    void setFromList(const ResourceParamWithRefDataList& list);
    void extractFromList(const nx::Uuid& id, ResourceParamWithRefDataList* list);
    void setFromParameter(const ResourceParamData& parameter);
    std::optional<QJsonValue> parameter(const QString& key) const;

    /**%apidoc [opt] Extended resource parameters. */
    std::map<QString, QJsonValue> parameters;

    template<typename Derived, typename DbBaseType, typename DbListTypes>
    static std::vector<Derived> fillListFromDbTypes(
        DbListTypes* all, std::function<Derived(DbBaseType)> convert)
    {
        auto& baseList = std::get<std::vector<DbBaseType>>(*all);
        const auto size = baseList.size();
        std::vector<Derived> result;
        result.reserve(size);
        auto& allParameters = std::get<ResourceParamWithRefDataList>(*all);
        for (auto& baseData: baseList)
        {
            Derived model = convert(std::move(baseData));
            model.extractFromList(model.getId(), &allParameters);
            result.emplace_back(std::move(model));
        }
        return result;
    }
};

} // namespace nx::vms::api
