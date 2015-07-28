#ifndef __QN_AUTH_HELPER__
#define __QN_AUTH_HELPER__

#include <map>

#include <QAuthenticator>
#include <QtCore/QMap>
#include <QElapsedTimer>
#include <QCache>

#include <core/resource/resource_fwd.h>

#include "utils/common/id.h"
#include "utils/network/http/httptypes.h"
#include "utils/common/uuid.h"


namespace AuthMethod
{
    enum Value
    {
        noAuth          = 0x01,
        //!authentication method described in rfc2617
        http            = 0x02,
        //!base on \a authinfo cookie
        cookie          = 0x04,
        //!authentication by X-NetworkOptix-VideoWall header
        //TODO #ak remove this value from here
        videowall       = 0x08,
        //!Authentication by url query parameters \a auth and \a proxy_auth
        /*!
            params has following format: BASE64-URL( UTF8(username) ":" MD5( LATIN1(username) ":NetworkOptix:" LATIN1(password) ) )
            TODO #ak this method is too poor, have to introduce some salt
        */
        urlQueryParam   = 0x10
    };
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

class HttpAuthenticationClientContext
{
public:
    boost::optional<nx_http::header::WWWAuthenticate> authenticateHeader;
    int responseStatusCode;

    HttpAuthenticationClientContext()
    :
        responseStatusCode( nx_http::StatusCode::ok )
    {
    }

    void clear()
    {
        authenticateHeader.reset();
        responseStatusCode = nx_http::StatusCode::ok;
    }
};

class QnAuthHelper: public QObject
{
    Q_OBJECT

public:
    static const QString REALM;

    QnAuthHelper();
    virtual ~QnAuthHelper();

    static void initStaticInstance(QnAuthHelper* instance);
    static QList<QByteArray> smartSplit(const QByteArray& data, const char delimiter);
    static QnAuthHelper* instance();

    //!Authenticates request on server side
    bool authenticate(const nx_http::Request& request, nx_http::Response& response, bool isProxy = false, QnUuid* authUserId = 0, bool* authInProgress = 0);
    //!Authenticates request on client side
    /*!
        Usage:\n
        - client sends request with no authentication information
        - client receives response with 401 or 407 status code
        - client calls this method supplying received response. This method adds necessary headers to request
        - client sends request to server
    */
    bool authenticate(
        const QAuthenticator& auth,
        const nx_http::Response& response,
        nx_http::Request* const request,
        HttpAuthenticationClientContext* const authenticationCtx );
    //!Same as above, but uses cached authentication info
    bool authenticate(
        const QAuthenticator& auth,
        nx_http::Request* const request,
        const HttpAuthenticationClientContext* const authenticationCtx );
    bool authenticate(const QString& login, const QByteArray& digest) const;

    QnAuthMethodRestrictionList* restrictionList();

    //!Calculates HA1 (see rfc 2617)
    /*!
        \warning \a realm is not used currently
    */
    static QByteArray createUserPasswordDigest(
        const QString& userName,
        const QString& password,
        const QString& realm );
    static QByteArray createHttpQueryAuthParam(
        const QString& userName,
        const QString& password,
        const QString& realm,
        const QByteArray method );

    static QByteArray symmetricalEncode(const QByteArray& data);
signals:
    void emptyDigestDetected(const QnUserResourcePtr& user, const QString& login, const QString& password);
private slots:
    void at_resourcePool_resourceAdded(const QnResourcePtr &);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &);

private:
    void addAuthHeader(nx_http::Response& responseHeaders, bool isProxy);
    QByteArray getNonce();
    bool isNonceValid(const QByteArray& nonce) const;
    bool isCookieNonceValid(const QByteArray& nonce);
    bool doDigestAuth(const QByteArray& method, const QByteArray& authData, nx_http::Response& responseHeaders, bool isProxy, QnUuid* authUserId, bool* authInProgress, char delimiter, 
                      std::function<bool(const QByteArray&)> checkNonceFunc);
    bool doBasicAuth(const QByteArray& authData, nx_http::Response& responseHeaders, QnUuid* authUserId);
    bool doCookieAuthorization(const QByteArray& method, const QByteArray& authData, nx_http::Response& responseHeaders, QnUuid* authUserId, bool* authInProgress);

    mutable QMutex m_mutex;
    static QnAuthHelper* m_instance;
    //QMap<QByteArray, QElapsedTimer> m_nonces;
    QMap<QnUuid, QnUserResourcePtr> m_users;
    QMap<QnUuid, QnMediaServerResourcePtr> m_servers;
    QnAuthMethodRestrictionList m_authMethodRestrictionList;

    QMap<qint64, qint64> m_cookieNonceCache;
    mutable QMutex m_cookieNonceCacheMutex;

    QCache<QByteArray, QElapsedTimer> m_digestNonceCache;
    mutable QMutex m_nonceMtx;

    /*!
        \param authDigest base64(username : nonce : MD5(ha1, nonce, MD5(METHOD :)))
    */
    bool authenticateByUrl( const QByteArray& authRecord, const QByteArray& method ) const;
};

#define qnAuthHelper QnAuthHelper::instance()

#endif // __QN_AUTH_HELPER__
