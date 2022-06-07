// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/instrument.h>

#include "data_macros.h"
#include "user_data.h"

namespace nx::vms::api {

struct NX_VMS_API LoginUserFilter
{
    QString username;
};
#define LoginUserFilter_Fields (username)
NX_VMS_API_DECLARE_STRUCT_EX(LoginUserFilter, (json))

NX_REFLECTION_ENUM_CLASS(LoginMethod,
    http = 1 << 0,
    sessions = 1 << 1,
    nxOAuth2 = 1 << 2
)

Q_DECLARE_FLAGS(LoginMethods, LoginMethod)

struct NX_VMS_API LoginUser
{
    QString username;

    /**%apidoc
     * User type, one of:
     * * `local` - This User is managed by the Server. Session tokens must be obtained locally.
     * * `ldap` - This User is managed by the LDAP server. Session tokens must be obtained locally.
     * * `cloud` - This User is managed by the Cloud. Session tokens must be obtained on the cloud
     *     side.
     */
    UserType type = UserType::local;

    /**%apidoc
     * Supported authorization types separated by "|", one of:
     * * `http` - HTTP Basic and digest.
     * * `sessions` - Local sessions.
     * * `nxOAuth2` - Cloud sessions by OAuth2 (Nx implementation).
     */
    LoginMethods methods;
};
#define LoginUser_Fields (username)(type)(methods)
NX_VMS_API_DECLARE_STRUCT_EX(LoginUser, (json))
NX_REFLECTION_INSTRUMENT(LoginUser, LoginUser_Fields);

// -----------------------------------------------------------------------------------------------

struct NX_VMS_API LoginSessionRequest
{
    QString username;
    QString password;

    /**%apidoc[opt] Set HTTP cookie for automatic login by browser. */
    bool setCookie = false;
};
#define LoginSessionRequest_Fields (username)(password)(setCookie)
NX_VMS_API_DECLARE_STRUCT_EX(LoginSessionRequest, (json))
NX_REFLECTION_INSTRUMENT(LoginSessionRequest, LoginSessionRequest_Fields);

struct NX_VMS_API LoginSessionFilter
{
    /**%apidoc If the value "current" is used, the token is obtained from authorization. */
    std::string token;

    /**%apidoc[opt] Set HTTP cookie for automatic login by browser. */
    bool setCookie = false;
};
#define LoginSessionFilter_Fields (token)(setCookie)
NX_VMS_API_DECLARE_STRUCT_EX(LoginSessionFilter, (json))

struct NX_VMS_API LoginSession
{
    QString username;

    /**%apidoc The session authorization token to be used as HTTP bearer token or URL parameter. */
    std::string token;

    /**%apidoc:integer */
    std::chrono::seconds ageS{0};
    /**%apidoc:integer */
    std::chrono::seconds expiresInS{0};
};
#define LoginSession_Fields (username)(token)(ageS)(expiresInS)
NX_VMS_API_DECLARE_STRUCT_EX(LoginSession, (json))
NX_REFLECTION_INSTRUMENT(LoginSession, LoginSession_Fields);

struct NX_VMS_API CloudSignature
{
    /**%apidoc:string */
    std::string message;

    /**%apidoc:string SIGNATURE = base64(hmacSha256(cloudSystemAuthKey, message)) */
    std::optional<std::string> signature;
};
#define CloudSignature_Fields (message)(signature)
NX_VMS_API_DECLARE_STRUCT_EX(CloudSignature, (json))
NX_REFLECTION_INSTRUMENT(CloudSignature, CloudSignature_Fields);

} // namespace nx::vms::api
