// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string_view>

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
NX_REFLECTION_INSTRUMENT(LoginUserFilter, LoginUserFilter_Fields)

NX_REFLECTION_ENUM_CLASS(LoginMethod,
    /**%apidoc HTTP Basic and digest (RFC 2617). */
    http = 1 << 0,

    /**%apidoc Local sessions (API `/rest/v{1-}/login/sessions`). */
    sessions = 1 << 1,

    /**%apidoc Cloud sessions by OAuth2 (Nx implementation). */
    nxOAuth2 = 1 << 2
)

Q_DECLARE_FLAGS(LoginMethods, LoginMethod)

struct NX_VMS_API LoginUser
{
    nx::Uuid id;
    QString username;
    UserType type = UserType::local;
    LoginMethods methods;
};
#define LoginUser_Fields (id)(username)(type)(methods)
NX_VMS_API_DECLARE_STRUCT_EX(LoginUser, (json))
NX_REFLECTION_INSTRUMENT(LoginUser, LoginUser_Fields);

// -----------------------------------------------------------------------------------------------

struct NX_VMS_API TemporaryLoginSessionRequest
{
    /**%apidoc
     * A token (UserModel.temporaryToken.token), previously received as a response to the
     * `POST /rest/v{3-}/users` request for creation a `temporaryLocal` user.
     */
    QString token;

    /**%apidoc[opt] Set HTTP cookie for automatic login by browser. */
    bool setCookie = false;
};
#define TemporaryLoginSessionRequest_Fields (token)(setCookie)
NX_VMS_API_DECLARE_STRUCT_EX(TemporaryLoginSessionRequest, (json))
NX_REFLECTION_INSTRUMENT(TemporaryLoginSessionRequest, TemporaryLoginSessionRequest_Fields);

struct NX_VMS_API LoginSessionRequest
{
    /**%apidoc
     * %example admin
     */
    QString username;

    /**%apidoc
     * %example password123
     */
    QString password;

    /**%apidoc[opt] Set HTTP cookie for automatic login by browser. */
    bool setCookie = false;

    /**%apidoc
     * Authorization Session token lifetime (seconds).
     * If the field is not set, the Site-wide value is used by default.
     * If the value is longer than the value of the Site-wide limit,
     * then the limit will be used.
     * If the value is present, it must be greater than or equal to 5 seconds.
     * %example 100
     */
    std::optional<std::chrono::seconds> durationS;
};
#define LoginSessionRequest_Fields (username)(password)(setCookie)(durationS)
NX_VMS_API_DECLARE_STRUCT_EX(LoginSessionRequest, (json))
NX_REFLECTION_INSTRUMENT(LoginSessionRequest, LoginSessionRequest_Fields);

struct NX_VMS_API LoginSessionFilter
{
    /**%apidoc
     * If the value "-" or "current" is used, the token is obtained from the authorization headers.
     * %example -
     */
    std::string token;

    /**%apidoc[opt] Set HTTP cookie for automatic login by browser. */
    bool setCookie = false;

    /**%apidoc[opt] Apply `token` to the current WebSocket connection. Useless for HTTP requests. */
    bool setSession = false;

    // Only works for /rest/v4+.
    /**%apidoc[opt]
     * User to read sessions for (requires administrator permissions), empty means current user,
     * explicit `*` means all users in the Site.
     */
    std::string username;

    /**%apidoc[opt] Target server, entire Site if not specified. */
    nx::Uuid serverId;
};
#define LoginSessionFilter_Fields (token)(setCookie)(setSession)(username)(serverId)
NX_VMS_API_DECLARE_STRUCT_EX(LoginSessionFilter, (json))
NX_REFLECTION_INSTRUMENT(LoginSessionFilter, LoginSessionFilter_Fields)

struct NX_VMS_API LoginSession
{
    static constexpr std::string_view kTokenPrefix = "vms-";
    static constexpr std::string_view kTicketPrefix = "vmsTicket-";

    nx::Uuid id;
    QString username;

    /**%apidoc The session authorization token to be used as HTTP bearer token or URL parameter. */
    std::string token;

    std::chrono::seconds ageS{0};
    std::chrono::seconds expiresInS{0};
};
#define LoginSession_Fields (id)(username)(token)(ageS)(expiresInS)
NX_VMS_API_DECLARE_STRUCT_EX(LoginSession, (json))
NX_REFLECTION_INSTRUMENT(LoginSession, LoginSession_Fields);

using Ticket = LoginSession;

struct NX_VMS_API CloudSignature
{
    /**%apidoc
     * %example MESSAGE
     */
    std::string message;

    /**%apidoc SIGNATURE = base64(hmacSha256(cloudAuthKey, message)) */
    std::optional<std::string> signature;
};
#define CloudSignature_Fields (message)(signature)
NX_VMS_API_DECLARE_STRUCT_EX(CloudSignature, (json))
NX_REFLECTION_INSTRUMENT(CloudSignature, CloudSignature_Fields);

} // namespace nx::vms::api
