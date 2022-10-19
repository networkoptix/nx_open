// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <vector>

#include <QtCore/QRegularExpression>

#include <nx/network/http/http_types.h>
#include <nx/utils/thread/mutex.h>

namespace nx::network::http {

namespace AuthMethod {

enum Value
{
    NotDefined = 0,
    noAuth = 1 << 0,

    httpBasic = 1 << 1, //< Authentication method described in rfc2617.
    httpDigest = 1 << 2, //< Authentication method described in rfc2617.
    httpBearer = 1 << 3, //< Authentication method described in rfc6750.
    http = httpBasic | httpDigest | httpBearer,

    cookie = 1 << 4, //< Based on authinfo cookie.

    /**
     * Authentication by url query parameters auth and proxy_auth.
     * Params have the following format:
     *     BASE64(UTF8(username) + ":" + MD5(UTF8(username) + ":" + realm + ":" + UTF8(password)))
     * TODO: This method is too poor and deprecated, session authentication should be used instead.
     */
    urlQueryDigest = 1 << 6,
    temporaryUrlQueryKey = 1 << 7,

    /**
     * X-Runtime-Guid header name.
     */
    xRuntimeGuid = 1 << 8,

    /**
     * URL Query: _sessionToken
     */
    urlQuerySessionToken = 1 << 9,

    /**
     * Normally all GET requests do not requres SCRF token, but we could not allow that, because of
     * poorly designed requests changing information (including passwords).
     * TODO: Remove this propery as soon as all these requests are terminated.
     */
    allowWithoutCsrf = 1 << 30,
    urlQueryDigestWithoutCsrf = urlQueryDigest | allowWithoutCsrf,

    defaults = http | cookie | urlQueryDigest | xRuntimeGuid | urlQuerySessionToken,
};

Q_DECLARE_FLAGS(Values, Value);
Q_DECLARE_OPERATORS_FOR_FLAGS(Values);

} // namespace AuthMethod

/**
 * NOTE: By default, AuthMethod::http, AuthMethod::cookie and AuthMethod::videowall
 * authorization methods are allowed fo every url.
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

    AuthMethodRestrictionList() = default;
    AuthMethodRestrictionList(AuthMethod::Values allowedByDefault);

    /**
     * @return Bit mask of auth methods (AuthMethod::Value enumeration).
     */
    AuthMethod::Values getAllowedAuthMethods(const nx::network::http::Request& request) const;

    void allow(const Filter& filter, AuthMethod::Values authMethod);

    /**
     * Allow using path filter only.
     */
    void allow(const std::string& pathMask, AuthMethod::Values method);

    void deny(const Filter& filter, AuthMethod::Values authMethod);

    /**
     * Deny using path filter only.
     */
    void deny(const std::string& pathMask, AuthMethod::Values method);

    void backupDeniedRulesForTests();
    void restoreDeniedRulesForTests();

private:
    struct Rule
    {
        Filter filter;
        // TODO: #akolesnikov Replace with std::regex.
        QRegularExpression pathRegexp;
        AuthMethod::Values methods;

        Rule(const Filter& filter, AuthMethod::Values methods);

        bool matches(const Request& request) const;
    };

    const AuthMethod::Values m_allowedByDefault = AuthMethod::defaults;
    mutable nx::Mutex m_mutex;
    std::vector<Rule> m_allowed;
    std::vector<Rule> m_denied;
    std::vector<Rule> m_deniedBackup;
};

} // namespace nx::network::http
