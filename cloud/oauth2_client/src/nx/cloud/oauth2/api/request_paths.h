// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::cloud::oauth2::api {

static constexpr char kOauthIntrospectPath[] = "/oauth2/v1/introspect";
static constexpr char kOauthLogoutPath[] = "/oauth2/v1/user/self";
static constexpr char kOauthJwksPath[] = "/oauth2/v1/jwks";
static constexpr char kOauthJwkByIdPath[] = "/oauth2/v1/jwks/{kid}";
static constexpr char kOauthTokenPath[] = "/oauth2/v1/token";
static constexpr char kOauthSessionPath[] = "/oauth2/v1/session/{sessionId}";

} // namespace nx::cloud::oauth2::api
