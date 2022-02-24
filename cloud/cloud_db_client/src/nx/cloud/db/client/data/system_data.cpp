// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
    systemSharing->accessRole = nx::reflect::fromString(
        urlQuery.queryItemValue(SystemSharing_accessRole_field).toStdString(),
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
        QString::fromStdString(nx::reflect::toString(data.accessRole)));
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
// struct SystemIdList

void serializeToUrlQuery(const SystemIdList& data, QUrlQuery* const urlQuery)
{
    for (const auto& systemId : data.systemIds)
        serializeToUrlQuery(api::SystemId(systemId), urlQuery);
}

//-------------------------------------------------------------------------------------------------
// class Filter

void serializeToUrlQuery(const Filter& data, QUrlQuery* const urlQuery)
{
    for (const auto& param: data.nameToValue)
    {
        urlQuery->addQueryItem(
            QString::fromStdString(nx::reflect::toString(param.first)),
            QString::fromStdString(param.second));
    }
}

void serialize(QnJsonContext* /*ctx*/, const Filter& filter, QJsonValue* target)
{
    QJsonObject localTarget = target->toObject();
    for (const auto& param: filter.nameToValue)
    {
        localTarget.insert(
            QString::fromStdString(nx::reflect::toString(param.first)),
            QString::fromStdString(param.second));
    }
    *target = localTarget;
}


//-------------------------------------------------------------------------------------------------
// class SystemAttributesUpdate

MAKE_FIELD_NAME_STR_CONST(SystemAttributesUpdate, systemId)
MAKE_FIELD_NAME_STR_CONST(SystemAttributesUpdate, name)
MAKE_FIELD_NAME_STR_CONST(SystemAttributesUpdate, opaque)
MAKE_FIELD_NAME_STR_CONST(SystemAttributesUpdate, system2faEnabled)
MAKE_FIELD_NAME_STR_CONST(SystemAttributesUpdate, totp)

bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemAttributesUpdate* const data)
{
    if (urlQuery.hasQueryItem(SystemAttributesUpdate_system2faEnabled_field))
        data->system2faEnabled =
            urlQuery.queryItemValue(SystemAttributesUpdate_system2faEnabled_field).toInt();
    if (urlQuery.hasQueryItem(SystemAttributesUpdate_totp_field))
        data->totp =
            urlQuery.queryItemValue(SystemAttributesUpdate_totp_field).toStdString();

    return url::deserializeField(urlQuery, SystemAttributesUpdate_systemId_field, &data->systemId)
        && url::deserializeField(urlQuery, SystemAttributesUpdate_name_field, &data->name)
        && url::deserializeField(urlQuery, SystemAttributesUpdate_opaque_field, &data->opaque);
}

void serializeToUrlQuery(const SystemAttributesUpdate& data, QUrlQuery* const urlQuery)
{
    url::serializeField(urlQuery, SystemAttributesUpdate_systemId_field, data.systemId);
    url::serializeField(urlQuery, SystemAttributesUpdate_name_field, data.name);
    if (data.system2faEnabled)
        urlQuery->addQueryItem(
            SystemAttributesUpdate_system2faEnabled_field,
            QString::number(*data.system2faEnabled));
    url::serializeField(urlQuery, SystemAttributesUpdate_opaque_field, data.opaque);
    url::serializeField(urlQuery, SystemAttributesUpdate_totp_field, data.totp);
}

void serialize(QnJsonContext*, const SystemAttributesUpdate& data, QJsonValue* jsonValue)
{
    QJsonObject jsonObject;

    if (data.systemId)
    {
        jsonObject.insert(
            SystemAttributesUpdate_systemId_field, QString::fromStdString(*data.systemId));
    }

    if (data.name)
    {
        jsonObject.insert(SystemAttributesUpdate_name_field, QString::fromStdString(*data.name));
    }

    if (data.opaque)
    {
        jsonObject.insert(
            SystemAttributesUpdate_opaque_field, QString::fromStdString(*data.opaque));
    }

    if (data.system2faEnabled)
    {
        jsonObject.insert(
            SystemAttributesUpdate_system2faEnabled_field,
            *data.system2faEnabled ? "true" : "false");
    }

    if (data.totp)
    {
        jsonObject.insert(SystemAttributesUpdate_totp_field, QString::fromStdString(*data.totp));
    }

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
    auto system2faIter = map.find(SystemAttributesUpdate_system2faEnabled_field);
    if (system2faIter != map.constEnd())
        data->system2faEnabled = system2faIter.value().toInt();
    auto totpIter = map.find(SystemAttributesUpdate_totp_field);
    if (totpIter != map.constEnd())
        data->totp = totpIter.value().toString().toStdString();

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
    {
        jsonObject.insert(
            UserSessionDescriptor_accountEmail_field,
            QString::fromStdString(*data.accountEmail));
    }

    if (data.systemId)
    {
        jsonObject.insert(
            UserSessionDescriptor_systemId_field,
            QString::fromStdString(*data.systemId));
    }

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

//-------------------------------------------------------------------------------------------------
// GetSystemUsersRequest

MAKE_FIELD_NAME_STR_CONST(GetSystemUsersRequest, systemId)
MAKE_FIELD_NAME_STR_CONST(GetSystemUsersRequest, localOnly)

void serializeToUrlQuery(const GetSystemUsersRequest& data, QUrlQuery* const urlQuery)
{
    urlQuery->addQueryItem(
        GetSystemUsersRequest_systemId_field,
        QString::fromStdString(data.systemId));
    urlQuery->addQueryItem(
        GetSystemUsersRequest_localOnly_field,
        QString::number(data.localOnly));
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SystemRegistrationData, (json), SystemRegistrationData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SystemData, (json), SystemData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SystemSharing, (json), SystemSharing_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SystemId, (json), SystemId_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SystemIdList, (json), SystemIdList_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SystemMergeInfo, (json), SystemMergeInfo_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SystemDataEx, (json), SystemDataEx_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SystemDataList, (json), SystemDataList_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SystemDataExList, (json), SystemDataExList_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SystemSharingList, (json), SystemSharingList_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SystemSharingEx, (ubjson)(json), SystemSharingEx_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SystemSharingExList, (json), SystemSharingExList_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SystemAccessRoleData, (json), SystemAccessRoleData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SystemAccessRoleList, (json), SystemAccessRoleList_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SystemHealthHistoryItem, (json), SystemHealthHistoryItem_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SystemHealthHistory, (json), SystemHealthHistory_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(MergeRequest, (json), MergeRequest_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ValidateMSSignatureRequest, (json), ValidateMSSignatureRequest_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(GetSystemUsersRequest, (json), GetSystemUsersRequest_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(CreateSystemOfferRequest, (json), CreateSystemOfferRequest_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SystemOffer, (json), SystemOffer_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SystemOfferPatch, (json), SystemOfferPatch_Fields)

} // namespace nx::cloud::db::api
