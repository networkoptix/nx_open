// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::common {

/**
 * Authentication error code
 */
NX_REFLECTION_ENUM(AuthResult,
    Auth_OK, /**< OK. */
    Auth_WrongLogin, /**< Invalid login. */
    Auth_WrongInternalLogin, /**< Invalid login used for internal auth scheme. */
    Auth_WrongDigest, /**< Invalid or empty digest. */
    Auth_WrongPassword, /**< Invalid password. */
    /** No auth method found or custom auth scheme without login/password is failed. */
    Auth_Forbidden,
    Auth_PasswordExpired, /**< Password is expired. */
    Auth_LDAPConnectError, /**< Can't connect to the LDAP server to authenticate. */
    Auth_CloudConnectError, /**< Can't connect to the Cloud to authenticate. */
    Auth_DisabledUser, /**< Disabled user. */
    Auth_InvalidCsrfToken, /**< For cookie login. */
    Auth_LockedOut, /**< Locked out for a period of time. */
    Auth_WrongSessionToken, /**< Session token is invalid or expired. */
    Auth_WrongTicketToken, /**< Ticket token is invalid or expired. */
    Auth_DisabledBasicAndDigest, /**< HTTP basic and digest are disabled. */
    Auth_ClashedLogin, /**< More than one user with the same login are presented. */
    Auth_LdapTlsError /**< Error related to upgrading LDAP session to TLS */
)

NX_VMS_COMMON_API QString toErrorMessage(AuthResult value);

} // namespace nx::vms::common
