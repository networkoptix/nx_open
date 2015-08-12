/**********************************************************
* Aug 6, 2015
* a.kolesnikov
***********************************************************/

#include "account_data.h"

#include <utils/common/model_functions.h>


namespace nx {
namespace cdb {
namespace data {

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(AccountStatus,
    (asInvalid,  "invalid")
    (asAwaitingEmailConfirmation, "awaitingEmailConfirmation")
    (asActivated,    "activated")
    (asBlocked,   "blocked")
)


bool AccountData::getAsVariant( int resID, QVariant* const value ) const
{
    //TODO #ak
    return false;
}

void fromUrlQuery( const QUrlQuery& urlQuery, AccountData* const accountData )
{
    //TODO #ak
}


QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (AccountData)(EmailVerificationCode),
    (json)(sql_record),
    _Fields )


}   //data
}   //cdb
}   //nx
