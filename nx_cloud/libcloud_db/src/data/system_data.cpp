/**********************************************************
* Aug 26, 2015
* a.kolesnikov
***********************************************************/

#include "system_data.h"

#include <utils/common/model_functions.h>


namespace nx {
namespace cdb {
namespace data {

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
    return false;
}


QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SystemRegistrationData)(SystemData),
    (json)(sql_record),
    _Fields )

}   //data
}   //cdb
}   //nx
