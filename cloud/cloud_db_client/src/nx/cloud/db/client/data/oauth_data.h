// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/cloud/db/api/oauth_data.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/url_query.h>

namespace nx::cloud::db::api {

NX_REFLECTION_INSTRUMENT_ENUM(GrantType,
    authorization_code,
    password,
    refresh_token
)

NX_REFLECTION_INSTRUMENT_ENUM(ResponseType,
    token,
    code
)

NX_REFLECTION_INSTRUMENT_ENUM(TokenType, bearer)

#define Token_request_Fields \
    (grant_type)(response_type)(client_id)(scope)(password)(username)(refresh_token)(code)( \
        refresh_token_lifetime)
NX_REFLECTION_INSTRUMENT(IssueTokenRequest, Token_request_Fields)

#define Issue_token_response_Fields \
    (access_token)(refresh_token)(expires_in)(expires_at)(token_type)(scope)(error)

#define Issue_code_response_Fields (access_code)(code)(error)

#define Issue_stun_token_request_Fields (server_name)

#define Issue_stun_token_response_Fields (token)(mac_code)(error)(kid)(expires_at)(expires_in)

NX_REFLECTION_INSTRUMENT(IssueTokenResponse, Issue_token_response_Fields);

NX_REFLECTION_INSTRUMENT(ValidateTokenResponse, (access_token)(expires_in)(expires_at)(token_type) \
    (scope)(username)(vms_user_id)(time_since_password))

NX_REFLECTION_INSTRUMENT(IssueCodeResponse, Issue_code_response_Fields);

NX_REFLECTION_INSTRUMENT(IssueStunTokenRequest, Issue_stun_token_request_Fields);

NX_REFLECTION_INSTRUMENT(IssueStunTokenResponse, Issue_stun_token_response_Fields);

} // namespace nx::cloud::db::api
