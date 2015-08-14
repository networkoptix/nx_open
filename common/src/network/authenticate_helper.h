#ifndef __QN_AUTH_HELPER__
#define __QN_AUTH_HELPER__

#include <map>

#include <QAuthenticator>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include <QElapsedTimer>
#include <QCache>

#include <core/resource/resource_fwd.h>

#include "utils/common/id.h"
#include "utils/common/timermanager.h"
#include "utils/network/http/httptypes.h"
#include "utils/common/uuid.h"


namespace AuthMethod
{
    enum Value
    {
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

enum AuthResult
{
    Auth_OK,            // OK
    Auth_WrongLogin,    // invalid login
    Auth_WrongInternalLogin, // invalid login used for internal auth scheme
    Auth_WrongDigest,   // invalid or empty digest
    Auth_WrongPassword, // invalid password
    Auth_Forbidden      // no auth mehod found or custom auth scheme without login/password is failed
};


class QnAuthHelper: public QObject
{
    Q_OBJECT
    
public:
    static const unsigned int MAX_AUTHENTICATION_KEY_LIFE_TIME_MS;

    QnAuthHelper();
    virtual ~QnAuthHelper();

    static void initStaticInstance(QnAuthHelper* instance);
    static QList<QByteArray> smartSplit(const QByteArray& data, const char delimiter);
    static QnAuthHelper* instance();

    //!Authenticates request on server side
    AuthResult authenticate(const nx_http::Request& request, nx_http::Response& response, bool isProxy = false, QnUuid* authUserId = 0, AuthMethod::Value* usedAuthMethod = 0);
    //!Authenticates request on client side
    /*!
        Usage:\n
        - client sends request with no authentication information
        - client receives response with 401 or 407 status code
        - client calls this method supplying received response. This method adds necessary headers to request
        - client sends request to server
    */
    AuthResult authenticate(
        const QAuthenticator& auth,
        const nx_http::Response& response,
        nx_http::Request* const request,
        HttpAuthenticationClientContext* const authenticationCtx );
    //!Same as above, but uses cached authentication info
    AuthResult authenticate(
        const QAuthenticator& auth,
        nx_http::Request* const request,
        const HttpAuthenticationClientContext* const authenticationCtx );
    AuthResult authenticate(const QString& login, const QByteArray& digest) const;

    QnAuthMethodRestrictionList* restrictionList();

    //!Creates query item for \a path which does not require authentication
    /*!
        Created key is valid for \a periodMillis milliseconds
        \param periodMillis cannot be greater than \a QnAuthHelper::MAX_AUTHENTICATED_ALIAS_LIFE_TIME_MS
        \note pair<query key name, key value>. Returned key is only valid for \a path
    */
    QPair<QString, QString> createAuthenticationQueryItemForPath( const QString& path, unsigned int periodMillis );

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
        const QByteArray& method,
        QByteArray nonce = QByteArray() );

    static QByteArray symmetricalEncode(const QByteArray& data);

signals:
    void emptyDigestDetected(const QnUserResourcePtr& user, const QString& login, const QString& password);

private slots:
    void at_resourcePool_resourceAdded(const QnResourcePtr &);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &);

private:
    class TempAuthenticationKeyCtx
    {
    public:
        TimerManager::TimerGuard timeGuard;
        QString path;

        TempAuthenticationKeyCtx() {}
        TempAuthenticationKeyCtx( TempAuthenticationKeyCtx&& right ) 
        :
            timeGuard( std::move( right.timeGuard ) ),
            path( std::move( right.path ) )
        {
        }
        TempAuthenticationKeyCtx& operator=( TempAuthenticationKeyCtx&& right )
        {
            timeGuard = std::move( right.timeGuard );
            path = std::move( right.path );
            return *this;
        }

    private:
        TempAuthenticationKeyCtx( const TempAuthenticationKeyCtx& );
        TempAuthenticationKeyCtx& operator=( const TempAuthenticationKeyCtx& );
    };

    void addAuthHeader(
        nx_http::Response& responseHeaders,
        const QString& realm,
        bool isProxy );
    QByteArray getNonce();
    bool isNonceValid(const QByteArray& nonce) const;
    bool isCookieNonceValid(const QByteArray& nonce);
    AuthResult doDigestAuth(const QByteArray& method, const QByteArray& authData, nx_http::Response& responseHeaders, bool isProxy, QnUuid* authUserId, char delimiter, 
                      std::function<bool(const QByteArray&)> checkNonceFunc, QnUserResourcePtr* const outUserResource = nullptr);
    AuthResult doBasicAuth(const QByteArray& authData, nx_http::Response& responseHeaders, QnUuid* authUserId);
    AuthResult doCookieAuthorization(const QByteArray& method, const QByteArray& authData, nx_http::Response& responseHeaders, QnUuid* authUserId);

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

    std::map<QString, TempAuthenticationKeyCtx> m_authenticatedPaths;

    void authenticationExpired( const QString& path, quint64 timerID );
    /*!
        \param authDigest base64(username : nonce : MD5(ha1, nonce, MD5(METHOD :)))
    */
    AuthResult authenticateByUrl( const QByteArray& authRecord, const QByteArray& method, QnUuid* authUserId, std::function<bool(const QByteArray&)> checkNonceFunc) const;
    QnUserResourcePtr findUserByName( const QByteArray& nxUserName ) const;
};

#define qnAuthHelper QnAuthHelper::instance()

#endif // __QN_AUTH_HELPER__
