/**********************************************************
* Sep 4, 2015
* a.kolesnikov
***********************************************************/

#include "account_data.h"

#include <common/common_globals.h>
#include <utils/common/model_functions.h>
#include <utils/network/buffer.h>
#include <utils/preprocessor/field_name.h>


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


MAKE_FIELD_NAME_STR_CONST(AccountData, id)
MAKE_FIELD_NAME_STR_CONST(AccountData, email)
MAKE_FIELD_NAME_STR_CONST(AccountData, passwordHa1)
MAKE_FIELD_NAME_STR_CONST(AccountData, fullName)
MAKE_FIELD_NAME_STR_CONST(AccountData, customization)
MAKE_FIELD_NAME_STR_CONST(AccountData, statusCode)

bool loadFromUrlQuery( const QUrlQuery& urlQuery, AccountData* const accountData )
{
    accountData->id = QnUuid(urlQuery.queryItemValue(AccountData_id_field));
    accountData->email = urlQuery.queryItemValue(AccountData_email_field).toStdString();
    accountData->passwordHa1 = urlQuery.queryItemValue(AccountData_passwordHa1_field).toStdString();
    accountData->fullName = urlQuery.queryItemValue(AccountData_fullName_field).toStdString();
    accountData->customization = urlQuery.queryItemValue(AccountData_customization_field).toStdString();
    bool success = true;
    if (urlQuery.hasQueryItem(AccountData_statusCode_field))
        accountData->statusCode = QnLexical::deserialized<api::AccountStatus>(
            urlQuery.queryItemValue(AccountData_statusCode_field),
            api::AccountStatus::invalid,
            &success);
    else
        accountData->statusCode = api::AccountStatus::invalid;

    success &= !accountData->email.empty();
    return success;
}

void serializeToUrlQuery(const AccountData& data, QUrlQuery* const urlQuery)
{
    urlQuery->addQueryItem(
        AccountData_id_field,
        data.id.toString());
    urlQuery->addQueryItem(
        AccountData_email_field,
        QString::fromStdString(data.email));
    urlQuery->addQueryItem(
        AccountData_passwordHa1_field,
        QString::fromStdString(data.passwordHa1));
    urlQuery->addQueryItem(
        AccountData_fullName_field,
        QString::fromStdString(data.fullName));
    urlQuery->addQueryItem(
        AccountData_statusCode_field,
        QnLexical::serialized(data.statusCode));
    urlQuery->addQueryItem(
        AccountData_customization_field,
        QString::fromStdString(data.customization));
}


////////////////////////////////////////////////////////////
//// class AccountActivationCode
////////////////////////////////////////////////////////////

MAKE_FIELD_NAME_STR_CONST(AccountActivationCode, code)

bool loadFromUrlQuery(const QUrlQuery& urlQuery, AccountActivationCode* const data)
{
    if (!urlQuery.hasQueryItem(AccountActivationCode_code_field))
        return false;
    data->code = urlQuery.queryItemValue(AccountActivationCode_code_field).toStdString();
    return true;
}

void serializeToUrlQuery(const AccountActivationCode& data, QUrlQuery* const urlQuery)
{
    urlQuery->addQueryItem(
        AccountActivationCode_code_field,
        QString::fromStdString(data.code));
}


QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (AccountData)(AccountActivationCode),
    (json)(sql_record),
    _Fields )

}   //api
}   //cdb
}   //nx
