/**********************************************************
* Aug 26, 2015
* a.kolesnikov
***********************************************************/

#include "system_data.h"

#include <utils/common/model_functions.h>

#include "cdb_ns.h"


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
}   //api

namespace data {


////////////////////////////////////////////////////////////
//// class SystemRegistrationData
////////////////////////////////////////////////////////////

bool SystemRegistrationData::getAsVariant( int /*resID*/, QVariant* const /*value*/ ) const
{
    return false;
}

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

bool SystemData::getAsVariant( int resID, QVariant* const value ) const
{
    switch( resID )
    {
        case param::accountID:
            *value = QVariant::fromValue(ownerAccountID);
            return true;
        case param::systemID:
            *value = QVariant::fromValue(id);
            return true;
    }

    //TODO #ak
    return false;
}


////////////////////////////////////////////////////////////
//// class SystemSharing
////////////////////////////////////////////////////////////

bool SystemSharing::getAsVariant( int /*resID*/, QVariant* const /*value*/ ) const
{
    //TODO #ak
    return false;
}

bool loadFromUrlQuery( const QUrlQuery& urlQuery, SystemSharing* const systemSharing )
{
    if (!urlQuery.hasQueryItem("systemID") || !urlQuery.hasQueryItem("accountID"))
        return false;

    systemSharing->systemID = QnUuid::fromStringSafe(urlQuery.queryItemValue("systemID"));
    systemSharing->accountID = QnUuid::fromStringSafe(urlQuery.queryItemValue("accountID"));
    bool success = false;
    systemSharing->accessRole = QnLexical::deserialized<api::SystemAccessRole::Value>(
        urlQuery.queryItemValue(lit("accessRole")), api::SystemAccessRole::none, &success );
    return success;
}


////////////////////////////////////////////////////////////
//// class SystemID
////////////////////////////////////////////////////////////

bool SystemID::getAsVariant(int resID, QVariant* const value) const
{
    switch (resID)
    {
        case param::systemID:
            *value = QVariant::fromValue(id);
            return true;
        default:
            return false;
    }
}

bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemID* const systemID)
{
    if (!urlQuery.hasQueryItem("id"))
        return false;
    systemID->id = urlQuery.queryItemValue("id");
    return true;
}


QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SystemRegistrationData)(SystemData)(SystemSharing)(SystemID),
    (json)(sql_record),
    _Fields )

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SystemDataList),
    (json),
    _Fields )

}   //data
}   //cdb
}   //nx
