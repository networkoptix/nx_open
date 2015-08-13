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

bool loadFromUrlQuery( const QUrlQuery& urlQuery, AccountData* const accountData )
{
    accountData->id = QnUuid(urlQuery.queryItemValue( lit("id") ));
    accountData->email = urlQuery.queryItemValue( lit("email") ).toUtf8();
    accountData->passwordHa1 = urlQuery.queryItemValue( lit("passwordHa1") ).toUtf8();
    accountData->fullName = urlQuery.queryItemValue( lit("fullName") ).toUtf8();
    bool success = false;
    accountData->statusCode =
        QnLexical::deserialized<AccountStatus>( lit( "statusCode" ), asInvalid, &success );
    return success;
}


QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (AccountData)(EmailVerificationCode),
    (json)(sql_record),
    _Fields )


}   //data
}   //cdb
}   //nx
