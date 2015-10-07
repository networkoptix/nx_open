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

////////////////////////////////////////////////////////////
//// class AccountData
////////////////////////////////////////////////////////////

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(AccountStatus,
    (AccountStatus::invalid, "invalid")
    (AccountStatus::awaitingActivation, "awaitingEmailConfirmation")
    (AccountStatus::activated, "activated")
    (AccountStatus::blocked, "blocked")
    )


bool loadFromUrlQuery( const QUrlQuery& urlQuery, AccountData* const accountData )
{
    accountData->id = QnUuid(urlQuery.queryItemValue("id"));
    accountData->email = urlQuery.queryItemValue("email").toStdString();
    accountData->passwordHa1 = urlQuery.queryItemValue("passwordHa1").toStdString();
    accountData->fullName = urlQuery.queryItemValue("fullName").toStdString();
    accountData->customization = urlQuery.queryItemValue("customization").toStdString();
    bool success = true;
    accountData->statusCode = urlQuery.hasQueryItem("statusCode")
        ? QnLexical::deserialized<api::AccountStatus>(
            urlQuery.queryItemValue("statusCode"), api::AccountStatus::invalid, &success )
        : api::AccountStatus::invalid;

    success &= !accountData->email.empty();
    return success;
}

void serializeToUrlQuery(const AccountData& data, QUrlQuery* const urlQuery)
{
    urlQuery->addQueryItem("id", data.id.toString());
    urlQuery->addQueryItem("email", QString::fromStdString(data.email));
    urlQuery->addQueryItem("passwordHa1", QString::fromStdString(data.passwordHa1));
    urlQuery->addQueryItem("fullName", QString::fromStdString(data.fullName));
    urlQuery->addQueryItem("statusCode", QnLexical::serialized(data.statusCode));
    urlQuery->addQueryItem("customization", QString::fromStdString(data.customization));
}


////////////////////////////////////////////////////////////
//// class AccountActivationCode
////////////////////////////////////////////////////////////

bool loadFromUrlQuery(const QUrlQuery& urlQuery, AccountActivationCode* const data)
{
    if (!urlQuery.hasQueryItem("code"))
        return false;
    data->code = urlQuery.queryItemValue("code").toStdString();
    return true;
}

void serializeToUrlQuery(const AccountActivationCode& data, QUrlQuery* const urlQuery)
{
    urlQuery->addQueryItem("code", QString::fromStdString(data.code));
}


QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (AccountData)(AccountActivationCode),
    (json)(sql_record),
    _Fields )

}   //api
}   //cdb
}   //nx
