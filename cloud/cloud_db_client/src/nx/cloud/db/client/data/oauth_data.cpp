// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "oauth_data.h"

#include <nx/fusion/model_functions.h>
#include <nx/network/url/query_parse_helper.h>
#include <nx/reflect/json/serializer.h>

#include "../field_name.h"

namespace nx::cloud::db::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    IssueTokenRequest,
    (json),
    Token_request_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(IssueTokenResponse, (json), Issue_token_response_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ValidateTokenResponse, (json), Validate_token_response_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(IssueCodeResponse, (json), Issue_code_response_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(IssueStunTokenResponse, (json), Issue_stun_token_response_Fields)
} // namespace nx::cloud::db::api
