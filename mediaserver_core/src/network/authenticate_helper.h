#ifndef __QN_AUTH_HELPER__
#define __QN_AUTH_HELPER__

#include <map>
#include <tuple>

#include <QtNetwork/QAuthenticator>
#include <QtCore/QMap>
#include <QElapsedTimer>
#include <QCache>

#include <core/resource/resource_fwd.h>

#include "utils/common/id.h"
#include <nx/utils/timer_manager.h>
#include <nx/utils/uuid.h>
#include <nx/utils/singleton.h>
#include <nx/network/http/httptypes.h>
#include <nx/utils/thread/mutex.h>
#include <nx/network/http/auth_restriction_list.h>

#include "ldap/ldap_manager.h"
#include "network/auth/abstract_nonce_provider.h"
#include "network/auth/abstract_user_data_provider.h"
#include <core/resource_access/user_access_data.h>
#include <common/common_module_aware.h>

#define USE_USER_RESOURCE_PROVIDER


struct QnLdapDigestAuthContext;
class TimeBasedNonceProvider;
struct CloudManagerGroup;

class QnAuthHelper
:
    public QObject,
    public QnCommonModuleAware,
    public Singleton<QnAuthHelper>
{
    Q_OBJECT

public:
    static const unsigned int MAX_AUTHENTICATION_KEY_LIFE_TIME_MS;

    QnAuthHelper(
        QnCommonModule* commonModule,
        TimeBasedNonceProvider* timeBasedNonceProvider,
        CloudManagerGroup* cloudManagerGroup);
    virtual ~QnAuthHelper();

    //!Authenticates request on server side
    Qn::AuthResult authenticate(
        const nx_http::Request& request,
        nx_http::Response& response,
        bool isProxy = false,
        Qn::UserAccessData* accessRights = 0,
        nx_http::AuthMethod::Value* usedAuthMethod = 0);

    nx_http::AuthMethodRestrictionList* restrictionList();

    //!Creates query item for \a path which does not require authentication
    /*!
        Created key is valid for \a periodMillis milliseconds
        \param periodMillis cannot be greater than \a QnAuthHelper::MAX_AUTHENTICATED_ALIAS_LIFE_TIME_MS
        \note pair<query key name, key value>. Returned key is only valid for \a path
    */
    QPair<QString, QString> createAuthenticationQueryItemForPath(
        const Qn::UserAccessData& accessRights,
        const QString& path,
        unsigned int periodMillis);

    static QByteArray symmetricalEncode(const QByteArray& data);

    enum class NonceProvider { automatic, local };
    QByteArray generateNonce(NonceProvider provider = NonceProvider::automatic) const;

    Qn::AuthResult doCookieAuthorization(const QByteArray& method, const QByteArray& authData, nx_http::Response& responseHeaders, Qn::UserAccessData* accessRights);

    /*!
    \param authDigest base64(username : nonce : MD5(ha1, nonce, MD5(METHOD :)))
    */
    Qn::AuthResult authenticateByUrl(
        const QByteArray& authRecord,
        const QByteArray& method,
        nx_http::Response& response,
        Qn::UserAccessData* accessRights = nullptr) const;

    bool checkUserPassword(const QnUserResourcePtr& user, const QString& password);

signals:
    void emptyDigestDetected(const QnUserResourcePtr& user, const QString& login, const QString& password);

#ifndef USE_USER_RESOURCE_PROVIDER
private slots:
    void at_resourcePool_resourceAdded(const QnResourcePtr &);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &);
#endif
private:
    class TempAuthenticationKeyCtx
    {
    public:
        nx::utils::TimerManager::TimerGuard timeGuard;
        QString path;
        Qn::UserAccessData accessRights;

        TempAuthenticationKeyCtx() {}
        TempAuthenticationKeyCtx( TempAuthenticationKeyCtx&& right )
        :
            timeGuard( std::move( right.timeGuard ) ),
            path( std::move( right.path ) ),
            accessRights(right.accessRights)
        {
        }
        TempAuthenticationKeyCtx& operator=( TempAuthenticationKeyCtx&& right )
        {
            timeGuard = std::move( right.timeGuard );
            path = std::move( right.path );
            accessRights = right.accessRights;
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
    Qn::AuthResult doDigestAuth(
        const QByteArray& method,
        const nx_http::header::Authorization& authorization,
        nx_http::Response& responseHeaders,
        bool isProxy,
        Qn::UserAccessData* accessRights);
    Qn::AuthResult doBasicAuth(
        const QByteArray& method,
        const nx_http::header::Authorization& authorization,
        nx_http::Response& responseHeaders,
        Qn::UserAccessData* accessRights);

    mutable QnMutex m_mutex;
#ifndef USE_USER_RESOURCE_PROVIDER
    QMap<QnUuid, QnUserResourcePtr> m_users;
    QMap<QnUuid, QnMediaServerResourcePtr> m_servers;
#endif
    nx_http::AuthMethodRestrictionList m_authMethodRestrictionList;
    std::map<QString, TempAuthenticationKeyCtx> m_authenticatedPaths;
    AbstractNonceProvider* m_timeBasedNonceProvider;
    AbstractNonceProvider* m_nonceProvider;
    AbstractUserDataProvider* m_userDataProvider;

    void authenticationExpired( const QString& path, quint64 timerID );
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
