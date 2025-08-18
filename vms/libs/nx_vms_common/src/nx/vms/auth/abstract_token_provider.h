// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <include/nx/cloud/db/api/oauth_data.h>

using TokenData = nx::cloud::db::api::IssueTokenResponse;
using MaybeTokenData = std::optional<TokenData>;

namespace nx::vms::auth {

class AbstractOauthTokenProvider
{
public:
    virtual ~AbstractOauthTokenProvider() = default;

    virtual MaybeTokenData tokenData() const = 0;
};

} // nx::vms::auth
