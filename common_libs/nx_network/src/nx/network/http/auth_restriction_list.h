#pragma once

#include <map>

#include <QtCore/QString>
#include <QtCore/QRegExp>

#include <nx/network/http/http_types.h>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace network {
namespace http {

namespace AuthMethod {

enum Value
{
    NotDefined = 0x00,
    noAuth = 0x01,
    /** Authentication method described in rfc2617. */
    httpBasic = 0x02,
    /** Authentication method described in rfc2617. */
    httpDigest = 0x04,
    http = httpBasic | httpDigest,
    /** Based on authinfo cookie. */
    cookie = 0x08,
    /**
     * Authentication by X-NetworkOptix-VideoWall header.
     * TODO: #ak Does not look appropriate here.
     */
    videowall = 0x10,
    /**
     * Authentication by url query parameters auth and proxy_auth.
     * Params have the following format: BASE64-URL( UTF8(username) ":" MD5( LATIN1(username) ":NetworkOptix:" LATIN1(password) ) )
     * TODO #ak this method is too poor, have to introduce some salt.
     */
    urlQueryParam = 0x20,
    tempUrlQueryParam = 0x40,
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
    static const unsigned int kDefaults;

    /**
     * @return Bit mask of auth methods (AuthMethod::Value enumeration).
     */
    unsigned int getAllowedAuthMethods(const nx::network::http::Request& request) const;

    /**
     * @param pathMask Wildcard-mask of path.
     */
    void allow(const QString& pathMask, AuthMethod::Value method);
    /**
     * @param pathMask Wildcard-mask of path.
     */
    void deny(const QString& pathMask, AuthMethod::Value method);

private:
    struct Rule
    {
        QRegExp expression;
        unsigned int method;
        Rule(const QString& expression, unsigned int method);
    };

    mutable QnMutex m_mutex;
    std::map<QString, Rule> m_allowed;
    std::map<QString, Rule> m_denied;
};

} // namespace nx
} // namespace network
} // namespace http
