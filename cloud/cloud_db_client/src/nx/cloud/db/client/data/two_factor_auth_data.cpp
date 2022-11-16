// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "two_factor_auth_data.h"

#include <nx/network/url/query_parse_helper.h>
#include <nx/reflect/json/serializer.h>
#include <nx/utils/switch.h>

namespace nx::cloud::db::api {

void serializeToUrlQuery(const ValidateKeyRequest& requestData, QUrlQuery* urlQuery)
{
    urlQuery->addQueryItem("token", QString::fromStdString(requestData.token));
}

bool loadFromUrlQuery(const QUrlQuery& urlQuery, ValidateKeyRequest* const data)
{
    if (!urlQuery.hasQueryItem("token"))
        return false;
    data->token = urlQuery.queryItemValue("token").toStdString();
    return true;
}

void serializeToUrlQuery(const ValidateBackupCodeRequest& requestData, QUrlQuery* urlQuery)
{
    urlQuery->addQueryItem("token", QString::fromStdString(requestData.token));
}

bool loadFromUrlQuery(const QUrlQuery& urlQuery, ValidateBackupCodeRequest* const data)
{
    if (!urlQuery.hasQueryItem("token"))
        return false;
    data->token = urlQuery.queryItemValue("token").toStdString();
    return true;
}

bool fromString(const std::string& str, SecondFactorState* state)
{
    bool success = true;
    *state = nx::utils::switch_(
        str,
        "entered",
        []() { return SecondFactorState::entered; },
        "not_entered",
        [] { return SecondFactorState::notEntered; },
        nx::utils::default_,
        [&success] {
            success = false;
            return SecondFactorState::notEntered;
        });
    return success;
}

std::string toString(SecondFactorState state)
{
    switch (state)
    {
        case SecondFactorState::entered:
            return "entered";
        case SecondFactorState::notEntered:
            return "not_entered";
    }
    return "";
}

} // namespace nx::cloud::db::api
