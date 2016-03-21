#ifndef __QN_AUTH_HELPER__
#define __QN_AUTH_HELPER__

#include <map>

#include <QAuthenticator>
#include <QtCore/QMap>
#include <QElapsedTimer>
#include <QCache>

#include <core/resource/resource_fwd.h>

#include "utils/common/id.h"
#include "utils/common/timermanager.h"
#include "utils/common/uuid.h"
#include "utils/network/http/httptypes.h"
#include "utils/thread/mutex.h"
#include "utils/network/auth_restriction_list.h"

#include "ldap/ldap_manager.h"


struct QnLdapDigestAuthContext;

class QnAuthHelper: public QObject
{
    Q_OBJECT
    
public:
    static const unsigned int MAX_AUTHENTICATION_KEY_LIFE_TIME_MS;

    QnAuthHelper();
    virtual ~QnAuthHelper();

    static void initStaticInstance(QnAuthHelper* instance);
    static QnAuthHelper* instance();

    //!Authenticates request on server side
    Qn::AuthResult authenticate(const nx_http::Request& request, nx_http::Response& response, bool isProxy = false, QnUuid* authUserId = 0, AuthMethod::Value* usedAuthMethod = 0);
    
    Qn::AuthResult authenticate(const QString& login, const QByteArray& digest) const;

    QnAuthMethodRestrictionList* restrictionList();

    //!Creates query item for \a path which does not require authentication
    /*!
        Created key is valid for \a periodMillis milliseconds
        \param periodMillis cannot be greater than \a QnAuthHelper::MAX_AUTHENTICATED_ALIAS_LIFE_TIME_MS
        \note pair<query key name, key value>. Returned key is only valid for \a path
    */
    QPair<QString, QString> createAuthenticationQueryItemForPath( const QString& path, unsigned int periodMillis );

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

    class UserDigestData
    {
    public:
        nx_http::StringType ha1Digest;
        nx_http::StringType realm;
        nx_http::StringType cryptSha512Hash;
        nx_http::StringType nxUserName;

        void parse( const nx_http::Request& request );
        bool empty() const;
    };


    /*!
        \param userRes Can be NULL
    */
    void addAuthHeader(
        nx_http::Response& responseHeaders,
        const QnUserResourcePtr& userRes,
        bool isProxy,
        bool isDigest = true);
    QByteArray getNonce();
    bool isNonceValid(const QByteArray& nonce);
    Qn::AuthResult doDigestAuth(const QByteArray& method, const QByteArray& authData, nx_http::Response& responseHeaders, bool isProxy, QnUuid* authUserId, char delimiter, 
                      std::function<bool(const QByteArray&)> checkNonceFunc, QnUserResourcePtr* const outUserResource = nullptr);
    Qn::AuthResult doBasicAuth(const QByteArray& authData, nx_http::Response& responseHeaders, QnUuid* authUserId);
    Qn::AuthResult doCookieAuthorization(const QByteArray& method, const QByteArray& authData, nx_http::Response& responseHeaders, QnUuid* authUserId);

    mutable QnMutex m_mutex;
    static QnAuthHelper* m_instance;
    //QMap<QByteArray, QElapsedTimer> m_nonces;
    QMap<QnUuid, QnUserResourcePtr> m_users;
    QMap<QnUuid, QnMediaServerResourcePtr> m_servers;
    QnAuthMethodRestrictionList m_authMethodRestrictionList;

    //map<nonce, nonce creation timestamp usec>
    QMap<qint64, qint64> m_cookieNonceCache;
    mutable QnMutex m_cookieNonceCacheMutex;

    QCache<QByteArray, QElapsedTimer> m_digestNonceCache;
    mutable QnMutex m_nonceMtx;

    std::map<QString, TempAuthenticationKeyCtx> m_authenticatedPaths;

    void authenticationExpired( const QString& path, quint64 timerID );
    /*!
        \param authDigest base64(username : nonce : MD5(ha1, nonce, MD5(METHOD :)))
    */
    Qn::AuthResult authenticateByUrl( const QByteArray& authRecord, const QByteArray& method, QnUuid* authUserId, std::function<bool(const QByteArray&)> checkNonceFunc) const;
    QnUserResourcePtr findUserByName( const QByteArray& nxUserName ) const;
    void applyClientCalculatedPasswordHashToResource(
        const QnUserResourcePtr& userResource,
        const UserDigestData& userDigestData );
    Qn::AuthResult doPasswordProlongation(QnUserResourcePtr userResource);

    /*!
        \return \a true if password expiration timestamp has been increased
    */
    //!Check \a digest validity with external authentication service (LDAP currently)
    Qn::AuthResult checkDigestValidity(QnUserResourcePtr userResource, const QByteArray& digest );

};

#define qnAuthHelper QnAuthHelper::instance()

#endif // __QN_AUTH_HELPER__
