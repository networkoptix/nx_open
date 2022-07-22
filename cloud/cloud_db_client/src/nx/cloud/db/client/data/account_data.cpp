// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "account_data.h"

#include <nx/fusion/model_functions.h>
#include <nx/network/url/query_parse_helper.h>
#include <nx/utils/buffer.h>

#include "../field_name.h"

namespace nx::cloud::db::api {

using namespace nx::network;

//-------------------------------------------------------------------------------------------------
// class AccountRegistrationData

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    AccountRegistrationSecuritySettings, (json), AccountRegistrationSecuritySettings_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    AccountRegistrationSettings, (json), AccountRegistrationSettings_Fields)

MAKE_FIELD_NAME_STR_CONST(AccountRegistrationData, email)
MAKE_FIELD_NAME_STR_CONST(AccountRegistrationData, passwordHa1)
MAKE_FIELD_NAME_STR_CONST(AccountRegistrationData, password)
MAKE_FIELD_NAME_STR_CONST(AccountRegistrationData, fullName)
MAKE_FIELD_NAME_STR_CONST(AccountRegistrationData, customization)

bool loadFromUrlQuery(const QUrlQuery& urlQuery, AccountRegistrationData* const accountData)
{
    accountData->email =
        urlQuery.queryItemValue(AccountRegistrationData_email_field).toStdString();
    accountData->passwordHa1 =
        urlQuery.queryItemValue(AccountRegistrationData_passwordHa1_field).toStdString();
    accountData->password =
        urlQuery.queryItemValue(AccountRegistrationData_password_field).toStdString();
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
        AccountRegistrationData_password_field,
        QString::fromStdString(data.password));
    urlQuery->addQueryItem(
        AccountRegistrationData_fullName_field,
        QString::fromStdString(data.fullName));
    urlQuery->addQueryItem(
        AccountRegistrationData_customization_field,
        QString::fromStdString(data.customization));
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AccountStatusData, (json), AccountStatusData_Fields)

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

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    AccountRegistrationData, (json), AccountRegistrationData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AccountData, (json)(sql_record), AccountData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    AccountConfirmationCode, (json)(sql_record), AccountConfirmationCode_Fields)

//-------------------------------------------------------------------------------------------------
// class AccountUpdateData

MAKE_FIELD_NAME_STR_CONST(AccountUpdateData, passwordHa1)
MAKE_FIELD_NAME_STR_CONST(AccountUpdateData, password)
MAKE_FIELD_NAME_STR_CONST(AccountUpdateData, currentPassword)
MAKE_FIELD_NAME_STR_CONST(AccountUpdateData, fullName)
MAKE_FIELD_NAME_STR_CONST(AccountUpdateData, customization)
MAKE_FIELD_NAME_STR_CONST(AccountUpdateData, totp)
MAKE_FIELD_NAME_STR_CONST(AccountUpdateData, mfaCode)

bool loadFromUrlQuery(const QUrlQuery& urlQuery, AccountUpdateData* const data)
{
    url::deserializeField(
        urlQuery, AccountUpdateData_passwordHa1_field, &data->passwordHa1);
    url::deserializeField(
        urlQuery, AccountUpdateData_password_field, &data->password);
    url::deserializeField(
        urlQuery, AccountUpdateData_currentPassword_field, &data->currentPassword);
    url::deserializeField(
        urlQuery, AccountUpdateData_fullName_field, &data->fullName);
    url::deserializeField(
        urlQuery, AccountUpdateData_customization_field, &data->customization);
    url::deserializeField(
        urlQuery, AccountUpdateData_totp_field, &data->totp);
    url::deserializeField(
        urlQuery, AccountUpdateData_mfaCode_field, &data->mfaCode);
    return true;
}

