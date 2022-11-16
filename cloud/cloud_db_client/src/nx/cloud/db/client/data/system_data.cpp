// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_data.h"

#include <nx/network/url/query_parse_helper.h>
#include <nx/utils/log/assert.h>

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

void serialize(
    nx::reflect::json::SerializationContext* ctx,
    const Filter& value)
{
    nx::reflect::json::serialize(ctx, value.nameToValue);
}

//-------------------------------------------------------------------------------------------------
// class SystemAttributesUpdate

MAKE_FIELD_NAME_STR_CONST(SystemAttributesUpdate, systemId)
MAKE_FIELD_NAME_STR_CONST(SystemAttributesUpdate, name)
MAKE_FIELD_NAME_STR_CONST(SystemAttributesUpdate, opaque)
MAKE_FIELD_NAME_STR_CONST(SystemAttributesUpdate, system2faEnabled)
MAKE_FIELD_NAME_STR_CONST(SystemAttributesUpdate, totp)
MAKE_FIELD_NAME_STR_CONST(SystemAttributesUpdate, mfaCode)

bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemAttributesUpdate* const data)
{
    if (urlQuery.hasQueryItem(SystemAttributesUpdate_system2faEnabled_field))
        data->system2faEnabled =
            urlQuery.queryItemValue(SystemAttributesUpdate_system2faEnabled_field).toInt();
    if (urlQuery.hasQueryItem(SystemAttributesUpdate_totp_field))
        data->totp =
            urlQuery.queryItemValue(SystemAttributesUpdate_totp_field).toStdString();
    if (urlQuery.hasQueryItem(SystemAttributesUpdate_mfaCode_field))
        data->mfaCode =
            urlQuery.queryItemValue(SystemAttributesUpdate_mfaCode_field).toStdString();

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
    url::serializeField(urlQuery, SystemAttributesUpdate_mfaCode_field, data.mfaCode);
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

} // namespace nx::cloud::db::api
