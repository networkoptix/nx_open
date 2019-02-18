#pragma once

#include <vector>

#include <QtCore/QString>
#include <QtCore/QRegExp>

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
    http = httpBasic | httpDigest,

    cookie = 1 << 3, //< Based on authinfo cookie.

    /**
     * Authentication by X-NetworkOptix-VideoWall header.
     * TODO: #ak Does not look appropriate here.
     */
    videowallHeader = 1 << 4,

    /**
     * Authentication by url query parameters auth and proxy_auth.
     * Params have the following format: BASE64-URL( UTF8(username) ":" MD5( LATIN1(username) ":NetworkOptix:" LATIN1(password) ) )
     * TODO: #ak This method is too poor, have to introduce some salt.
     */
    urlQueryDigest = 1 << 5,
    temporaryUrlQueryKey = 1 << 6,

    /**
     * X-Runtime-Guid header name.
     */
    sessionKey = 1 << 7,

    /**
     * Normally all GET requests do not requres SCRF token, but we could not allow that, because of
     * poorly designed requests changing information (including passwords).
     * TODO: Remove this propery as soon as all these requests are terminated.
     */
    allowWithourCsrf = 1 << 30,
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
        std::optional<std::string> method;
        /** Regular expression. */
        std::optional<std::string> path;
    };

    static const AuthMethod::Values kDefaults;

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

private:
    struct Rule
    {
        Filter filter;
        QRegExp pathRegexp;
        AuthMethod::Values methods;

        Rule(const Filter& filter, AuthMethod::Values methods);

        bool matches(const Request& request) const;
    };

    const AuthMethod::Values m_allowedByDefault = kDefaults;
    mutable QnMutex m_mutex;
    std::vector<Rule> m_allowed;
    std::vector<Rule> m_denied;
};

} // namespace nx::network::http
