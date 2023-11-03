// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_data.h"

#include <nx/network/url/query_parse_helper.h>
#include <nx/utils/log/assert.h>

#include "../field_name.h"

namespace nx::cloud::db::api {

using namespace nx::network;

//-------------------------------------------------------------------------------------------------
// class SystemRegistrationData

MAKE_FIELD_NAME_STR_CONST(SystemRegistrationData, id)
MAKE_FIELD_NAME_STR_CONST(SystemRegistrationData, name)
MAKE_FIELD_NAME_STR_CONST(SystemRegistrationData, customization)
MAKE_FIELD_NAME_STR_CONST(SystemRegistrationData, opaque)

bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemRegistrationData* const data)
{
    if (urlQuery.hasQueryItem(SystemRegistrationData_id_field))
    {
        data->id = std::string();
        url::deserializeField(urlQuery, SystemRegistrationData_opaque_field, &data->id.value());
    }

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
    if (data.id)
        url::serializeField(urlQuery, SystemRegistrationData_id_field, *data.id);
    url::serializeField(urlQuery, SystemRegistrationData_name_field, data.name);
    url::serializeField(urlQuery, SystemRegistrationData_customization_field, data.customization);
    url::serializeField(urlQuery, SystemRegistrationData_opaque_field, data.opaque);
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
// SystemDataEx

namespace {

template<typename T>
static void writeJsonAttribute(
    nx::reflect::json::SerializationContext* ctx,
    const std::string& name,
    const T& value)
{
    ctx->composer.writeAttributeName(name);
    nx::reflect::json::serialize(ctx, value);
}

template<typename T>
static void writeJsonAttribute(
    nx::reflect::json::SerializationContext* ctx,
    const std::string& name,
    const std::optional<T>& value)
{
    if (!value)
        return;

    ctx->composer.writeAttributeName(name);
    nx::reflect::json::serialize(ctx, *value);
}

} // namespace

//-------------------------------------------------------------------------------------------------
// SystemData & SystemDataEx

template<typename T>
requires std::is_base_of_v<SystemData, T>
static void serializeFlattenningAttrs(
    nx::reflect::json::SerializationContext* ctx,
    const T& value)
{
    ctx->composer.startObject();

    nx::reflect::forEachField<T>(
        [ctx, &value](const auto& field)
        {
            if (nx::reflect::isSameField(field, &SystemData::attributes))
            {
                // Serializing "attributes" values on the top level of the JSON document.
                for (const auto& attr: value.attributes)
                    writeJsonAttribute(ctx, attr.name, attr.value);
            }
            else
            {
                // Serializing each field but "attributes" as usual.
                writeJsonAttribute(ctx, field.name(), field.get(value));
            }
        });

    ctx->composer.endObject();
}

template<typename T>
requires std::is_base_of_v<SystemData, T>
static nx::reflect::DeserializationResult deserializeFlattenningAttrs(
    const nx::reflect::json::DeserializationContext& ctx,
    T* value)
{
    nx::reflect::DeserializationResult result;

    std::set<typename std::decay_t<decltype(ctx.value)>::ConstMemberIterator> readMembers;

    // Deserializing all fields but "attributes" as usual.
    nx::reflect::forEachField<T>(
        [&ctx, value, &result, &readMembers](const auto& field)
        {
            if (!result)
                return; //< Error happened earlier. Skipping...

            if (nx::reflect::isSameField(field, &SystemData::attributes))
                return; //< Attributes will be parsed later.

            auto it = ctx.value.FindMember(field.name());
            if (it != ctx.value.MemberEnd())
            {
                typename std::decay_t<decltype(field)>::Type val;
                result = nx::reflect::json::deserialize(
                    nx::reflect::json::DeserializationContext{it->value, ctx.flags}, &val);
                readMembers.insert(it);
                if (result)
                    field.set(value, val);
            }
        });

    if (!result)
        return result;

    // Loading all remaining string members into "attributes".
    value->attributes.reserve(ctx.value.MemberCount() - readMembers.size());
    for (auto it = ctx.value.MemberBegin(); it != ctx.value.MemberEnd(); ++it)
    {
        if (!it->value.IsString())
            continue;

        if (!readMembers.contains(it))
        {
            Attribute attr;
            attr.name.assign(it->name.GetString(), it->name.GetStringLength());
            attr.value.assign(it->value.GetString(), it->value.GetStringLength());
            value->attributes.push_back(std::move(attr));
        }
    }

    return result;
}

void serialize(
    nx::reflect::json::SerializationContext* ctx,
    const SystemData& value)
{
    serializeFlattenningAttrs(ctx, value);
}

nx::reflect::DeserializationResult deserialize(
    const nx::reflect::json::DeserializationContext& ctx,
    SystemData* value)
{
    return deserializeFlattenningAttrs(ctx, value);
}

void serialize(
    nx::reflect::json::SerializationContext* ctx,
    const SystemDataEx& value)
{
    serializeFlattenningAttrs(ctx, value);
}

nx::reflect::DeserializationResult deserialize(
    const nx::reflect::json::DeserializationContext& ctx,
    SystemDataEx* value)
{
    return deserializeFlattenningAttrs(ctx, value);
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

//-------------------------------------------------------------------------------------------------

std::string getAttrValueOr(
    const AttributesList& attrs, const std::string& name, const std::string& defaultV)
{
    for (const auto& attr: attrs)
    {
        if (attr.name == name)
            return attr.value;
    }

    return defaultV;
}

} // namespace nx::cloud::db::api
