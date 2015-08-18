/**********************************************************
* Aug 18, 2015
* a.kolesnikov
***********************************************************/

#include "email_verification_code.h"

#include <utils/common/model_functions.h>


namespace nx {
namespace cdb {
namespace data {


bool EmailVerificationCode::getAsVariant( int /*resID*/, QVariant* const /*value*/ ) const
{
    //TODO #ak
    return false;
}

bool loadFromUrlQuery( const QUrlQuery& urlQuery, EmailVerificationCode* const data )
{
    if( !urlQuery.hasQueryItem( "code" ) )
        return false;
    data->code = urlQuery.queryItemValue( "code" ).toStdString();
    return true;
}


QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (EmailVerificationCode),
    (json)(sql_record),
    _Fields )


}   //data
}   //cdb
}   //nx
