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

namespace nx {
namespace vms::server {

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
    static const constexpr std::chrono::milliseconds kPathKeyLifeTime = std::chrono::hours(1);

    Authenticator(
        QnCommonModule* commonModule,
        TimeBasedNonceProvider* timeBasedNonceProvider,
        nx::vms::auth::AbstractNonceProvider* cloudNonceProvider,
        nx::vms::auth::AbstractUserDataProvider* userAuthenticator);
    virtual ~Authenticator() override;

    struct Result
    {
        Qn::AuthResult code = Qn::Auth_Forbidden;
        Qn::UserAccessData access;
        nx::network::http::AuthMethod::Value usedMethods =
            nx::network::http::AuthMethod::NotDefined;

        QString toString() const;
    };

    bool isPasswordCorrect(const Qn::UserAccessData& access, const QString& password);

    Result tryAllMethods(
        const nx::network::HostAddress& clientIp,
        const nx::network::http::Request& request,
        nx::network::http::Response* response,
        bool isProxy = false);

    Result tryCookie(const nx::network::http::Request& request);

    void setAccessCookie(
        const nx::network::http::Request& request,
        nx::network::http::Response* response,
        Qn::UserAccessData access,
        bool secure);

    void removeAccessCookie(
        const nx::network::http::Request& request,
        nx::network::http::Response* response);

    /**
     * @param authRecord base64(username : nonce : MD5(ha1, nonce, MD5(METHOD :)))
     */
    Qn::AuthResult tryAuthRecord(
        const nx::network::HostAddress& clientIp,
        const QByteArray& authRecord,
        const QByteArray& method,
        nx::network::http::Response& response,
        Qn::UserAccessData* accessRights = nullptr);

    nx::network::http::AuthMethodRestrictionList* restrictionList();

    /**
     * Creates query item for \a path which does not require authentication.
     * @note pair<query key name, key value>. Returned key is only valid for provided path.
     */
    QPair<QString, QString> makeQueryItemForPath(
        const Qn::UserAccessData& accessRights,
        const QString& path);

    enum class NonceProvider { automatic, local };
    QByteArray generateNonce(NonceProvider provider = NonceProvider::automatic) const;

    AbstractLdapManager* ldapManager() const;
    void setLdapManager(std::unique_ptr<AbstractLdapManager> ldapManager);

    struct LockoutOptions
    {
        static constexpr size_t kMaxLoginFailures{5};
        static constexpr std::chrono::seconds kLockoutTimeout{30};

        size_t maxLoginFailures = kMaxLoginFailures;
        std::chrono::milliseconds accountTime = kLockoutTimeout * kMaxLoginFailures;
        std::chrono::milliseconds lockoutTime = kLockoutTimeout;
    };

    std::optional<LockoutOptions> getLockoutOptions() const;
    void setLockoutOptions(std::optional<LockoutOptions> options);

signals:
    void emptyDigestDetected(const QnUserResourcePtr& user, const QString& login, const QString& password);

private:
    struct AccessFailureData
    {
        std::optional<std::chrono::steady_clock::time_point> lockedOut;
        std::deque<std::chrono::steady_clock::time_point> failures;
    };

    // TODO: Refactor all these methods so they return Result instead of out paramiters.
    // It's also preferable to come to identical interfase, so they could be used as generics.
    Qn::AuthResult tryAllMethods(
        const nx::network::HostAddress& clientIp,
        const nx::network::http::Request& request,
        nx::network::http::Response& response,
        bool isProxy = false,
        Qn::UserAccessData* accessRights = 0,
        nx::network::http::AuthMethod::Value* usedAuthMethod = 0);

    Qn::AuthResult tryHttpMethods(
        const nx::network::HostAddress& clientIp,
        const nx::network::http::Request& request,
        nx::network::http::Response& response,
        bool isProxy,
        Qn::UserAccessData* accessRights,
        nx::network::http::AuthMethod::Value* usedAuthMethod);

    void addAuthHeader(
        nx::network::http::Response& responseHeaders,
        bool isProxy = false,
        bool isDigest = true);

    Qn::AuthResult tryHttpDigest(
        const nx::network::http::RequestLine& requestLine,
        const nx::network::http::header::Authorization& authorization,
        nx::network::http::Response& responseHeaders,
        bool isProxy,
        Qn::UserAccessData* accessRights);

    Qn::AuthResult tryHttpBasic(
        const QByteArray& method,
        const nx::network::http::header::Authorization& authorization,
        nx::network::http::Response& responseHeaders,
        Qn::UserAccessData* accessRights);

    bool isLoginLockedOut(
        const nx::String& name, const nx::network::HostAddress& address) const;
    void saveLoginResult(
        const nx::String& name, const nx::network::HostAddress& address, Qn::AuthResult result);

    QnUserResourcePtr findUserByName( const QByteArray& nxUserName ) const;
    void updateUserHashes(const QnUserResourcePtr& userResource, const QString& password);

    bool decodeBasicAuthData(const QByteArray& authData, QString* outUserName, QString* outPassword);
    bool decodeLDAPPassword(const QByteArray& hash, QString* outPassword);

    /** Check \a digest validity with external authentication service (LDAP currently). */
    Qn::AuthResult checkDigestValidity(QnUserResourcePtr userResource, const QByteArray& digest);

private:
    nx::network::http::AuthMethodRestrictionList m_authMethodRestrictionList;
    nx::vms::auth::AbstractNonceProvider* const m_timeBasedNonceProvider;
    nx::vms::auth::AbstractNonceProvider* const m_nonceProvider;
    nx::vms::auth::AbstractUserDataProvider* const m_userDataProvider;
    std::unique_ptr<AbstractLdapManager> m_ldap;

    struct SessionKeys: public nx::network::TemporaryKeyKeeper<Qn::UserAccessData> { SessionKeys(); };
    SessionKeys m_sessionKeys;

    struct PathKeys: public nx::network::TemporaryKeyKeeper<Qn::UserAccessData> { PathKeys(); };
    SessionKeys m_pathKeys;

    mutable QnMutex m_mutex;
    mutable std::map<nx::String /*userName*/, std::map<nx::network::HostAddress, AccessFailureData>>
        m_accessFailures;
    std::optional<LockoutOptions> m_lockoutOptions = LockoutOptions();
};

} // namespace vms::server
} // namespace nx
