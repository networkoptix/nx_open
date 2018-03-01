#pragma once 

#include <map>

#include <QtCore/QString>
#include <QtCore/QRegExp>

#include <nx/network/http/http_types.h>
#include <nx/utils/thread/mutex.h>

namespace nx_http {

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
    videowall = 1 << 4,

    /**
     * Authentication by url query parameters auth and proxy_auth.
     * Params have the following format: BASE64-URL( UTF8(username) ":" MD5( LATIN1(username) ":NetworkOptix:" LATIN1(password) ) )
     * TODO: #ak This method is too poor, have to introduce some salt.
     */
    urlQueryParam = 1 << 5,
    tempUrlQueryParam = 1 << 6,

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

class NX_NETWORK_API AuthMethodRestrictionList
{
public:
    static const AuthMethod::Values kDefaults;

    AuthMethod::Values getAllowedAuthMethods(const nx_http::Request& request) const;
    void allow(const QString& pathMask, AuthMethod::Values method);
    void deny(const QString& pathMask, AuthMethod::Values method);

private:
    struct Rule
    {
        QRegExp expression;
        AuthMethod::Values methods;
        Rule(const QString& expression, AuthMethod::Values methods);
    };

    mutable QnMutex m_mutex;
    std::map<QString, Rule> m_allowed;
    std::map<QString, Rule> m_denied;
    std::set<QString> m_allowCookieWithotScrf;
};

} // namespace nx_http
