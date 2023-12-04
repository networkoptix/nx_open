// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/cloud/db/api/oauth_data.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/url_query.h>

namespace nx::cloud::db::api {

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(GrantType*, Visitor&& visitor)
{
    using GrantType = GrantType;
    using Item = nx::reflect::enumeration::Item<GrantType>;
    return visitor(
        Item{GrantType::authorizationCode, "authorization_code"},
        Item{GrantType::password, "password"},
        Item{GrantType::refreshToken, "refresh_token"}
    );
}

NX_REFLECTION_INSTRUMENT_ENUM(ResponseType,
    token,
    code
)

NX_REFLECTION_INSTRUMENT_ENUM(TokenType, bearer)

#define Token_request_Fields \
    (grant_type)(response_type)(client_id)(scope)(password)(username)(refresh_token)(code)( \
        refresh_token_lifetime)
NX_REFLECTION_INSTRUMENT(IssueTokenRequest, Token_request_Fields)

QN_FUSION_DECLARE_FUNCTIONS(IssueTokenRequest, (json))
QN_FUSION_DECLARE_FUNCTIONS(IssueTokenResponse, (json))
QN_FUSION_DECLARE_FUNCTIONS(ValidateTokenResponse, (json))
QN_FUSION_DECLARE_FUNCTIONS(IssueCodeResponse, (json))
QN_FUSION_DECLARE_FUNCTIONS(IssueCodeResponse, (json))
QN_FUSION_DECLARE_FUNCTIONS(IssueStunTokenRequest, (json))
QN_FUSION_DECLARE_FUNCTIONS(IssueStunTokenResponse, (json))

#define Issue_token_response_Fields \
    (access_token)(refresh_token)(expires_in)(expires_at)(token_type)(scope)(error)

#define Validate_token_response_Fields \
    (access_token)(expires_in)(expires_at)(token_type)(scope)(username)(vms_user_id) \
    (time_since_password)

#define Issue_code_response_Fields (access_code)(code)(error)

#define Issue_stun_token_request_Fields (server_name)

#define Issue_stun_token_response_Fields (token)(mac_code)(error)(kid)(expires_at)(expires_in)

NX_REFLECTION_INSTRUMENT(IssueTokenResponse, Issue_token_response_Fields);

NX_REFLECTION_INSTRUMENT(ValidateTokenResponse, Validate_token_response_Fields)

NX_REFLECTION_INSTRUMENT(IssueCodeResponse, Issue_code_response_Fields);

NX_REFLECTION_INSTRUMENT(IssueStunTokenRequest, Issue_stun_token_request_Fields);

NX_REFLECTION_INSTRUMENT(IssueStunTokenResponse, Issue_stun_token_response_Fields);

} // namespace nx::cloud::db::api
