// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::cloud::oauth2::api {

static constexpr char kOauthIntrospectPath[] = "/oauth2/v1/introspect";
static constexpr char kOauthLogoutPath[] = "/oauth2/v1/user/self";
static constexpr char kOauthClientLogoutPath[] = "/oauth2/v1/user/self/client/{clientId}";
static constexpr char kOauthTokenPath[] = "/oauth2/v1/token";

static constexpr char kOauthJwksPath[] = "/oauth2/v1/jwks";
static constexpr char kOauthJwksRfc7517Path[] = "/oauth2/v1/.well-known/jwks.json";
static constexpr char kOauthJwkByIdPath[] = "/oauth2/v1/jwks/{kid}";

static constexpr char kOauthPasswordResetCodePath[] = "/oauth2/v1/internal/passwordResetCode";
static constexpr char kOauthSessionPath[] = "/oauth2/v1/session/{sessionId}";
static constexpr char kAccountChangedEventHandler[] = "/oauth2/v1/accountUpdate";

static constexpr char kHealthPath[] = "/maintenance/health";

static constexpr char kApiPrefix[] = "/oauth2";
static constexpr char kClientIdParam[] = "clientId";

static constexpr char kApiDocPrefix[] = "/oauth2/docs/api";

static constexpr char kOauthServiceToken[] = "/oauth2/v1/internal/serviceToken";

} // namespace nx::cloud::oauth2::api
