#include "authenticator.h"

#include <QtCore/QCryptographicHash>

#include <nx/kit/ini_config.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/rtsp/rtsp_types.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/log.h>
#include <nx/utils/match/wildcard.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/string.h>
#include <nx/utils/uuid.h>
#include <nx/vms/auth/generic_user_data_provider.h>
#include <nx/vms/auth/time_based_nonce_provider.h>
#include <nx/vms/cloud_integration/cloud_manager_group.h>

#include <api/app_server_connection.h>
#include <api/global_settings.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <network/authutil.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/dummy_handler.h>
#include <nx_ec/managers/abstract_user_manager.h>
#include <utils/common/app_info.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>
#include <utils/crypt/symmetrical.h>

namespace nx {
namespace network {
namespace http {

// Function overlad for clean logs.
static QString toString(const network::http::RequestLine& value)
{
    return QString::fromUtf8(value.toString()).trimmed();
}

} // namespace nx
} // namespace network
} // namespace http

namespace nx {
namespace vms::server {

// TODO: Does it make sense to move into some config?
static const bool kVerifyDigestUriWithParams = false;
static const QByteArray kCookieAuthMethod("GET");

static const qint64 LDAP_TIMEOUT = 1000000ll * 60 * 5;
static const QString COOKIE_DIGEST_AUTH(lit("Authorization=Digest"));
static const QString TEMP_AUTH_KEY_NAME = lit("authKey");

Authenticator::Authenticator(
    QnCommonModule* commonModule,
    TimeBasedNonceProvider* timeBasedNonceProvider,
    nx::vms::auth::AbstractNonceProvider* cloudNonceProvider,
    nx::vms::auth::AbstractUserDataProvider* userAuthenticator)
:
    QnCommonModuleAware(commonModule),
    m_timeBasedNonceProvider(timeBasedNonceProvider),
    m_nonceProvider(cloudNonceProvider),
    m_userDataProvider(userAuthenticator),
    m_ldap(std::make_unique<LdapManager>(commonModule))
{
    const auto settings = commonModule->globalSettings();
    const auto updateSessionTimeout =
        [this, settings = commonModule->globalSettings()]()
        {
            const auto timeout = settings->sessionTimeoutLimit();
            if (timeout == std::chrono::minutes::zero())
                m_sessionKeys.setOptions({kSessionKeyLifeTime, /*prolongLifeOnUse*/ true});
            else
                m_sessionKeys.setOptions({timeout, /*prolongLifeOnUse*/ false});
        };

    connect(
        settings, &QnGlobalSettings::sessionTimeoutChanged,
        this, updateSessionTimeout, Qt::DirectConnection);

    updateSessionTimeout();
}

Authenticator::~Authenticator()
{
    commonModule()->globalSettings()->disconnect(this);
}

QString Authenticator::Result::toString() const
{
    return lm("%1, used methods %2")
        .arg(code == Qn::Auth_OK ? access.toString() : Qn::toString(code))
        .arg((int) usedMethods, 0, 2);
}

Authenticator::Result Authenticator::tryAllMethods(
    const nx::network::HostAddress& clientIp,
    const nx::network::http::Request& request,
    nx::network::http::Response* response,
    bool isProxy)
{
    const auto sessionKey = nx::network::http::getHeaderValue(
        request.headers, Qn::EC2_RUNTIME_GUID_HEADER_NAME);

    if (!sessionKey.isEmpty())
    {
        if (auto session = m_sessionKeys.get(sessionKey))
            return {Qn::Auth_OK, std::move(*session), network::http::AuthMethod::sessionKey};
    }

    Result result;
    result.code = tryAllMethods(
        clientIp, request, *response, isProxy, &result.access, &result.usedMethods);

    if (result.code == Qn::Auth_OK && !sessionKey.isEmpty())
    {
        // TODO: Transfer user resource from internal functions instead of searching again.
        if (const auto user = resourcePool()->getResourceById<QnUserResource>(result.access.userId))
        {
            m_sessionKeys.addOrUpdate(sessionKey, result.access);
            const auto removeKey =
                [this, id = user->getId()](const QnResourcePtr& user)
                {
                    user->disconnect(this);
                    m_sessionKeys.removeIf(
                        [&](const auto& /*key*/, const auto& value, const auto& /*binding*/)
                        {
                            return value.userId == id;
                        });
                };

            connect(user.data(), &QnUserResource::permissionsChanged, this, removeKey, Qt::DirectConnection);
            connect(user.data(), &QnUserResource::userRoleChanged, this, removeKey, Qt::DirectConnection);
            connect(user.data(), &QnUserResource::enabledChanged, this, removeKey, Qt::DirectConnection);
            connect(user.data(), &QnUserResource::hashesChanged, this, removeKey, Qt::DirectConnection);
            connect(user.data(), &QnUserResource::sessionExpired, this, removeKey, Qt::DirectConnection);
        }
    }

    NX_VERBOSE(this, lm("Result %1 for %2").args(result, request.requestLine));
    return result;
}

Qn::AuthResult Authenticator::verifyPassword(
    const nx::network::HostAddress& clientIp,
    const Qn::UserAccessData& access,
    const QString& password)
{
    const auto user = commonModule()->resourcePool()
        ->getResourceById<QnUserResource>(access.userId);
    if (!user)
        return Qn::Auth_WrongLogin;

    NX_VERBOSE(this, "Check %1 password '%2' for correctness...", user,
        nx::utils::log::showPasswords() ? password : QString("******"));

    // TODO: Refactor and use direct checks instead of full HTTP authorization.
    namespace http = nx::network::http;

    static const nx::Buffer kRequestLine("GET / HTTP/1.1");
    http::Request request;
    request.requestLine.parse(kRequestLine);

    http::Response response;
    addAuthHeader(response);

    http::header::WWWAuthenticate unauthorized;
    if (!unauthorized.parse(http::getHeaderValue(response.headers, unauthorized.NAME)))
    {
        NX_ASSERT(false, user);
        return Qn::Auth_Forbidden;
    }

    http::header::DigestAuthorization digestAuthorization;
    digestAuthorization.digest->userid = user->getName().toUtf8();
    if (!http::calcDigestResponse(
        request.requestLine.method, {user->getName(), {password.toUtf8(), http::AuthTokenType::password}},
        request.requestLine.url.toString().toUtf8(), unauthorized, &digestAuthorization))
    {
        NX_ASSERT(false, user);
        return Qn::Auth_Forbidden;
    }

    request.headers.emplace(digestAuthorization.NAME, digestAuthorization.serialized());

    // Address is changed so lockout through authorized requests still happen but does not affect
    // user connections.
    const nx::network::HostAddress clientIpForLockout(clientIp.toString() + "_API");
    const auto result = tryHttpMethods(
        clientIpForLockout, request, response, /*isProxy*/ false, nullptr, nullptr);

    NX_VERBOSE(this, "Check %1 password result: %2", user, result);
    return result;
}

static const auto kCookieRuntimeGuid = Qn::EC2_RUNTIME_GUID_HEADER_NAME.toLower();

Authenticator::Result Authenticator::tryCookie(const nx::network::http::Request& request)
{
    const auto sessionKey = request.getCookieValue(kCookieRuntimeGuid);
    if (sessionKey.isEmpty())
        return {Qn::Auth_Forbidden, {}, nx::network::http::AuthMethod::NotDefined};

    // TODO: Should be replaced with request.requestLine.method == "GET"
    // as soon as GET queries do not modify anything.
    if (m_authMethodRestrictionList.getAllowedAuthMethods(request)
        && nx::network::http::AuthMethod::allowWithourCsrf)
    {
        if (auto session = m_sessionKeys.get(sessionKey))
            return {Qn::Auth_OK, std::move(*session), nx::network::http::AuthMethod::cookie};
    }

    return {Qn::Auth_InvalidCsrfToken, {}, nx::network::http::AuthMethod::cookie};
}

static void removeLegacyCookie(
    const nx::network::http::Request& request,
    nx::network::http::Response* response)
{
    for (const auto& name: {"X-runtime-guid", "auth", "nx-vms-csrf-token"})
    {
        if (!request.getCookieValue(name).isEmpty())
            response->setDeletedCookie(name);
    }
}

void Authenticator::setAccessCookie(
    const nx::network::http::Request& request,
    nx::network::http::Response* response,
    Qn::UserAccessData access,
    bool secure)
{
    removeLegacyCookie(request, response);

    const auto sessionKey = request.getCookieValue(kCookieRuntimeGuid);
    if (!sessionKey.isEmpty())
        return m_sessionKeys.addOrUpdate(sessionKey, access); //< Use existing if possible.

    const auto newSessionKey = m_sessionKeys.make(access);
    response->setCookie(kCookieRuntimeGuid, newSessionKey, "/", secure);
}

void Authenticator::removeAccessCookie(
    const nx::network::http::Request& request,
    nx::network::http::Response* response)
{
    removeLegacyCookie(request, response);

    const auto sessionKey = request.getCookieValue(kCookieRuntimeGuid);
    if (sessionKey.isEmpty())
        return;

    m_sessionKeys.remove(sessionKey);
    response->setDeletedCookie(kCookieRuntimeGuid);
}

Qn::AuthResult Authenticator::tryAllMethods(
    const nx::network::HostAddress& clientIp,
    const nx::network::http::Request& request,
    nx::network::http::Response& response,
    bool isProxy,
    Qn::UserAccessData* accessRights,
    nx::network::http::AuthMethod::Value* usedAuthMethod)
{
    if (accessRights)
        *accessRights = Qn::UserAccessData();
    if (usedAuthMethod)
        *usedAuthMethod = nx::network::http::AuthMethod::noAuth;

    const QUrlQuery urlQuery(request.requestLine.url.query());

    const unsigned int allowedAuthMethods = m_authMethodRestrictionList.getAllowedAuthMethods(request);
    if (allowedAuthMethods == 0)
        return Qn::Auth_Forbidden;   //NOTE assert?

    NX_VERBOSE(this, lm("Authenticating %1. Allowed auth methods 0b%2")
        .arg(request.requestLine).arg(allowedAuthMethods, 0, 2));

    if (allowedAuthMethods & nx::network::http::AuthMethod::noAuth)
    {
        NX_VERBOSE(this, lm("Authenticated %1 by noauth").arg(request.requestLine));
        return Qn::Auth_OK;
    }

    const auto tempAuthKey = urlQuery.queryItemValue(TEMP_AUTH_KEY_NAME).toUtf8();
    if (!tempAuthKey.isEmpty())
    {
        const auto path = request.requestLine.url.path().toUtf8();
        if (const auto access = m_pathKeys.get(tempAuthKey, path))
        {
            if (usedAuthMethod)
                *usedAuthMethod = nx::network::http::AuthMethod::temporaryUrlQueryKey;

            if (accessRights)
                *accessRights = std::move(*access);

            return Qn::Auth_OK;
        }
    }

    if (allowedAuthMethods & nx::network::http::AuthMethod::videowallHeader)
    {
        const nx::network::http::StringType& videoWall_auth = nx::network::http::getHeaderValue(request.headers, Qn::VIDEOWALL_GUID_HEADER_NAME);
        if (!videoWall_auth.isEmpty())
        {
            NX_VERBOSE(this, lm("Authenticating %1 by video wall key %2")
                .arg(request.requestLine.url).arg(videoWall_auth));

            if (usedAuthMethod)
                *usedAuthMethod = nx::network::http::AuthMethod::videowallHeader;

            if (resourcePool()->getResourceById<QnVideoWallResource>(QnUuid(videoWall_auth)).isNull())
            {
                NX_VERBOSE(this, lm("Failed to authenticate %1 by video wall key %2")
                    .arg(request.requestLine.url).arg(videoWall_auth));
                return Qn::Auth_Forbidden;
            }
            else
            {
                NX_VERBOSE(this, lm("Authenticated %1 by video wall key %2")
                    .arg(request.requestLine.url).arg(videoWall_auth));

                if (accessRights)
                    *accessRights = Qn::kVideowallUserAccess;
                return Qn::Auth_OK;
            }
        }
    }

    if (allowedAuthMethods & nx::network::http::AuthMethod::urlQueryDigest)
    {
        const QByteArray& authQueryParam = urlQuery.queryItemValue(
            isProxy ? lit("proxy_auth") : QString::fromLatin1(Qn::URL_QUERY_AUTH_KEY_NAME)).toLatin1();
        if (!authQueryParam.isEmpty())
        {
            NX_VERBOSE(this, lm("Authenticating %1 by auth query item")
                .arg(request.requestLine));

            auto authResult = tryAuthRecord(
                clientIp,
                authQueryParam,
                request.requestLine.version.protocol == nx::network::rtsp::rtsp_1_0.protocol
                ? "PLAY"    //for rtsp always using PLAY since client software does not know
                            //which request underlying player will issue first
                : request.requestLine.method,
                response,
                accessRights);
            if (authResult == Qn::Auth_OK)
            {
                if (usedAuthMethod)
                    *usedAuthMethod = nx::network::http::AuthMethod::urlQueryDigest;
                NX_DEBUG(this, lm("%1 with urlQueryDigest (%2)").args(Qn::Auth_OK, request.requestLine));
                return Qn::Auth_OK;
            }
        }
    }

    auto result = Qn::Auth_Forbidden;
    if (allowedAuthMethods & nx::network::http::AuthMethod::cookie)
    {
        const auto cookieResult = tryCookie(request);
        if (usedAuthMethod)
            *usedAuthMethod = cookieResult.usedMethods;

        if (accessRights)
            *accessRights = cookieResult.access;

        result = cookieResult.code;
        if (result == Qn::Auth_OK)
            return result;
    }

    if (allowedAuthMethods & nx::network::http::AuthMethod::http)
    {
        NX_VERBOSE(this, lm("Authenticating %1 with HTTP authentication")
            .arg(request.requestLine));

        result = tryHttpMethods(clientIp, request, response, isProxy, accessRights, usedAuthMethod);
        if (result == Qn::Auth_OK)
        {
            NX_VERBOSE(this, lm("Authenticated %1 with HTTP authentication").args(request.requestLine.url));
            return result;
        }
        else
        {
            NX_VERBOSE(this, lm("Failed to authenticate %1 with HTTP authentication: %2")
                .args(request.requestLine.url, result));
        }
    }

    NX_VERBOSE(this, lm("Failed to authenticate %1 with any method").arg(request.requestLine.url));
    return result;
}

Qn::AuthResult Authenticator::tryHttpMethods(
    const nx::network::HostAddress& clientIp,
    const nx::network::http::Request& request,
    nx::network::http::Response& response,
    bool isProxy,
    Qn::UserAccessData* accessRights,
    nx::network::http::AuthMethod::Value* usedAuthMethod)
{
    const nx::network::http::StringType& authorization = isProxy
        ? nx::network::http::getHeaderValue(request.headers, "Proxy-Authorization")
        : nx::network::http::getHeaderValue(request.headers, "Authorization");
    const nx::network::http::StringType nxUserName = nx::network::http::getHeaderValue(request.headers, Qn::CUSTOM_USERNAME_HEADER_NAME);
    if (authorization.isEmpty())
    {
        NX_VERBOSE(this, lm("Authenticating %1. Authorization header not found")
            .arg(request.requestLine));

        Qn::AuthResult authResult = Qn::Auth_WrongDigest;
        if (usedAuthMethod)
            *usedAuthMethod = nx::network::http::AuthMethod::httpDigest;
        QnUserResourcePtr userResource;
        if (!nxUserName.isEmpty())
        {
            userResource = findUserByName(nxUserName);
            if (userResource)
            {
                NX_VERBOSE(this, lm("Authenticating %1. Found Nx user %2. Checking realm...")
                    .arg(request.requestLine).arg(nxUserName));

                QString desiredRealm = nx::network::AppInfo::realm();

                bool canUpdateRealm = request.headers.find(Qn::CUSTOM_CHANGE_REALM_HEADER_NAME) != request.headers.end();
                bool needUpdateRealm = userResource->isLocal() && userResource->getRealm() != desiredRealm;
                if (canUpdateRealm && needUpdateRealm)
                {
                    //requesting client to re-calculate digest after upgrade to 2.4
                    nx::network::http::insertOrReplaceHeader(
                        &response.headers,
                        nx::network::http::HttpHeader(Qn::REALM_HEADER_NAME, desiredRealm.toLatin1()));
                }
                bool needRecalcPassword = (needUpdateRealm && canUpdateRealm)
                     || (userResource->isLdap() && userResource->passwordExpired());
                if (needRecalcPassword)
                {
                    //  Request basic auth to recalculate digest or fill ldap password
                    addAuthHeader(
                        response,
                        isProxy,
                        false); //< Requesting Basic authorization.
                    return authResult;
                }
            }
        }

        addAuthHeader(
            response,
            isProxy);

        NX_DEBUG(this, lm("%1 requesting digest auth (%2)").args(authResult, request.requestLine));
        return authResult;
    }

    nx::network::http::header::Authorization authorizationHeader;
    if (!authorizationHeader.parse(authorization))
    {
        NX_VERBOSE(this, lm("Failed to authenticate %1 with HTTP authentication. "
            "Error parsing Authorization header").arg(request.requestLine.url));
        return Qn::Auth_Forbidden;
    }

    if (isLoginLockedOut(authorizationHeader.userid(), clientIp))
        return Qn::Auth_LockedOut;

    //TODO #ak better call m_userDataProvider->authorize here
    QnUserResourcePtr userResource = findUserByName(authorizationHeader.userid());

    // Extra step for LDAP authentication

    if (userResource && userResource->isLdap() && userResource->passwordExpired())
    {
        NX_VERBOSE(this, lm("Authenticating %1. Authentication LDAP user %2")
            .arg(request.requestLine).arg(userResource->getName()));

        // Check user password on LDAP server
        QString password;
        if (authorizationHeader.authScheme == nx::network::http::header::AuthScheme::basic)
        {
            password = QString::fromUtf8(authorizationHeader.basic->password);
        }
        else if (authorizationHeader.authScheme == nx::network::http::header::AuthScheme::digest)
        {
            password = userResource->decodeLDAPPassword();
            if (password.isEmpty())
                return Qn::Auth_Forbidden; //< can't perform digest auth for LDAP user yet
        }

        auto authResult = m_ldap->authenticate(userResource->getName(), password);

        if ((authResult == Qn::Auth_WrongPassword ||
            authResult == Qn::Auth_WrongDigest ||
            authResult == Qn::Auth_WrongLogin) &&
            authorizationHeader.authScheme == nx::network::http::header::AuthScheme::digest)
        {
            if (tryHttpDigest(request.requestLine,
                authorizationHeader, response, isProxy, accessRights) == Qn::Auth_OK)
            {
                // Cached value matched user digest but not LDAP server.
                // Reset password in database to force user to relogin.
                updateUserHashes(userResource, QString());
            }
        }

        if (authResult != Qn::Auth_OK)
        {
            saveLoginResult(authorizationHeader.userid(), clientIp, authResult);
            return authResult;
        }

        updateUserHashes(userResource, password); //< update stored LDAP password/hash if need
        userResource->prolongatePassword();
    }

    // Standard authentication

    Qn::AuthResult authResult = Qn::Auth_Forbidden;
    if (authorizationHeader.authScheme == nx::network::http::header::AuthScheme::digest)
    {
        if (usedAuthMethod)
            *usedAuthMethod = nx::network::http::AuthMethod::httpDigest;

        authResult = tryHttpDigest(
            request.requestLine, authorizationHeader, response, isProxy, accessRights);
        NX_DEBUG(this, lm("%1 with digest (%2)").args(authResult, request.requestLine));
    }
    else if (authorizationHeader.authScheme == nx::network::http::header::AuthScheme::basic)
    {
        if (usedAuthMethod)
            *usedAuthMethod = nx::network::http::AuthMethod::httpBasic;
        authResult = tryHttpBasic(request.requestLine.method, authorizationHeader, response, accessRights);

        if (authResult == Qn::Auth_OK && userResource &&
            (userResource->getDigest().isEmpty() || userResource->getRealm() != nx::network::AppInfo::realm()))
        {
            updateUserHashes(userResource, QString::fromUtf8(authorizationHeader.basic->password));
        }
    }
    else
    {
        if (usedAuthMethod)
            *usedAuthMethod = nx::network::http::AuthMethod::httpBasic;
        authResult = Qn::Auth_Forbidden;
    }

    if (authResult == Qn::Auth_OK)
    {
        NX_VERBOSE(this, lm("Authenticating %1. Fetching access rights").arg(request.requestLine));

        // update user information if authorization by server authKey and user-name is specified
        if (accessRights &&
            resourcePool()->getResourceById<QnMediaServerResource>(accessRights->userId))
        {
            *accessRights = Qn::kSystemAccess;
            auto itr = request.headers.find(Qn::CUSTOM_USERNAME_HEADER_NAME);
            if (itr != request.headers.end())
            {
                auto userRes = findUserByName(itr->second);
                if (userRes)
                    *accessRights = Qn::UserAccessData(userRes->getId());
            }
        }
    }

    saveLoginResult(authorizationHeader.userid(), clientIp, authResult);
    return authResult;
}

nx::network::http::AuthMethodRestrictionList* Authenticator::restrictionList()
{
    return &m_authMethodRestrictionList;
}

QPair<QString, QString> Authenticator::makeQueryItemForPath(
    const Qn::UserAccessData& accessRights,
    const QString& path)
{
    return {TEMP_AUTH_KEY_NAME, m_pathKeys.make(accessRights, path.toUtf8())};
}

bool Authenticator::isLoginLockedOut(
    const nx::String& name, const nx::network::HostAddress& address) const
{
    const auto now = std::chrono::steady_clock::now();

    QnMutexLocker lock(&m_mutex);
    if (!m_lockoutOptions)
    {
        m_accessFailures.clear();
        return false;
    }

    auto& userData = m_accessFailures[name];
    for (auto it = userData.begin(); it != userData.end(); )
    {
        auto& ipData = it->second;
        while (!ipData.failures.empty() && ipData.failures.front() + m_lockoutOptions->accountTime <= now)
            ipData.failures.pop_front(); //< Remove outdated failures.

        if (ipData.lockedOut && *ipData.lockedOut + m_lockoutOptions->lockoutTime <= now)
            ipData.lockedOut = std::nullopt; //< Remove expired lockout.

        if (!ipData.lockedOut && ipData.failures.empty())
            it = userData.erase(it); //< Remove useless record.
        else
            ++it;
    }

    const auto ipIt = userData.find(address);
    if (ipIt == userData.end() || !ipIt->second.lockedOut)
        return false;

    NX_VERBOSE(this, "User '%1' from %2 is locked out for about %3", name, address,
        std::chrono::duration_cast<std::chrono::seconds>(
            ipIt->second.failures.front() + m_lockoutOptions->lockoutTime - now));

    return true;
}

void Authenticator::saveLoginResult(
    const nx::String& name, const nx::network::HostAddress& address, Qn::AuthResult result)
{
    if (result != Qn::Auth_WrongPassword)
        return; //< Only password traversal should cause lock out.

    QnMutexLocker lock(&m_mutex);
    if (!m_lockoutOptions)
        return;

    auto& data = m_accessFailures[name][address];
    if (!data.lockedOut)
    {
        if (data.failures.size() + 1 >= m_lockoutOptions->maxLoginFailures)
        {
            NX_DEBUG(this, lm("Lockout for %1 from %2 for %3")
                .args(name, address, m_lockoutOptions->lockoutTime));

            data.lockedOut = std::chrono::steady_clock::now();
            data.failures.clear();
        }
        else
        {
            NX_VERBOSE(this, lm("Record login failure for %1 from %2").args(name, address));
            data.failures.push_back(std::chrono::steady_clock::now());
        }
    }
}

static bool verifyDigestUri(const nx::utils::Url& requestUrl, const QByteArray& uri)
{
    static const auto isEqual =
        [](const QString& request, const QString& digest)
        {
            if (request == digest)
                return true;

            // TODO: #ak Remove in 4.0 and fix another way on cloud side.
            // Currently VmsGateway proxies queries like /gateway/<SYSTEM_ID>/<QUERY> to mediaserver
            // as is with just just /<QUERY>, see VMS-9360.
            static const auto kGatewayPrefix = lit("/gateway/");
            if (digest.startsWith(kGatewayPrefix))
            {
                const auto proxiedPathStart = digest.indexOf(QChar::fromLatin1('/'), kGatewayPrefix.size());
                if (proxiedPathStart > 0 && request == digest.mid(proxiedPathStart))
                    return true;
            }

            return false;
        };

    const nx::utils::Url digestUrl(QString::fromUtf8(uri));
    return kVerifyDigestUriWithParams
        ? isEqual(requestUrl.query(), digestUrl.query())
        : isEqual(requestUrl.path(), digestUrl.path());
}

Qn::AuthResult Authenticator::tryHttpDigest(
    const nx::network::http::RequestLine& requestLine,
    const nx::network::http::header::Authorization& authorization,
    nx::network::http::Response& responseHeaders,
    bool isProxy,
    Qn::UserAccessData* accessRights)
{
    const QByteArray userName = authorization.digest->userid;
    const QByteArray response = authorization.digest->params["response"];
    const QByteArray nonce = authorization.digest->params["nonce"];
    const QByteArray realm = authorization.digest->params["realm"];
    const QByteArray uri = authorization.digest->params["uri"];

    if (nonce.isEmpty() || userName.isEmpty() || !verifyDigestUri(requestLine.url, uri))
        return Qn::Auth_WrongDigest;

    Qn::AuthResult errCode = Qn::Auth_WrongDigest;
    if (m_nonceProvider->isNonceValid(nonce))
    {
        errCode = Qn::Auth_WrongLogin;

        QnResourcePtr res;
        std::tie(errCode, res) = m_userDataProvider->authorize(
            requestLine.method,
            authorization,
            &responseHeaders.headers);

        if (res && accessRights)
            *accessRights = Qn::UserAccessData(res->getId());

        if (errCode == Qn::Auth_OK)
            return Qn::Auth_OK;
    }

    addAuthHeader(
        responseHeaders,
        isProxy);

    if (errCode == Qn::Auth_WrongLogin && !QUuid(userName).isNull())
        errCode = Qn::Auth_WrongInternalLogin;

    return errCode;
}

Qn::AuthResult Authenticator::tryHttpBasic(
    const QByteArray& method,
    const nx::network::http::header::Authorization& authorization,
    nx::network::http::Response& response,
    Qn::UserAccessData* accessRights)
{
    NX_ASSERT(authorization.authScheme == nx::network::http::header::AuthScheme::basic);

    Qn::AuthResult errCode = Qn::Auth_WrongLogin;

    QnResourcePtr res;
    std::tie(errCode, res) = m_userDataProvider->authorize(
        method,
        authorization,
        &response.headers);
    if (auto user = res.dynamicCast<QnUserResource>())
    {
        if (accessRights)
            *accessRights = Qn::UserAccessData(user->getId());
    }
    else if (auto server = res.dynamicCast<QnMediaServerResource>())
    {
        if (accessRights)
            *accessRights = Qn::UserAccessData(server->getId());
    }

    if (errCode == Qn::Auth_OK)
    {
        if (auto user = res.dynamicCast<QnUserResource>(); user && user->getDigest().isEmpty())
        {
                const auto name = QString::fromUtf8(authorization.basic->userid);
                const auto password = QString::fromUtf8(authorization.basic->password);
                emit emptyDigestDetected(user, name, password);
        }
        return Qn::Auth_OK;
    }

    return errCode;
}

void Authenticator::addAuthHeader(
    nx::network::http::Response& response,
    bool isProxy,
    bool isDigest)
{
    const QString realm = nx::network::AppInfo::realm();

    const QString auth =
        isDigest
        ? lit("Digest realm=\"%1\", nonce=\"%2\", algorithm=MD5")
            .arg(realm)
            .arg(QLatin1String(m_nonceProvider->generateNonce()))
        : lit("Basic realm=\"%1\"").arg(realm);

    const QByteArray headerName = isProxy ? "Proxy-Authenticate" : "WWW-Authenticate";
    nx::network::http::insertOrReplaceHeader(&response.headers, nx::network::http::HttpHeader(
        headerName,
        auth.toLatin1()));
}

QByteArray Authenticator::generateNonce(NonceProvider provider) const
{
    if (provider == NonceProvider::automatic)
        return m_nonceProvider->generateNonce();
    else
        return m_timeBasedNonceProvider->generateNonce();
}

Qn::AuthResult Authenticator::tryAuthRecord(
    const nx::network::HostAddress& clientIp,
    const QByteArray& authRecordBase64,
    const QByteArray& method,
    nx::network::http::Response& response,
    Qn::UserAccessData* accessRights)
{
    auto authRecord = QByteArray::fromBase64(authRecordBase64);
    auto authFields = authRecord.split(':');
    if (authFields.size() != 3)
        return Qn::Auth_WrongDigest;

    nx::network::http::header::Authorization authorization(nx::network::http::header::AuthScheme::digest);
    authorization.digest->userid = authFields[0];
    authorization.digest->params["response"] = authFields[2];
    authorization.digest->params["nonce"] = authFields[1];
    authorization.digest->params["realm"] = nx::network::AppInfo::realm().toUtf8();
    //digestAuthParams.params["uri"];   uri is empty

    if (isLoginLockedOut(authorization.digest->userid, clientIp))
        return Qn::Auth_LockedOut;

    if (!m_nonceProvider->isNonceValid(authorization.digest->params["nonce"]))
        return Qn::Auth_WrongDigest;

    auto [errorCode, resource] = m_userDataProvider->authorize(
        method,
        authorization,
        &response.headers);

    if (const auto user = resource.dynamicCast<QnUserResource>())
    {
        if (accessRights)
            *accessRights = Qn::UserAccessData(user->getId());
    }

    saveLoginResult(authorization.digest->userid, clientIp, errorCode);
    return errorCode;
}

QnUserResourcePtr Authenticator::findUserByName(const QByteArray& nxUserName) const
{
    auto res = m_userDataProvider->findResByName(nxUserName);
    if (auto user = res.dynamicCast<QnUserResource>())
        return user;
    return QnUserResourcePtr();
}

void Authenticator::updateUserHashes(const QnUserResourcePtr& userResource, const QString& password)
{
    if (userResource->isLdap() && userResource->decodeLDAPPassword() == password)
        return; //< password is not changed

    userResource->setRealm(nx::network::AppInfo::realm());
    userResource->setPasswordAndGenerateHash(password);

    nx::vms::api::UserData userData;
    ec2::fromResourceToApi(userResource, userData);
    commonModule()->ec2Connection()->getUserManager(Qn::kSystemAccess)->save(
        userData,
        QString(),
        ec2::DummyHandler::instance(),
        &ec2::DummyHandler::onRequestDone);
}

AbstractLdapManager* Authenticator::ldapManager() const
{
    return m_ldap.get();
}

void Authenticator::setLdapManager(std::unique_ptr<AbstractLdapManager> ldapManager)
{
    m_ldap = std::move(ldapManager);
}

std::optional<Authenticator::LockoutOptions> Authenticator::getLockoutOptions() const
{
    QnMutexLocker lock(&m_mutex);
    return m_lockoutOptions;
}

void Authenticator::setLockoutOptions(std::optional<LockoutOptions> options)
{
    QnMutexLocker lock(&m_mutex);
    m_lockoutOptions = std::move(options);
    if (!m_lockoutOptions)
        m_accessFailures.clear();
}

Authenticator::SessionKeys::SessionKeys():
    nx::network::TemporaryKeyKeeper<Qn::UserAccessData>(
        {kSessionKeyLifeTime, /*prolongLifeOnUse*/ true})
{
}

Authenticator::PathKeys::PathKeys():
    nx::network::TemporaryKeyKeeper<Qn::UserAccessData>(
        {kPathKeyLifeTime, /*prolongLifeOnUse*/ false})
{
}

} // namespace vms::server
} // namespace nx
