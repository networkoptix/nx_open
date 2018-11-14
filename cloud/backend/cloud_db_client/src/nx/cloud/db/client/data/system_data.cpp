#include "system_data.h"

#include <nx/fusion/model_functions.h>
#include <nx/network/url/query_parse_helper.h>

#include "../field_name.h"

namespace nx::cloud::db::api {

using namespace nx::network;

//-------------------------------------------------------------------------------------------------
// class SystemRegistrationData

MAKE_FIELD_NAME_STR_CONST(SystemRegistrationData, name)
MAKE_FIELD_NAME_STR_CONST(SystemRegistrationData, customization)
MAKE_FIELD_NAME_STR_CONST(SystemRegistrationData, opaque)

bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemRegistrationData* const data)
{
    if (!url::deserializeField(urlQuery, SystemRegistrationData_name_field, &data->name))
        return false;

    if (!url::deserializeField(
            urlQuery,
            SystemRegistrationData_customization_field,
            &data->customization))
    {
        return false;
    }

    // Optional field.
    url::deserializeField(urlQuery, SystemRegistrationData_opaque_field, &data->opaque);

    return true;
}

void serializeToUrlQuery(const SystemRegistrationData& data, QUrlQuery* const urlQuery)
{
    url::serializeField(urlQuery, SystemRegistrationData_name_field, data.name);
    url::serializeField(urlQuery, SystemRegistrationData_customization_field, data.customization);
    url::serializeField(urlQuery, SystemRegistrationData_opaque_field, data.opaque);
}

//-------------------------------------------------------------------------------------------------
// class SystemSharing

MAKE_FIELD_NAME_STR_CONST(SystemSharing, systemId)
MAKE_FIELD_NAME_STR_CONST(SystemSharing, accountEmail)
MAKE_FIELD_NAME_STR_CONST(SystemSharing, accessRole)
MAKE_FIELD_NAME_STR_CONST(SystemSharing, userRoleId)
MAKE_FIELD_NAME_STR_CONST(SystemSharing, customPermissions)
MAKE_FIELD_NAME_STR_CONST(SystemSharing, isEnabled)

bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemSharing* const systemSharing)
{
    if (!urlQuery.hasQueryItem(SystemSharing_systemId_field) ||
        !urlQuery.hasQueryItem(SystemSharing_accountEmail_field))
    {
        return false;
    }

    systemSharing->systemId = urlQuery.queryItemValue(SystemSharing_systemId_field).toStdString();
    systemSharing->accountEmail =
        urlQuery.queryItemValue(SystemSharing_accountEmail_field).toStdString();
    bool success = false;
    systemSharing->accessRole = QnLexical::deserialized<api::SystemAccessRole>(
        urlQuery.queryItemValue(SystemSharing_accessRole_field),
        api::SystemAccessRole::none,
        &success);
    systemSharing->userRoleId =
        urlQuery.queryItemValue(SystemSharing_userRoleId_field).toStdString();
    systemSharing->customPermissions =
        urlQuery.queryItemValue(SystemSharing_customPermissions_field).toStdString();
    if (urlQuery.hasQueryItem(SystemSharing_isEnabled_field))
        systemSharing->isEnabled =
            urlQuery.queryItemValue(SystemSharing_isEnabled_field) == "true";
    return success;
}

void serializeToUrlQuery(const SystemSharing& data, QUrlQuery* const urlQuery)
{
    urlQuery->addQueryItem(
        SystemSharing_systemId_field,
        QString::fromStdString(data.systemId));
    urlQuery->addQueryItem(
        SystemSharing_accountEmail_field,
        QString::fromStdString(data.accountEmail));
    urlQuery->addQueryItem(
        SystemSharing_accessRole_field,
        QnLexical::serialized(data.accessRole));
    urlQuery->addQueryItem(
        SystemSharing_userRoleId_field,
        QString::fromStdString(data.userRoleId));
    urlQuery->addQueryItem(
        SystemSharing_customPermissions_field,
        QString::fromStdString(data.customPermissions));
    urlQuery->addQueryItem(
        SystemSharing_isEnabled_field,
        data.isEnabled ? "true" : "false");
}


bool loadFromUrlQuery(const QUrlQuery& /*urlQuery*/, SystemSharingList* const /*systemSharing*/)
{
    //TODO
    NX_ASSERT(false);
    return false;
}


