#pragma once 

#include <map>

#include <QtCore/QString>

#include <nx/network/http/httptypes.h>

namespace nx_http {

namespace AuthMethod {

enum Value
{
    NotDefined      = 0x00,
    noAuth          = 0x01,
    /**
     * Authentication method described in rfc2617.
     */
    httpBasic       = 0x02,
    httpDigest      = 0x04,
    http            = httpBasic | httpDigest,
    /**
     * Based on authinfo cookie.
     */
    cookie          = 0x08,
    /**
     * Authentication by X-NetworkOptix-VideoWall header.
     * TODO: #ak Does not look appropriate here.
     */
    videowall       = 0x10,
    /**
     * Authentication by url query parameters auth and proxy_auth.
     * params has following format: BASE64-URL( UTF8(username) ":" MD5( LATIN1(username) ":NetworkOptix:" LATIN1(password) ) )
     * TODO #ak this method is too poor, have to introduce some salt.
     */
    urlQueryParam   = 0x20,
    tempUrlQueryParam   = 0x40
};

Q_DECLARE_FLAGS(Values, Value);
Q_DECLARE_OPERATORS_FOR_FLAGS(Values);

} // namespace AuthMethod

/**
 * @note By default, AuthMethod::http, AuthMethod::cookie and AuthMethod::videowall 
 * authorization methods are allowed fo every url.
 */
class NX_NETWORK_API AuthMethodRestrictionList
{
public:
    /**
     * @return bit mask of auth methods (AuthMethod::Value enumeration).
     */
    unsigned int getAllowedAuthMethods(const nx_http::Request& request) const;

    /**
     * @param pathMask wildcard-mask of path.
     */
    void allow(const QString& pathMask, AuthMethod::Value method);
    /**
     * @param pathMask wildcard-mask of path.
     */
    void deny(const QString& pathMask, AuthMethod::Value method);

private:
    /** map<path mask, allowed auth bitmask>. */
    std::map<QString, unsigned int> m_allowed;
    /** map<path mask, denied auth bitmask>. */
    std::map<QString, unsigned int> m_denied;
};

} // namespace nx_http
