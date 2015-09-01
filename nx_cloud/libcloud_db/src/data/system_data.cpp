/**********************************************************
* Aug 26, 2015
* a.kolesnikov
***********************************************************/

#include "system_data.h"

#include <utils/common/model_functions.h>


namespace nx {
namespace cdb {
namespace data {

namespace SystemAccessRole
{
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


bool SystemData::getAsVariant( int /*resID*/, QVariant* const /*value*/ ) const
{
    //TODO #ak
    return false;
}


bool SystemSharing::getAsVariant( int /*resID*/, QVariant* const /*value*/ ) const
{
    //TODO #ak
    return false;
}

bool loadFromUrlQuery( const QUrlQuery& urlQuery, SystemSharing* const systemSharing )
{
    systemSharing->systemID = urlQuery.queryItemValue("systemID").toStdString();
    systemSharing->accountID = urlQuery.queryItemValue("accountID").toStdString();
    bool success = false;
    systemSharing->accessRole = QnLexical::deserialized<SystemAccessRole::Value>(
        urlQuery.queryItemValue(lit("accessRole")), SystemAccessRole::none, &success );
    return success && !systemSharing->systemID.empty() && !systemSharing->accountID.empty();
}


QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SystemRegistrationData)(SystemData)(SystemSharing),
    (json)(sql_record),
    _Fields )

}   //data
}   //cdb
}   //nx