//-------------------------------------------------------------------------------------------------
// class SystemId

SystemId::SystemId()
{
}

SystemId::SystemId(std::string systemIdStr)
:
    systemId(std::move(systemIdStr))
{
}

MAKE_FIELD_NAME_STR_CONST(SystemId, systemId)

bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemId* const systemId)
{
    if (!urlQuery.hasQueryItem(SystemId_systemId_field))
        return false;
    systemId->systemId = urlQuery.queryItemValue(SystemId_systemId_field).toStdString();
    return true;
}

void serializeToUrlQuery(const SystemId& data, QUrlQuery* const urlQuery)
{
    urlQuery->addQueryItem(
        SystemId_systemId_field,
        QString::fromStdString(data.systemId));
}


//-------------------------------------------------------------------------------------------------
// class Filter

void serializeToUrlQuery(const Filter& data, QUrlQuery* const urlQuery)
{
    for (const auto& param: data.nameToValue)
    {
        urlQuery->addQueryItem(
            QnLexical::serialized(param.first),
            QString::fromStdString(param.second));
    }
}

void serialize(QnJsonContext* /*ctx*/, const Filter& filter, QJsonValue* target)
{
    QJsonObject localTarget = target->toObject();
    for (const auto& param: filter.nameToValue)
    {
        localTarget.insert(
            QnLexical::serialized(param.first),
            QString::fromStdString(param.second));
    }
    *target = localTarget;
}


//-------------------------------------------------------------------------------------------------
// class SystemAttributesUpdate

MAKE_FIELD_NAME_STR_CONST(SystemAttributesUpdate, systemId)
MAKE_FIELD_NAME_STR_CONST(SystemAttributesUpdate, name)
MAKE_FIELD_NAME_STR_CONST(SystemAttributesUpdate, opaque)

bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemAttributesUpdate* const data)
{
    return url::deserializeField(urlQuery, SystemAttributesUpdate_systemId_field, &data->systemId)
        && url::deserializeField(urlQuery, SystemAttributesUpdate_name_field, &data->name)
        && url::deserializeField(urlQuery, SystemAttributesUpdate_opaque_field, &data->opaque);
}

void serializeToUrlQuery(const SystemAttributesUpdate& data, QUrlQuery* const urlQuery)
{
    url::serializeField(urlQuery, SystemAttributesUpdate_systemId_field, data.systemId);
    url::serializeField(urlQuery, SystemAttributesUpdate_name_field, data.name);
    url::serializeField(urlQuery, SystemAttributesUpdate_opaque_field, data.opaque);
}

void serialize(QnJsonContext*, const SystemAttributesUpdate& data, QJsonValue* jsonValue)
{
    QJsonObject jsonObject;
    jsonObject.insert(
        SystemAttributesUpdate_systemId_field,
        QString::fromStdString(data.systemId));
    if (data.name)
        jsonObject.insert(
            SystemAttributesUpdate_name_field,
            QString::fromStdString(data.name.get()));
    if (data.opaque)
        jsonObject.insert(
            SystemAttributesUpdate_opaque_field,
            QString::fromStdString(data.opaque.get()));
    *jsonValue = jsonObject;
}

bool deserialize(QnJsonContext*, const QJsonValue& value, SystemAttributesUpdate* data)
{
    if (value.type() != QJsonValue::Object)
        return false;
    const QJsonObject map = value.toObject();

    auto systemIdIter = map.find(SystemAttributesUpdate_systemId_field);
    if (systemIdIter == map.constEnd())
        return false;
    data->systemId = systemIdIter.value().toString().toStdString();

    auto nameIter = map.find(SystemAttributesUpdate_name_field);
    if (nameIter != map.constEnd())
        data->name = nameIter.value().toString().toStdString();
    auto opaqueIter = map.find(SystemAttributesUpdate_opaque_field);
    if (opaqueIter != map.constEnd())
        data->opaque = opaqueIter.value().toString().toStdString();

    return data->name || data->opaque;
}

//-------------------------------------------------------------------------------------------------
// UserSessionDescriptor

MAKE_FIELD_NAME_STR_CONST(UserSessionDescriptor, accountEmail)
MAKE_FIELD_NAME_STR_CONST(UserSessionDescriptor, systemId)

