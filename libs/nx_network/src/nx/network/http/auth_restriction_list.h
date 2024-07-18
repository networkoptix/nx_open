// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <vector>

#include <nx/network/http/http_types.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/thread/rw_lock.h>

namespace nx::network::http {

NX_REFLECTION_ENUM_CLASS(AuthMethod,
    notDefined = 0,

    /**%apidoc Authentication is not required. */
    noAuth = 1 << 0,

    /**%apidoc Authentication method described in RFC2617. */
    httpBasic = 1 << 1,

    /**%apidoc Authentication method described in RFC2617. */
    httpDigest = 1 << 2, //< Authentication method described in rfc2617.

    /**%apidoc Authentication method described in RFC6750. */
    httpBearer = 1 << 3,

    /**%apidoc Authentication method based on authinfo cookie. */
    cookie = 1 << 4,

    /**%apidoc
     * %deprecated Session authentication should be used instead.
     * Authentication by URL query parameters auth and proxy_auth.
     * Params have the following format:
     * `BASE64(UTF8(username) + ":" + MD5(UTF8(username) + ":" + realm + ":" + UTF8(password)))`.
     */
    urlQueryDigest = 1 << 6,

    /**%apidoc Authentication method based on authKey URL query parameter. */
    temporaryUrlQueryKey = 1 << 7,

    /**%apidoc Authentication method based on X-Runtime-Guid header. */
    xRuntimeGuid = 1 << 8,

    /**%apidoc Authentication method based on _sessionToken URL query parameter. */
    urlQuerySessionToken = 1 << 9,

    /**%apidoc Authentication method based on _ticket URL query parameter. */
    urlQueryTicket = 1 << 10,

    /**%apidoc[proprietary]
     * Normally all GET requests do not requires SCRF token, but we could not allow that, because
     * of poorly designed legacy and deprecated requests that change information (including
     * passwords). This authentication method will be removed as soon as all such request support
     * is terminated.
     */
    allowWithoutCsrf = 1 << 30
)

Q_DECLARE_FLAGS(AuthMethods, AuthMethod);
Q_DECLARE_OPERATORS_FOR_FLAGS(AuthMethods);

constexpr AuthMethods httpAuthMethods =
    AuthMethod::httpBasic | AuthMethod::httpDigest | AuthMethod::httpBearer;

constexpr AuthMethods defaultAuthMethods = httpAuthMethods
    | AuthMethod::cookie
    | AuthMethod::urlQueryDigest
    | AuthMethod::xRuntimeGuid
    | AuthMethod::urlQuerySessionToken
    | AuthMethod::urlQueryTicket;

/**
 * NOTE: By default, defaultAuthMethods are allowed for every url.
 */
class NX_NETWORK_API AuthMethodRestrictionList
{
public:
    struct Filter
    {
        std::optional<std::string> protocol;
        std::optional<Method> method;
        /** Regular expression. */
        std::optional<std::string> path;
    };

    AuthMethodRestrictionList(AuthMethods allowedByDefault = defaultAuthMethods);

    /**
     * @return Bit mask of auth methods (AuthMethod::Value enumeration).
     */
    AuthMethods getAllowedAuthMethods(const nx::network::http::Request& request) const;

    void allow(const Filter& filter, AuthMethods authMethod);

    /**
     * Allow using path filter only.
     */
    void allow(const std::string& pathMask, AuthMethods method);

    void deny(const Filter& filter, AuthMethods authMethod);

    /**
     * Deny using path filter only.
     */
    void deny(const std::string& pathMask, AuthMethods method);

    void backupDeniedRulesForTests();
    void restoreDeniedRulesForTests();

private:
    struct Rule
    {
        Filter filter;
        std::regex pathRegexp;
        AuthMethods methods;

        Rule(const Filter& filter, AuthMethods methods);

        bool matches(const Request& request) const;
    };

    const AuthMethods m_allowedByDefault;
    mutable nx::ReadWriteLock m_mutex;
    std::vector<Rule> m_allowed;
    std::vector<Rule> m_denied;
    std::vector<Rule> m_deniedBackup;
};

} // namespace nx::network::http
