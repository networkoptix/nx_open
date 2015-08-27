/**********************************************************
* Aug 6, 2015
* a.kolesnikov
***********************************************************/

#include "account_data.h"

#include <utils/common/model_functions.h>
#include <utils/network/buffer.h>

#include "cdb_ns.h"


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
    switch( resID )
    {
        case param::accountID:
            *value = QVariant::fromValue(id);
            return true;

        default:
            return false;
    }
}

bool loadFromUrlQuery( const QUrlQuery& urlQuery, AccountData* const accountData )
{
    accountData->id = QnUuid(urlQuery.queryItemValue( lit("id") ));
    accountData->login = urlQuery.queryItemValue( lit("login") ).toStdString();
    accountData->email = urlQuery.queryItemValue( lit("email") ).toStdString();
    accountData->passwordHa1 = urlQuery.queryItemValue( lit("passwordHa1") ).toStdString();
    accountData->fullName = urlQuery.queryItemValue( lit("fullName") ).toStdString();
    bool success = true;
    accountData->statusCode = urlQuery.hasQueryItem( lit( "statusCode" ) )
        ? QnLexical::deserialized<AccountStatus>(
            urlQuery.queryItemValue( lit( "statusCode" ) ), asInvalid, &success )
        : asInvalid;
    return success;
}


QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (AccountData),
    (json)(sql_record),
    _Fields )


}   //data
}   //cdb
}   //nx
