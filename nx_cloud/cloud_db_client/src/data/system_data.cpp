/**********************************************************
* Sep 4, 2015
* a.kolesnikov
***********************************************************/

#include "system_data.h"

#include <common/common_globals.h>
#include <utils/common/model_functions.h>
#include <utils/preprocessor/field_name.h>


namespace nx {
namespace cdb {
namespace api {

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(SystemStatus,
    (ssInvalid, "invalid")
    (ssNotActivated, "notActivated")
    (ssActivated, "activated"));


////////////////////////////////////////////////////////////
//// class SystemRegistrationData
////////////////////////////////////////////////////////////

MAKE_FIELD_NAME_STR_CONST(SystemRegistrationData, name)
MAKE_FIELD_NAME_STR_CONST(SystemRegistrationData, customization)

bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemRegistrationData* const systemData)
{
    if (!urlQuery.hasQueryItem(SystemRegistrationData_name_field) ||
        !urlQuery.hasQueryItem(SystemRegistrationData_customization_field))
    {
        return false;
    }
    systemData->name =
        urlQuery.queryItemValue(SystemRegistrationData_name_field).toStdString();
    systemData->customization =
        urlQuery.queryItemValue(SystemRegistrationData_customization_field).toStdString();
    return true;
}

void serializeToUrlQuery(const SystemRegistrationData& data, QUrlQuery* const urlQuery)
{
    urlQuery->addQueryItem(
        SystemRegistrationData_name_field,
        data.name.c_str());
    urlQuery->addQueryItem(
        SystemRegistrationData_customization_field,
        data.customization.c_str());
}


////////////////////////////////////////////////////////////
//// system sharing data
////////////////////////////////////////////////////////////

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(SystemAccessRole,
    (SystemAccessRole::none, "none")
    (SystemAccessRole::liveViewer, "liveViewer")
    (SystemAccessRole::viewer, "viewer")
    (SystemAccessRole::advancedViewer, "advancedViewer")
    (SystemAccessRole::localAdmin, "localAdmin")
    (SystemAccessRole::cloudAdmin, "cloudAdmin")
    (SystemAccessRole::maintenance, "maintenance")
    (SystemAccessRole::owner, "owner"));

////////////////////////////////////////////////////////////
//// class SystemSharing
////////////////////////////////////////////////////////////

MAKE_FIELD_NAME_STR_CONST(SystemSharing, systemID)
MAKE_FIELD_NAME_STR_CONST(SystemSharing, accountEmail)
MAKE_FIELD_NAME_STR_CONST(SystemSharing, accessRole)

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


QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SystemRegistrationData)(SystemData)(SystemSharing)(SystemID),
    (json),
    _Fields);

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SystemDataEx)(SystemDataList)(SystemDataExList)(SystemSharingList)(SystemSharingEx) \
        (SystemSharingExList)(SystemAccessRoleData)(SystemAccessRoleList),
    (json),
    _Fields);

}   //api
}   //cdb
}   //nx
