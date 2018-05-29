#ifndef __QN_AUTH_HELPER__
#define __QN_AUTH_HELPER__

#include <map>
#include <tuple>

#include <QtNetwork/QAuthenticator>
#include <QtCore/QMap>
#include <QElapsedTimer>
#include <QCache>

#include <nx/utils/timer_manager.h>
#include <nx/utils/uuid.h>
#include <nx/utils/singleton.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/thread/mutex.h>
#include <nx/network/http/auth_restriction_list.h>
#include <nx/network/aio/timer.h>
#include <nx/network/socket_common.h>

#include <nx/vms/auth/abstract_nonce_provider.h>
#include <nx/vms/auth/abstract_user_data_provider.h>

#include <common/common_module_aware.h>
#include <core/resource/resource_fwd.h>
#include <core/resource_access/user_access_data.h>
#include <utils/common/id.h>

#include "ldap/ldap_manager.h"

struct QnLdapDigestAuthContext;
class TimeBasedNonceProvider;
namespace nx { namespace vms { namespace cloud_integration {  struct CloudManagerGroup; } } }

class QnAuthHelper:
    public QObject,
    public QnCommonModuleAware,
    public Singleton<QnAuthHelper>
{
    Q_OBJECT

public:
    static const constexpr std::chrono::milliseconds kMaxKeyLifeTime = std::chrono::hours(1);

    QnAuthHelper(
        QnCommonModule* commonModule,
        TimeBasedNonceProvider* timeBasedNonceProvider,
        nx::vms::cloud_integration::CloudManagerGroup* cloudManagerGroup);
    virtual ~QnAuthHelper();

    //!Authenticates request on server side
    Qn::AuthResult authenticate(
        const nx::network::HostAddress& clientIp,
        const nx::network::http::Request& request,
        nx::network::http::Response& response,
        bool isProxy = false,
        Qn::UserAccessData* accessRights = 0,
        nx::network::http::AuthMethod::Value* usedAuthMethod = 0);

    Qn::AuthResult httpAuthenticate(
        const nx::network::HostAddress& clientIp,
        const nx::network::http::Request& request,
        nx::network::http::Response& response,
        bool isProxy,
        Qn::UserAccessData* accessRights,
        nx::network::http::AuthMethod::Value* usedAuthMethod);

    nx::network::http::AuthMethodRestrictionList* restrictionList();

    //!Creates query item for \a path which does not require authentication
    /*!
        Created key is valid for \a periodMillis milliseconds
        \param periodMillis cannot be greater than \a QnAuthHelper::MAX_AUTHENTICATED_ALIAS_LIFE_TIME_MS
        \note pair<query key name, key value>. Returned key is only valid for \a path
    */
    QPair<QString, QString> createAuthenticationQueryItemForPath(
        const Qn::UserAccessData& accessRights,
        const QString& path,
        std::chrono::milliseconds timeout = kMaxKeyLifeTime);

    enum class NonceProvider { automatic, local };
    QByteArray generateNonce(NonceProvider provider = NonceProvider::automatic) const;

    Qn::AuthResult doCookieAuthorization(
        const nx::network::HostAddress& clientIp,
        const QByteArray& method, const QByteArray& authData,
        const boost::optional<QByteArray>& csrfToken,
        nx::network::http::Response& responseHeaders, Qn::UserAccessData* accessRights);

    /*!
    \param authDigest base64(username : nonce : MD5(ha1, nonce, MD5(METHOD :)))
    */
    Qn::AuthResult authenticateByUrl(
        const nx::network::HostAddress& clientIp,
        const QByteArray& authRecord,
        const QByteArray& method,
        nx::network::http::Response& response,
        Qn::UserAccessData* accessRights = nullptr);

    QnLdapManager* ldapManager() const;

    nx::Buffer newCsrfToken();
    void removeCsrfToken(const nx::Buffer& token);
    bool isCsrfTokenValid(const nx::Buffer& token);

    struct LockoutOptions
    {
        size_t maxLoginFailures = 10;
        std::chrono::milliseconds accountTime = std::chrono::minutes(1);
        std::chrono::milliseconds lockoutTime = std::chrono::minutes(5);
    };

    std::optional<LockoutOptions> getLockoutOptions() const;
    void setLockoutOptions(std::optional<LockoutOptions> options);

signals:
    void emptyDigestDetected(const QnUserResourcePtr& user, const QString& login, const QString& password);

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
        nx::network::http::StringType ha1Digest;
        nx::network::http::StringType realm;
        nx::network::http::StringType cryptSha512Hash;
        nx::network::http::StringType nxUserName;

        void parse( const nx::network::http::Request& request );
        bool empty() const;
    };

    struct AccessFailureData
    {
        std::optional<std::chrono::steady_clock::time_point> lockedOut{std::nullopt};
        std::deque<std::chrono::steady_clock::time_point> failures;
    };

    /*!
        \param userRes Can be NULL
    */
    void addAuthHeader(
        nx::network::http::Response& responseHeaders,
        const QnUserResourcePtr& userRes,
        bool isProxy,
        bool isDigest = true);
    Qn::AuthResult doDigestAuth(
        const nx::network::http::RequestLine& requestLine,
        const nx::network::http::header::Authorization& authorization,
        nx::network::http::Response& responseHeaders,
        bool isProxy,
        Qn::UserAccessData* accessRights);
    Qn::AuthResult doBasicAuth(
        const QByteArray& method,
        const nx::network::http::header::Authorization& authorization,
        nx::network::http::Response& responseHeaders,
        Qn::UserAccessData* accessRights);

    std::optional<std::chrono::milliseconds> isLoginLockedOut(
        const nx::String& name, const nx::network::HostAddress& address);
    void saveLoginResult(
        const nx::String& name, const nx::network::HostAddress& address, Qn::AuthResult result);

    void authenticationExpired( const QString& path, quint64 timerID );
    QnUserResourcePtr findUserByName( const QByteArray& nxUserName ) const;
    void updateUserHashes(const QnUserResourcePtr& userResource, const QString& password);

    bool decodeBasicAuthData(const QByteArray& authData, QString* outUserName, QString* outPassword);
    bool decodeLDAPPassword(const QByteArray& hash, QString* outPassword);

    /*!
        \return \a true if password expiration timestamp has been increased
    */
    //!Check \a digest validity with external authentication service (LDAP currently)
    Qn::AuthResult checkDigestValidity(QnUserResourcePtr userResource, const QByteArray& digest );

    void setClenupTimer();
    void cleanupExpiredCsrfs();

private:
    mutable QnMutex m_mutex;
    nx::utils::TimerManager::TimerGuard m_clenupTimer;
    nx::network::http::AuthMethodRestrictionList m_authMethodRestrictionList;
    std::map<QString, TempAuthenticationKeyCtx> m_authenticatedPaths;
    nx::vms::auth::AbstractNonceProvider* m_timeBasedNonceProvider;
    nx::vms::auth::AbstractNonceProvider* m_nonceProvider;
    nx::vms::auth::AbstractUserDataProvider* m_userDataProvider;
    std::unique_ptr<QnLdapManager> m_ldap;
    std::map<nx::Buffer, std::chrono::steady_clock::time_point> m_csrfTokens;
    std::map<nx::String, std::map<nx::network::HostAddress, AccessFailureData>> m_accessFailures;
    std::optional<LockoutOptions> m_lockoutOptions = LockoutOptions();
};

#define qnAuthHelper QnAuthHelper::instance()

#endif // __QN_AUTH_HELPER__