bool loadFromUrlQuery(const QUrlQuery& urlQuery, UserSessionDescriptor* const data)
{
    url::deserializeField(urlQuery, UserSessionDescriptor_accountEmail_field, &data->accountEmail);
    url::deserializeField(urlQuery, UserSessionDescriptor_systemId_field, &data->systemId);
    return data->accountEmail || data->systemId;
}

void serializeToUrlQuery(const UserSessionDescriptor& data, QUrlQuery* const urlQuery)
{
    url::serializeField(urlQuery, UserSessionDescriptor_accountEmail_field, data.accountEmail);
    url::serializeField(urlQuery, UserSessionDescriptor_systemId_field, data.systemId);
}

void serialize(QnJsonContext*, const UserSessionDescriptor& data, QJsonValue* jsonValue)
{
    QJsonObject jsonObject;
    if (data.accountEmail)
        jsonObject.insert(
            UserSessionDescriptor_accountEmail_field,
            QString::fromStdString(data.accountEmail.get()));
    if (data.systemId)
        jsonObject.insert(
            UserSessionDescriptor_systemId_field,
            QString::fromStdString(data.systemId.get()));
    *jsonValue = jsonObject;
}

bool deserialize(QnJsonContext*, const QJsonValue& value, UserSessionDescriptor* data)
{
    if (value.type() != QJsonValue::Object)
        return false;
    const QJsonObject map = value.toObject();

    auto accountEmailIter = map.find(UserSessionDescriptor_accountEmail_field);
    if (accountEmailIter != map.constEnd())
        data->accountEmail = accountEmailIter.value().toString().toStdString();
    auto systemIdIter = map.find(UserSessionDescriptor_systemId_field);
    if (systemIdIter != map.constEnd())
        data->systemId = systemIdIter.value().toString().toStdString();

    return data->accountEmail || data->systemId;
}


QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SystemRegistrationData)(SystemData)(SystemSharing)(SystemId),
    (json),
    _Fields/*,
    (optional, false)*/)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SystemMergeInfo)(SystemDataEx)(SystemDataList)(SystemDataExList)(SystemSharingList) \
        (SystemSharingEx)(SystemSharingExList)(SystemAccessRoleData)(SystemAccessRoleList) \
        (SystemHealthHistoryItem)(SystemHealthHistory),
    (json),
    _Fields/*,
    (optional, false)*/)

} // namespace nx::cloud::db::api

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::cloud::db::api, SystemStatus,
    (nx::cloud::db::api::SystemStatus::invalid, "invalid")
    (nx::cloud::db::api::SystemStatus::notActivated, "notActivated")
    (nx::cloud::db::api::SystemStatus::activated, "activated")
    (nx::cloud::db::api::SystemStatus::deleted_, "deleted")
    (nx::cloud::db::api::SystemStatus::beingMerged, "beingMerged")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::cloud::db::api, SystemHealth,
    (nx::cloud::db::api::SystemHealth::offline, "offline")
    (nx::cloud::db::api::SystemHealth::online, "online")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::cloud::db::api, SystemAccessRole,
    (nx::cloud::db::api::SystemAccessRole::none, "none")
    (nx::cloud::db::api::SystemAccessRole::disabled, "disabled")
    (nx::cloud::db::api::SystemAccessRole::custom, "custom")
    (nx::cloud::db::api::SystemAccessRole::liveViewer, "liveViewer")
    (nx::cloud::db::api::SystemAccessRole::viewer, "viewer")
    (nx::cloud::db::api::SystemAccessRole::advancedViewer, "advancedViewer")
    (nx::cloud::db::api::SystemAccessRole::localAdmin, "localAdmin")
    (nx::cloud::db::api::SystemAccessRole::cloudAdmin, "cloudAdmin")
    (nx::cloud::db::api::SystemAccessRole::maintenance, "maintenance")
    (nx::cloud::db::api::SystemAccessRole::owner, "owner")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::cloud::db::api, FilterField,
    (nx::cloud::db::api::FilterField::customization, "customization")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::cloud::db::api, MergeRole,
    (nx::cloud::db::api::MergeRole::none, "none")
    (nx::cloud::db::api::MergeRole::master, "master")
    (nx::cloud::db::api::MergeRole::slave, "slave")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::cloud::db::api, SystemCapabilityFlag,
    (nx::cloud::db::api::SystemCapabilityFlag::cloudMerge, "cloudMerge")
)