void serializeToUrlQuery(const AccountUpdateData& data, QUrlQuery* const urlQuery)
{
    url::serializeField(
        urlQuery, AccountUpdateData_passwordHa1_field, data.passwordHa1);
    url::serializeField(
        urlQuery, AccountUpdateData_password_field, data.password);
    url::serializeField(
        urlQuery, AccountUpdateData_currentPassword_field, data.currentPassword);
    url::serializeField(
        urlQuery, AccountUpdateData_fullName_field, data.fullName);
    url::serializeField(
        urlQuery, AccountUpdateData_customization_field, data.customization);
    url::serializeField(
        urlQuery, AccountUpdateData_totp_field, data.totp);
    url::serializeField(
        urlQuery, AccountUpdateData_mfaCode_field, data.mfaCode);
}

void serialize(QnJsonContext*, const AccountUpdateData& data, QJsonValue* jsonValue)
{
    QJsonObject jsonObject;
    if (data.passwordHa1)
    {
        jsonObject.insert(
            AccountUpdateData_passwordHa1_field,
            QString::fromStdString(*data.passwordHa1));
    }

    if (data.password)
    {
        jsonObject.insert(
            AccountUpdateData_password_field,
            QString::fromStdString(*data.password));
    }

    if (data.currentPassword)
    {
        jsonObject.insert(
            AccountUpdateData_currentPassword_field, QString::fromStdString(*data.currentPassword));
    }

    if (data.totp)
    {
        jsonObject.insert(AccountUpdateData_totp_field,
            QString::fromStdString(*data.totp));
    }

    if (data.mfaCode)
    {
        jsonObject.insert(AccountUpdateData_mfaCode_field, QString::fromStdString(*data.mfaCode));
    }

    if (data.fullName)
    {
        jsonObject.insert(
            AccountUpdateData_fullName_field,
            QString::fromStdString(*data.fullName));
    }

    if (data.customization)
    {
        jsonObject.insert(
            AccountUpdateData_customization_field,
            QString::fromStdString(*data.customization));
    }

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

    auto passwordIter = map.find(AccountUpdateData_password_field);
    if (passwordIter != map.constEnd())
        data->password = passwordIter.value().toString().toStdString();

    auto currentPasswordIter = map.find(AccountUpdateData_currentPassword_field);
    if (currentPasswordIter != map.constEnd())
        data->currentPassword = currentPasswordIter.value().toString().toStdString();

    auto totpIter = map.find(AccountUpdateData_totp_field);
    if (totpIter != map.constEnd())
        data->totp = totpIter.value().toString().toStdString();

    auto mfaCodeIter = map.find(AccountUpdateData_mfaCode_field);
    if (mfaCodeIter != map.constEnd())
        data->mfaCode = totpIter.value().toString().toStdString();

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

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AccountEmail, (json), AccountEmail_Fields, (optional, false))

//-------------------------------------------------------------------------------------------------
// PasswordResetRequest

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PasswordResetRequest, (json), PasswordResetRequest_Fields)

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

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    TemporaryCredentialsTimeouts, (json), TemporaryCredentialsTimeouts_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    TemporaryCredentialsParams, (json), TemporaryCredentialsParams_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(TemporaryCredentials, (json), TemporaryCredentials_Fields)

//-------------------------------------------------------------------------------------------------
// struct AccountSecuritySettings

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    AccountSecuritySettings, (json), AccountSecuritySettings_Fields)

//-------------------------------------------------------------------------------------------------
// struct AccountForSharingRequest

void serializeToUrlQuery(const AccountForSharingRequest& requestData, QUrlQuery* urlQuery)
{
    if (requestData.nonce)
        urlQuery->addQueryItem("nonce", QString::fromStdString(*requestData.nonce));
}

bool loadFromUrlQuery(const QUrlQuery& urlQuery, AccountForSharingRequest* const data)
{
    if (urlQuery.hasQueryItem("nonce"))
        data->nonce = urlQuery.queryItemValue("nonce").toStdString();
    return true;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    AccountForSharingRequest, (json), AccountForSharingRequest_Fields)

//-------------------------------------------------------------------------------------------------
// struct AccountForSharing

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    AccountForSharing, (json), AccountForSharing_Fields)

} // namespace nx::cloud::db::api
