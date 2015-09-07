/**********************************************************
* Sep 4, 2015
* a.kolesnikov
***********************************************************/

#include "system_data.h"

#include <common/common_globals.h>
#include <utils/common/model_functions.h>


namespace nx {
namespace cdb {
namespace api {

namespace SystemAccessRole {
QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Value,
    (SystemAccessRole::none, "none")
    (SystemAccessRole::owner, "owner")
    (SystemAccessRole::maintenance, "maintenance")
    (SystemAccessRole::viewer, "viewer")
    (SystemAccessRole::editor, "editor")
    (SystemAccessRole::editorWithSharing, "editorWithSharing"));
}

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(SystemStatus,
    (ssInvalid, "invalid")
    (ssNotActivated, "notActivated")
    (ssActivated, "activated"));


////////////////////////////////////////////////////////////
//// class SystemRegistrationData
////////////////////////////////////////////////////////////

bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemRegistrationData* const systemData)
{
    if (!urlQuery.hasQueryItem("name"))
        return false;
    systemData->name = urlQuery.queryItemValue("name").toStdString();
    return true;
}

void serializeToUrlQuery(const SystemRegistrationData& data, QUrlQuery* const urlQuery)
{
    urlQuery->addQueryItem("name", data.name.c_str());
}


////////////////////////////////////////////////////////////
//// class SystemSharing
////////////////////////////////////////////////////////////

bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemSharing* const systemSharing)
{
    if (!urlQuery.hasQueryItem("systemID") || !urlQuery.hasQueryItem("accountID"))
        return false;

    systemSharing->systemID = QnUuid::fromStringSafe(urlQuery.queryItemValue("systemID"));
    systemSharing->accountID = QnUuid::fromStringSafe(urlQuery.queryItemValue("accountID"));
    bool success = false;
    systemSharing->accessRole = QnLexical::deserialized<api::SystemAccessRole::Value>(
        urlQuery.queryItemValue("accessRole"), api::SystemAccessRole::none, &success);
    return success;
}

void serializeToUrlQuery(const SystemSharing& data, QUrlQuery* const urlQuery)
{
    urlQuery->addQueryItem("systemID", data.systemID.toString());
    urlQuery->addQueryItem("accountID", data.accountID.toString());
    urlQuery->addQueryItem("accessRole", QnLexical::serialized(data.accessRole));
}


////////////////////////////////////////////////////////////
//// class SystemID
////////////////////////////////////////////////////////////

bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemID* const systemID)
{
    if (!urlQuery.hasQueryItem("id"))
        return false;
    systemID->id = urlQuery.queryItemValue("id");
    return true;
}

void serializeToUrlQuery(const SystemID& data, QUrlQuery* const urlQuery)
{
    urlQuery->addQueryItem("id", data.id.toString());
}


QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SystemRegistrationData)(SystemData)(SystemSharing)(SystemID),
    (json)(sql_record),
    _Fields);

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SystemDataList),
    (json),
    _Fields);

}   //api
}   //cdb
}   //nx
