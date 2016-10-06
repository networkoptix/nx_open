/**********************************************************
* Sep 4, 2015
* a.kolesnikov
***********************************************************/

#include "system_data.h"

#include <common/common_globals.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/url/query_parse_helper.h>
#include <utils/preprocessor/field_name.h>

namespace nx {
namespace cdb {
namespace api {

using namespace nx::network;

////////////////////////////////////////////////////////////
//// class SystemRegistrationData
////////////////////////////////////////////////////////////

MAKE_FIELD_NAME_STR_CONST(SystemRegistrationData, name)
MAKE_FIELD_NAME_STR_CONST(SystemRegistrationData, customization)

bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemRegistrationData* const data)
{
    return 
        url::deserializeField(
            urlQuery,
            SystemRegistrationData_name_field,
            &data->name)
        &&
        url::deserializeField(
            urlQuery,
            SystemRegistrationData_customization_field,
            &data->customization);
}

void serializeToUrlQuery(const SystemRegistrationData& data, QUrlQuery* const urlQuery)
{
    url::serializeField(urlQuery, SystemRegistrationData_name_field, data.name);
    url::serializeField(urlQuery, SystemRegistrationData_customization_field, data.customization);
}

////////////////////////////////////////////////////////////
//// class SystemSharing
////////////////////////////////////////////////////////////

MAKE_FIELD_NAME_STR_CONST(SystemSharing, systemID)
MAKE_FIELD_NAME_STR_CONST(SystemSharing, accountEmail)
MAKE_FIELD_NAME_STR_CONST(SystemSharing, accessRole)
MAKE_FIELD_NAME_STR_CONST(SystemSharing, groupID)
MAKE_FIELD_NAME_STR_CONST(SystemSharing, customPermissions)
MAKE_FIELD_NAME_STR_CONST(SystemSharing, isEnabled)

bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemSharing* const systemSharing)
{
    if (!urlQuery.hasQueryItem(SystemSharing_systemID_field) ||
        !urlQuery.hasQueryItem(SystemSharing_accountEmail_field))
    {
        return false;
    }

    systemSharing->systemID = urlQuery.queryItemValue(SystemSharing_systemID_field).toStdString();
    systemSharing->accountEmail =
        urlQuery.queryItemValue(SystemSharing_accountEmail_field).toStdString();
    bool success = false;
    systemSharing->accessRole = QnLexical::deserialized<api::SystemAccessRole>(
        urlQuery.queryItemValue(SystemSharing_accessRole_field),
        api::SystemAccessRole::none,
        &success);
    systemSharing->groupID =
        urlQuery.queryItemValue(SystemSharing_groupID_field).toStdString();
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
        SystemSharing_systemID_field,
        QString::fromStdString(data.systemID));
    urlQuery->addQueryItem(
        SystemSharing_accountEmail_field,
        QString::fromStdString(data.accountEmail));
    urlQuery->addQueryItem(
        SystemSharing_accessRole_field,
        QnLexical::serialized(data.accessRole));
    urlQuery->addQueryItem(
        SystemSharing_groupID_field,
        QString::fromStdString(data.groupID));
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
    NX_EXPECT(false);
    return false;
}


////////////////////////////////////////////////////////////
//// class SystemID
////////////////////////////////////////////////////////////

SystemID::SystemID()
{
}

SystemID::SystemID(std::string systemIDStr)
:
    systemID(std::move(systemIDStr))
{
}

MAKE_FIELD_NAME_STR_CONST(SystemID, systemID)

bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemID* const systemID)
{
    if (!urlQuery.hasQueryItem(SystemID_systemID_field))
        return false;
    systemID->systemID = urlQuery.queryItemValue(SystemID_systemID_field).toStdString();
    return true;
}

void serializeToUrlQuery(const SystemID& data, QUrlQuery* const urlQuery)
{
    urlQuery->addQueryItem(
        SystemID_systemID_field,
        QString::fromStdString(data.systemID));
}


////////////////////////////////////////////////////////////
//// class SystemNameUpdate
////////////////////////////////////////////////////////////

MAKE_FIELD_NAME_STR_CONST(SystemNameUpdate, systemID)
MAKE_FIELD_NAME_STR_CONST(SystemNameUpdate, name)

bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemNameUpdate* const data)
{
    return url::deserializeField(urlQuery, SystemNameUpdate_systemID_field, &data->systemID)
        && url::deserializeField(urlQuery, SystemNameUpdate_name_field, &data->name);
}

void serializeToUrlQuery(const SystemNameUpdate& data, QUrlQuery* const urlQuery)
{
    url::serializeField(urlQuery, SystemNameUpdate_systemID_field, data.systemID);
    url::serializeField(urlQuery, SystemNameUpdate_name_field, data.name);
}

//-----------------------------------------------------
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
    (SystemRegistrationData)(SystemData)(SystemSharing)(SystemID)(SystemNameUpdate),
    (json),
    _Fields/*,
    (optional, false)*/)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SystemDataEx)(SystemDataList)(SystemDataExList)(SystemSharingList)(SystemSharingEx) \
        (SystemSharingExList)(SystemAccessRoleData)(SystemAccessRoleList),
    (json),
    _Fields/*,
    (optional, false)*/)

}   //api
}   //cdb
}   //nx

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::cdb::api, SystemStatus,
    (nx::cdb::api::SystemStatus::ssInvalid, "invalid")
    (nx::cdb::api::SystemStatus::ssNotActivated, "notActivated")
    (nx::cdb::api::SystemStatus::ssActivated, "activated")
    (nx::cdb::api::SystemStatus::ssDeleted, "deleted")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::cdb::api, SystemHealth,
    (nx::cdb::api::SystemHealth::offline, "offline")
    (nx::cdb::api::SystemHealth::online, "online")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::cdb::api, SystemAccessRole,
    (nx::cdb::api::SystemAccessRole::none, "none")
    (nx::cdb::api::SystemAccessRole::disabled, "disabled")
    (nx::cdb::api::SystemAccessRole::custom, "custom")
    (nx::cdb::api::SystemAccessRole::liveViewer, "liveViewer")
    (nx::cdb::api::SystemAccessRole::viewer, "viewer")
    (nx::cdb::api::SystemAccessRole::advancedViewer, "advancedViewer")
    (nx::cdb::api::SystemAccessRole::localAdmin, "localAdmin")
    (nx::cdb::api::SystemAccessRole::cloudAdmin, "cloudAdmin")
    (nx::cdb::api::SystemAccessRole::maintenance, "maintenance")
    (nx::cdb::api::SystemAccessRole::owner, "owner")
)
