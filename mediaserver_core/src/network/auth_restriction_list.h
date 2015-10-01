/**********************************************************
* Aug 27, 2015
* a.kolesnikov
***********************************************************/

#ifndef AUTH_RESTRICTION_LIST_H
#define AUTH_RESTRICTION_LIST_H

#include <map>

#include <QtCore/QString>

#include "utils/network/http/httptypes.h"


namespace AuthMethod
{
    enum Value
    {
        NotDefined      = 0x00,
        noAuth          = 0x01,
        //!authentication method described in rfc2617
        httpBasic       = 0x02,
        httpDigest      = 0x04,
        http            = httpBasic | httpDigest,
        //!base on \a authinfo cookie
        cookie          = 0x08,
        //!authentication by X-NetworkOptix-VideoWall header
        //TODO #ak remove this value from here
        videowall       = 0x10,
        //!Authentication by url query parameters \a auth and \a proxy_auth
        /*!
            params has following format: BASE64-URL( UTF8(username) ":" MD5( LATIN1(username) ":NetworkOptix:" LATIN1(password) ) )
            TODO #ak this method is too poor, have to introduce some salt
        */
        urlQueryParam   = 0x20,
        tempUrlQueryParam   = 0x40
    };
    Q_DECLARE_FLAGS(Values, Value);
    Q_DECLARE_OPERATORS_FOR_FLAGS(Values);
}

/*!
    \note By default, \a AuthMethod::http, \a AuthMethod::cookie and \a AuthMethod::videowall authorization methods are allowed fo every url
*/
class QnAuthMethodRestrictionList
{
public:
    QnAuthMethodRestrictionList();

    /*!
        \return bit mask of auth methods (\a AuthMethod::Value enumeration)
    */
    unsigned int getAllowedAuthMethods( const nx_http::Request& request ) const;

    /*!
        \param pathMask wildcard-mask of path
    */
    void allow( const QString& pathMask, AuthMethod::Value method );
    /*!
        \param pathMask wildcard-mask of path
    */
    void deny( const QString& pathMask, AuthMethod::Value method );

private:
    //!map<path mask, allowed auth bitmask>
    std::map<QString, unsigned int> m_allowed;
    //!map<path mask, denied auth bitmask>
    std::map<QString, unsigned int> m_denied;
};

#endif  //AUTH_RESTRICTION_LIST_H
