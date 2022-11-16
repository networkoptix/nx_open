// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "account_data.h"

#include <nx/network/url/query_parse_helper.h>
#include <nx/utils/buffer.h>

#include "../field_name.h"

namespace nx::cloud::db::api {

using namespace nx::network;

//-------------------------------------------------------------------------------------------------
// class AccountRegistrationData

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

} // namespace nx::cloud::db::api
