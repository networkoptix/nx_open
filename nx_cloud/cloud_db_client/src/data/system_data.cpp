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
    QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS( Value,
        (SystemAccessRole::none, "none")
        (SystemAccessRole::owner, "owner")
        (SystemAccessRole::maintenance, "maintenance")
        (SystemAccessRole::viewer, "viewer")
        (SystemAccessRole::editor, "editor")
        (SystemAccessRole::editorWithSharing, "editorWithSharing")
    )
}

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS( SystemStatus,
    (ssInvalid, "invalid")
    (ssNotActivated, "notActivated")
    (ssActivated, "activated")
)


////////////////////////////////////////////////////////////
//// class SystemRegistrationData
////////////////////////////////////////////////////////////

bool loadFromUrlQuery( const QUrlQuery& urlQuery, SystemRegistrationData* const systemData )
{
    if( !urlQuery.hasQueryItem("name") )
        return false;
    systemData->name = urlQuery.queryItemValue("name").toStdString();
    return true;
}


////////////////////////////////////////////////////////////
//// class SystemData
////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////
//// class SystemSharing
////////////////////////////////////////////////////////////


bool loadFromUrlQuery( const QUrlQuery& urlQuery, SystemSharing* const systemSharing )
{
    if (!urlQuery.hasQueryItem("systemID") || !urlQuery.hasQueryItem("accountID"))
        return false;

    systemSharing->systemID = QnUuid::fromStringSafe(urlQuery.queryItemValue("systemID"));
    systemSharing->accountID = QnUuid::fromStringSafe(urlQuery.queryItemValue("accountID"));
    bool success = false;
    systemSharing->accessRole = QnLexical::deserialized<api::SystemAccessRole::Value>(
        urlQuery.queryItemValue("accessRole"), api::SystemAccessRole::none, &success );
    return success;
}



QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SystemRegistrationData)(SystemData)(SystemSharing),
    (json)(sql_record),
    _Fields )

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SystemDataList),
    (json),
    _Fields )

}   //api
}   //cdb
}   //nx
