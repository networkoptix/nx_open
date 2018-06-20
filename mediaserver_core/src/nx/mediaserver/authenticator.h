#pragma once

#include <deque>
#include <map>
#include <tuple>

#include <QtNetwork/QAuthenticator>
#include <QtCore/QMap>
#include <QElapsedTimer>
#include <QCache>

#include <common/common_module_aware.h>
#include <core/resource_access/user_access_data.h>
#include <core/resource/resource_fwd.h>
#include <nx/network/http/auth_restriction_list.h>
#include <nx/network/http/http_types.h>
#include <nx/network/socket_common.h>
#include <nx/network/temporary_key_keeper.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/timer_manager.h>
#include <nx/utils/uuid.h>
#include <nx/vms/auth/abstract_nonce_provider.h>
#include <nx/vms/auth/abstract_user_data_provider.h>
#include <utils/common/id.h>

#include "ldap_manager.h"

struct QnLdapDigestAuthContext;
class TimeBasedNonceProvider;
namespace nx { namespace vms { namespace cloud_integration {  struct CloudManagerGroup; } } }

namespace nx {
namespace mediaserver {

/**
 * Authenticates requests on server side.
 */
class Authenticator:
    public QObject,
    public QnCommonModuleAware
{
    Q_OBJECT

public:
    static const constexpr std::chrono::milliseconds kSessionKeyLifeTime = std::chrono::hours(1);
    static const constexpr std::chrono::milliseconds kMaxKeyLifeTime = std::chrono::hours(1);

    Authenticator(
        QnCommonModule* commonModule,
        TimeBasedNonceProvider* timeBasedNonceProvider,
        nx::vms::cloud_integration::CloudManagerGroup* cloudManagerGroup);

    struct Result
    {
          Qn::AuthResult code = Qn::Auth_Forbidden;
          Qn::UserAccessData access;
          network::http::AuthMethod::Value usedMethods = network::http::AuthMethod::NotDefined;
    };

    Result tryAllMethods(
        const nx::network::HostAddress& clientIp,
        const nx::network::http::Request& request,
        nx::network::http::Response* response,
        bool isProxy = false);

    Result tryCookie(const nx::network::http::Request& request);

    void setAccessCookie(
        const nx::network::http::Request& request,
        nx::network::http::Response* response,
        Qn::UserAccessData access);

    void removeAccessCookie(
        const nx::network::http::Request& request,
        nx::network::http::Response* response);

    nx::network::http::AuthMethodRestrictionList* restrictionList();

    /**
     * Creates query item for \a path which does not require authentication.
     * @note pair<query key name, key value>. Returned key is only valid for provided path.
     */
    QPair<QString, QString> createAuthenticationQueryItemForPath(
        const Qn::UserAccessData& accessRights,
        const QString& path,
        std::chrono::milliseconds timeout = kMaxKeyLifeTime);

    enum class NonceProvider { automatic, local };
    QByteArray generateNonce(NonceProvider provider = NonceProvider::automatic) const;

    /**
     * @param authDigest base64(username : nonce : MD5(ha1, nonce, MD5(METHOD :)))
     */
    Qn::AuthResult authenticateByUrl(
        const nx::network::HostAddress& clientIp,
        const QByteArray& authRecord,
        const QByteArray& method,
        nx::network::http::Response& response,
        Qn::UserAccessData* accessRights = nullptr);

    LdapManager* ldapManager() const;

    struct LockoutOptions
    {
        size_t maxLoginFailures = 10;
        std::chrono::milliseconds accountTime = std::chrono::minutes(5);
        std::chrono::milliseconds lockoutTime = std::chrono::minutes(1);
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

    struct AccessFailureData
    {
        std::optional<std::chrono::steady_clock::time_point> lockedOut;
        std::deque<std::chrono::steady_clock::time_point> failures;
    };

    Qn::AuthResult doAllMethods(
        const nx::network::HostAddress& clientIp,
        const nx::network::http::Request& request,
        nx::network::http::Response& response,
        bool isProxy = false,
        Qn::UserAccessData* accessRights = 0,
        nx::network::http::AuthMethod::Value* usedAuthMethod = 0);

    Qn::AuthResult doHttp(
        const nx::network::HostAddress& clientIp,
        const nx::network::http::Request& request,
        nx::network::http::Response& response,
        bool isProxy,
        Qn::UserAccessData* accessRights,
        nx::network::http::AuthMethod::Value* usedAuthMethod);

    void addAuthHeader(
        nx::network::http::Response& responseHeaders,
            const QnUserResourcePtr& userRes = {},
        bool isProxy = false,
        bool isDigest = true);

    Qn::AuthResult doDigest(
        const nx::network::http::RequestLine& requestLine,
        const nx::network::http::header::Authorization& authorization,
        nx::network::http::Response& responseHeaders,
        bool isProxy,
        Qn::UserAccessData* accessRights);

    Qn::AuthResult doBasic(
        const QByteArray& method,
        const nx::network::http::header::Authorization& authorization,
        nx::network::http::Response& responseHeaders,
        Qn::UserAccessData* accessRights);

    bool isLoginLockedOut(
        const nx::String& name, const nx::network::HostAddress& address) const;
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

private:
    struct SessionKeys: public nx::network::TemporayKeyKeeper<Qn::UserAccessData> { SessionKeys(); };
    SessionKeys m_sessionKeys;

    mutable QnMutex m_mutex;
    nx::utils::TimerManager::TimerGuard m_clenupTimer;
    nx::network::http::AuthMethodRestrictionList m_authMethodRestrictionList;
    std::map<QString, TempAuthenticationKeyCtx> m_authenticatedPaths;
    nx::vms::auth::AbstractNonceProvider* m_timeBasedNonceProvider;
    nx::vms::auth::AbstractNonceProvider* m_nonceProvider;
    nx::vms::auth::AbstractUserDataProvider* m_userDataProvider;
    std::unique_ptr<LdapManager> m_ldap;
    mutable std::map<nx::String /*userName*/, std::map<nx::network::HostAddress, AccessFailureData>>
        m_accessFailures;
    std::optional<LockoutOptions> m_lockoutOptions = LockoutOptions();
};

} // namespace mediaserver
} // namespace nx
