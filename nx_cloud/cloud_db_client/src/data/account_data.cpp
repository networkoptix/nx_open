/**********************************************************
* Sep 4, 2015
* a.kolesnikov
***********************************************************/

#include "account_data.h"

#include <common/common_globals.h>
#include <utils/common/model_functions.h>
#include <utils/network/buffer.h>


namespace nx {
namespace cdb {
namespace api {

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(AccountStatus,
    (asInvalid, "invalid")
    (asAwaitingEmailConfirmation, "awaitingEmailConfirmation")
    (asActivated, "activated")
    (asBlocked, "blocked")
    )


bool loadFromUrlQuery( const QUrlQuery& urlQuery, AccountData* const accountData )
{
    accountData->id = QnUuid(urlQuery.queryItemValue("id"));
    accountData->login = urlQuery.queryItemValue("login").toStdString();
    accountData->email = urlQuery.queryItemValue("email").toStdString();
    accountData->passwordHa1 = urlQuery.queryItemValue("passwordHa1").toStdString();
    accountData->fullName = urlQuery.queryItemValue("fullName").toStdString();
    bool success = true;
    accountData->statusCode = urlQuery.hasQueryItem("statusCode")
        ? QnLexical::deserialized<api::AccountStatus>(
            urlQuery.queryItemValue("statusCode"), api::asInvalid, &success )
        : api::asInvalid;
    return success;
}

void serializeToUrlQuery(const AccountData& data, QUrlQuery* const urlQuery)
{
    urlQuery->addQueryItem("id", data.id.toString());
    urlQuery->addQueryItem("login", QString::fromStdString(data.login));
    urlQuery->addQueryItem("email", QString::fromStdString(data.email));
    urlQuery->addQueryItem("passwordHa1", QString::fromStdString(data.passwordHa1));
    urlQuery->addQueryItem("fullName", QString::fromStdString(data.fullName));
    urlQuery->addQueryItem("statusCode", QnLexical::serialized(data.statusCode));
}


QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (AccountData),
    (json)(sql_record),
    _Fields )


}   //api
}   //cdb
}   //nx
