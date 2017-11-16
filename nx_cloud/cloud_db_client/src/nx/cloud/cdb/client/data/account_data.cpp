#include "account_data.h"

#include <nx/fusion/model_functions.h>
#include <nx/network/buffer.h>
#include <nx/network/url/query_parse_helper.h>

#include "../field_name.h"

namespace nx {
namespace cdb {
namespace api {

using namespace nx::network;

//-------------------------------------------------------------------------------------------------
// class AccountRegistrationData

MAKE_FIELD_NAME_STR_CONST(AccountRegistrationData, email)
MAKE_FIELD_NAME_STR_CONST(AccountRegistrationData, passwordHa1)
MAKE_FIELD_NAME_STR_CONST(AccountRegistrationData, passwordHa1Sha256)
MAKE_FIELD_NAME_STR_CONST(AccountRegistrationData, fullName)
MAKE_FIELD_NAME_STR_CONST(AccountRegistrationData, customization)

bool loadFromUrlQuery(const QUrlQuery& urlQuery, AccountRegistrationData* const accountData)
{
    accountData->email = 
        urlQuery.queryItemValue(AccountRegistrationData_email_field).toStdString();
    accountData->passwordHa1 = 
        urlQuery.queryItemValue(AccountRegistrationData_passwordHa1_field).toStdString();
    accountData->passwordHa1Sha256 = 
        urlQuery.queryItemValue(AccountRegistrationData_passwordHa1Sha256_field).toStdString();
    accountData->fullName = 
        urlQuery.queryItemValue(AccountRegistrationData_fullName_field).toStdString();
    accountData->customization =
        urlQuery.queryItemValue(AccountRegistrationData_customization_field).toStdString();

    return !accountData->email.empty();
}

void serializeToUrlQuery(const AccountRegistrationData& data, QUrlQuery* const urlQuery)
{
    urlQuery->addQueryItem(
        AccountRegistrationData_email_field,
        QString::fromStdString(data.email));
    urlQuery->addQueryItem(
        AccountRegistrationData_passwordHa1_field,
        QString::fromStdString(data.passwordHa1));
    urlQuery->addQueryItem(
        AccountRegistrationData_passwordHa1Sha256_field,
        QString::fromStdString(data.passwordHa1Sha256));
    urlQuery->addQueryItem(
        AccountRegistrationData_fullName_field,
        QString::fromStdString(data.fullName));
    urlQuery->addQueryItem(
        AccountRegistrationData_customization_field,
        QString::fromStdString(data.customization));
}


//-------------------------------------------------------------------------------------------------
// class AccountConfirmationCode

MAKE_FIELD_NAME_STR_CONST(AccountConfirmationCode, code)

bool loadFromUrlQuery(const QUrlQuery& urlQuery, AccountConfirmationCode* const data)
{
    if (!urlQuery.hasQueryItem(AccountConfirmationCode_code_field))
        return false;
    data->code = urlQuery.queryItemValue(AccountConfirmationCode_code_field).toStdString();
    return true;
}

void serializeToUrlQuery(const AccountConfirmationCode& data, QUrlQuery* const urlQuery)
{
    urlQuery->addQueryItem(
        AccountConfirmationCode_code_field,
        QString::fromStdString(data.code));
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (AccountRegistrationData)(AccountData)(AccountConfirmationCode),
    (json)(sql_record),
    _Fields)

//-------------------------------------------------------------------------------------------------
// class AccountUpdateData

MAKE_FIELD_NAME_STR_CONST(AccountUpdateData, passwordHa1)
MAKE_FIELD_NAME_STR_CONST(AccountUpdateData, passwordHa1Sha256)
MAKE_FIELD_NAME_STR_CONST(AccountUpdateData, fullName)
MAKE_FIELD_NAME_STR_CONST(AccountUpdateData, customization)

bool loadFromUrlQuery(const QUrlQuery& urlQuery, AccountUpdateData* const data)
{
    url::deserializeField(
        urlQuery, AccountUpdateData_passwordHa1_field, &data->passwordHa1);
    url::deserializeField(
        urlQuery, AccountUpdateData_passwordHa1Sha256_field, &data->passwordHa1Sha256);
    url::deserializeField(
        urlQuery, AccountUpdateData_fullName_field, &data->fullName);
    url::deserializeField(
        urlQuery, AccountUpdateData_customization_field, &data->customization);
    return true;
}

void serializeToUrlQuery(const AccountUpdateData& data, QUrlQuery* const urlQuery)
{
    url::serializeField(
        urlQuery, AccountUpdateData_passwordHa1_field, data.passwordHa1);
    url::serializeField(
        urlQuery, AccountUpdateData_passwordHa1Sha256_field, data.passwordHa1Sha256);
    url::serializeField(
        urlQuery, AccountUpdateData_fullName_field, data.fullName);
    url::serializeField(
        urlQuery, AccountUpdateData_customization_field, data.customization);
}

void serialize(QnJsonContext*, const AccountUpdateData& data, QJsonValue* jsonValue)
{
    QJsonObject jsonObject;
    if (data.passwordHa1)
        jsonObject.insert(
            AccountUpdateData_passwordHa1_field,
            QString::fromStdString(data.passwordHa1.get()));
    if (data.passwordHa1Sha256)
        jsonObject.insert(
            AccountUpdateData_passwordHa1Sha256_field,
            QString::fromStdString(data.passwordHa1Sha256.get()));
    if (data.fullName)
        jsonObject.insert(
            AccountUpdateData_fullName_field,
            QString::fromStdString(data.fullName.get()));
    if (data.customization)
        jsonObject.insert(
            AccountUpdateData_customization_field,
            QString::fromStdString(data.customization.get()));
    *jsonValue = jsonObject;
}

bool deserialize(QnJsonContext*, const QJsonValue& value, AccountUpdateData* data)
{
    if (value.type() != QJsonValue::Object)
        return false;
    const QJsonObject map = value.toObject();

    auto passwordHa1Iter = map.find(AccountUpdateData_passwordHa1_field);
    if (passwordHa1Iter != map.constEnd())
        data->passwordHa1 = passwordHa1Iter.value().toString().toStdString();

    auto passwordHa1Sha256Iter = map.find(AccountUpdateData_passwordHa1Sha256_field);
    if (passwordHa1Sha256Iter != map.constEnd())
        data->passwordHa1Sha256 = passwordHa1Sha256Iter.value().toString().toStdString();

    auto fullNameIter = map.find(AccountUpdateData_fullName_field);
    if (fullNameIter != map.constEnd())
        data->fullName = fullNameIter.value().toString().toStdString();

    auto customizationIter = map.find(AccountUpdateData_customization_field);
    if (customizationIter != map.constEnd())
        data->customization = customizationIter.value().toString().toStdString();

    return true;
}

//-------------------------------------------------------------------------------------------------
// class AccountEmail

MAKE_FIELD_NAME_STR_CONST(AccountEmail, email)

bool loadFromUrlQuery(const QUrlQuery& urlQuery, AccountEmail* const data)
{
    if (!urlQuery.hasQueryItem(AccountEmail_email_field))
        return false;
    data->email = urlQuery.queryItemValue(AccountEmail_email_field).toStdString();
    return true;
}

void serializeToUrlQuery(const AccountEmail& data, QUrlQuery* const urlQuery)
{
    urlQuery->addQueryItem(
        AccountEmail_email_field,
        QString::fromStdString(data.email));
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((AccountEmail), (json), _Fields, (optional, false))

//-------------------------------------------------------------------------------------------------
// class TemporaryCredentials

MAKE_FIELD_NAME_STR_CONST(TemporaryCredentialsTimeouts, expirationPeriod)
MAKE_FIELD_NAME_STR_CONST(TemporaryCredentialsTimeouts, autoProlongationEnabled)
MAKE_FIELD_NAME_STR_CONST(TemporaryCredentialsTimeouts, prolongationPeriod)

bool loadFromUrlQuery(const QUrlQuery& urlQuery, TemporaryCredentialsTimeouts* const data)
{
    if (!urlQuery.hasQueryItem(TemporaryCredentialsTimeouts_expirationPeriod_field))
        return false;
    data->expirationPeriod = std::chrono::seconds(
        urlQuery.queryItemValue(TemporaryCredentialsTimeouts_expirationPeriod_field).toLongLong());

    if (!urlQuery.hasQueryItem(TemporaryCredentialsTimeouts_autoProlongationEnabled_field))
        return true;
    data->autoProlongationEnabled = urlQuery.queryItemValue(
        TemporaryCredentialsTimeouts_autoProlongationEnabled_field) == "true";
    if (!data->autoProlongationEnabled)
        return true;

    //when autoProlongationEnabled is true, prolongationPeriod MUST be present
    if (!urlQuery.hasQueryItem(TemporaryCredentialsTimeouts_prolongationPeriod_field))
        return false;
    data->prolongationPeriod = std::chrono::seconds(
        urlQuery.queryItemValue(TemporaryCredentialsTimeouts_prolongationPeriod_field).toLongLong());

    return true;
}

void serializeToUrlQuery(
    const TemporaryCredentialsTimeouts& data,
    QUrlQuery* const urlQuery)
{
    urlQuery->addQueryItem(
        TemporaryCredentialsTimeouts_expirationPeriod_field,
        QString::number(data.expirationPeriod.count()));
    urlQuery->addQueryItem(
        TemporaryCredentialsTimeouts_autoProlongationEnabled_field,
        data.autoProlongationEnabled ? "true" : "false");
    urlQuery->addQueryItem(
        TemporaryCredentialsTimeouts_prolongationPeriod_field,
        QString::number(data.prolongationPeriod.count()));
}

MAKE_FIELD_NAME_STR_CONST(TemporaryCredentialsParams, type)

bool loadFromUrlQuery(
    const QUrlQuery& urlQuery,
    TemporaryCredentialsParams* const data)
{
    data->type = urlQuery.queryItemValue(TemporaryCredentialsParams_type_field).toStdString();
    if (!data->type.empty())
        return true;    //ignoring other parameters
    return loadFromUrlQuery(urlQuery, &data->timeouts);
}

void serializeToUrlQuery(
    const TemporaryCredentialsParams& data,
    QUrlQuery* const urlQuery)
{
    if (!data.type.empty())
    {
        urlQuery->addQueryItem(
            TemporaryCredentialsParams_type_field,
            QString::fromStdString(data.type));
    }
    else
    {
        serializeToUrlQuery(data.timeouts, urlQuery);
    }
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (TemporaryCredentialsTimeouts)(TemporaryCredentialsParams)(TemporaryCredentials),
    (json),
    _Fields)

} // namespace api
} // namespace cdb
} // namespace nx

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::cdb::api, AccountStatus,
    (nx::cdb::api::AccountStatus::invalid, "invalid")
    (nx::cdb::api::AccountStatus::awaitingActivation, "awaitingEmailConfirmation")
    (nx::cdb::api::AccountStatus::activated, "activated")
    (nx::cdb::api::AccountStatus::blocked, "blocked")
    (nx::cdb::api::AccountStatus::invited, "invited")
)
