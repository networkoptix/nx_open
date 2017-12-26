
#include "authenticate_helper.h"

#include <QtCore/QCryptographicHash>

#include <utils/crypt/symmetrical.h>
#include <utils/common/app_info.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/uuid.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include "utils/common/util.h"
#include "utils/common/synctime.h"
#include <nx/network/simple_http_client.h>
#include <nx/utils/match/wildcard.h>
#include "api/app_server_connection.h"
#include "common/common_module.h"
#include "core/resource/media_server_resource.h"
#include <nx/network/http/custom_headers.h>
#include "ldap/ldap_manager.h"
#include "network/authutil.h"
#include <nx_ec/dummy_handler.h>
#include <ldap/ldap_manager.h>
#include <nx/network/rtsp/rtsp_types.h>
#include "network/auth/time_based_nonce_provider.h"
#include "network/auth/generic_user_data_provider.h"
#include "network/auth/cdb_nonce_fetcher.h"
#include "network/auth/cloud_user_authenticator.h"

#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/managers/abstract_user_manager.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/string.h>
#include <nx/utils/log/log.h>

#include <nx/kit/ini_config.h>

#include "cloud/cloud_manager_group.h"

////////////////////////////////////////////////////////////
//// class QnAuthHelper
////////////////////////////////////////////////////////////

// TODO: Does it make sense to move into some config?
static const bool kVerifyDigestUriWithParams = false;

void QnAuthHelper::UserDigestData::parse(const nx_http::Request& request)
{
    ha1Digest = nx_http::getHeaderValue(request.headers, Qn::HA1_DIGEST_HEADER_NAME);
    realm = nx_http::getHeaderValue(request.headers, Qn::REALM_HEADER_NAME);
    cryptSha512Hash = nx_http::getHeaderValue(request.headers, Qn::CRYPT_SHA512_HASH_HEADER_NAME);
    nxUserName = nx_http::getHeaderValue(request.headers, Qn::CUSTOM_USERNAME_HEADER_NAME);
}

bool QnAuthHelper::UserDigestData::empty() const
{
    return ha1Digest.isEmpty() || realm.isEmpty() || nxUserName.isEmpty() || cryptSha512Hash.isEmpty();
}


static const qint64 LDAP_TIMEOUT = 1000000ll * 60 * 5;
static const QString COOKIE_DIGEST_AUTH(lit("Authorization=Digest"));
static const QString TEMP_AUTH_KEY_NAME = lit("authKey");

const unsigned int QnAuthHelper::MAX_AUTHENTICATION_KEY_LIFE_TIME_MS = 60 * 60 * 1000;

QnAuthHelper::QnAuthHelper(
    QnCommonModule* commonModule,
    TimeBasedNonceProvider* timeBasedNonceProvider,
    CloudManagerGroup* cloudManagerGroup)
:
    QnCommonModuleAware(commonModule),
    m_timeBasedNonceProvider(timeBasedNonceProvider),
    m_nonceProvider(&cloudManagerGroup->authenticationNonceFetcher),
    m_userDataProvider(&cloudManagerGroup->userAuthenticator),
    m_ldap(new QnLdapManager(commonModule))
{
}

QnAuthHelper::~QnAuthHelper()
{
}

Qn::AuthResult QnAuthHelper::authenticate(
    const nx_http::Request& request,
    nx_http::Response& response,
    bool isProxy,
    Qn::UserAccessData* accessRights,
    nx_http::AuthMethod::Value* usedAuthMethod)
{
    if (accessRights)
        *accessRights = Qn::UserAccessData();
    if (usedAuthMethod)
        *usedAuthMethod = nx_http::AuthMethod::noAuth;

    const QUrlQuery urlQuery(request.requestLine.url.query());

    const unsigned int allowedAuthMethods = m_authMethodRestrictionList.getAllowedAuthMethods(request);
    if (allowedAuthMethods == 0)
        return Qn::Auth_Forbidden;   //NOTE assert?

    NX_VERBOSE(this, lm("Authenticating %1. Allowed auth methods 0b%2")
        .arg(request.requestLine).arg(allowedAuthMethods, 0, 2));

    if (allowedAuthMethods & nx_http::AuthMethod::noAuth)
    {
        NX_VERBOSE(this, lm("Authenticated %1 by noauth").arg(request.requestLine));
        return Qn::Auth_OK;
    }

    {
        QnMutexLocker lk(&m_mutex);
        if (urlQuery.hasQueryItem(TEMP_AUTH_KEY_NAME))
        {
            NX_VERBOSE(this, lm("Authenticating %1 by auth key").arg(request.requestLine));

            auto it = m_authenticatedPaths.find(urlQuery.queryItemValue(TEMP_AUTH_KEY_NAME));
            if (it != m_authenticatedPaths.end() &&
                it->second.path == request.requestLine.url.path())
            {
                NX_VERBOSE(this, lm("Authenticated %1 by auth key").arg(request.requestLine));

                if (usedAuthMethod)
                    *usedAuthMethod = nx_http::AuthMethod::tempUrlQueryParam;
                if (accessRights)
                    *accessRights = it->second.accessRights;
                return Qn::Auth_OK;
            }
        }
    }

    if (allowedAuthMethods & nx_http::AuthMethod::videowall)
    {
        const nx_http::StringType& videoWall_auth = nx_http::getHeaderValue(request.headers, Qn::VIDEOWALL_GUID_HEADER_NAME);
        if (!videoWall_auth.isEmpty())
        {
            NX_VERBOSE(this, lm("Authenticating %1 by video wall key %2")
                .arg(request.requestLine.url).arg(videoWall_auth));

            if (usedAuthMethod)
                *usedAuthMethod = nx_http::AuthMethod::videowall;

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

    if (allowedAuthMethods & nx_http::AuthMethod::urlQueryParam)
    {
        const QByteArray& authQueryParam = urlQuery.queryItemValue(
            isProxy ? lit("proxy_auth") : QString::fromLatin1(Qn::URL_QUERY_AUTH_KEY_NAME)).toLatin1();
        if (!authQueryParam.isEmpty())
        {
            NX_VERBOSE(this, lm("Authenticating %1 by auth query item")
                .arg(request.requestLine));

            auto authResult = authenticateByUrl(
                authQueryParam,
                request.requestLine.version.protocol == nx_rtsp::rtsp_1_0.protocol
                ? "PLAY"    //for rtsp always using PLAY since client software does not know
                            //which request underlying player will issue first
                : request.requestLine.method,
                response,
                accessRights);
            if (authResult == Qn::Auth_OK)
            {
                if (usedAuthMethod)
                    *usedAuthMethod = nx_http::AuthMethod::urlQueryParam;
                NX_DEBUG(this, lm("%1 with urlQueryParam (%2)").args(Qn::Auth_OK, request.requestLine));
                return Qn::Auth_OK;
            }
        }
    }

    if (allowedAuthMethods & nx_http::AuthMethod::cookie)
    {
        const QString& cookie = QLatin1String(nx_http::getHeaderValue(request.headers, "Cookie"));
        int customAuthInfoPos = cookie.indexOf(Qn::URL_QUERY_AUTH_KEY_NAME);
        if (customAuthInfoPos >= 0)
        {
            if (usedAuthMethod)
                *usedAuthMethod = nx_http::AuthMethod::cookie;
            const auto result = doCookieAuthorization("GET", cookie.toUtf8(), response, accessRights);
            NX_DEBUG(this, lm("%1 with cookie (%2)").args(result, request.requestLine));
            return result;
        }
    }

    if (allowedAuthMethods & nx_http::AuthMethod::http)
    {
        NX_VERBOSE(this, lm("Authenticating %1 with HTTP authentication")
            .arg(request.requestLine));

        const nx_http::StringType& authorization = isProxy
            ? nx_http::getHeaderValue(request.headers, "Proxy-Authorization")
            : nx_http::getHeaderValue(request.headers, "Authorization");
        const nx_http::StringType nxUserName = nx_http::getHeaderValue(request.headers, Qn::CUSTOM_USERNAME_HEADER_NAME);
        bool canUpdateRealm = request.headers.find(Qn::CUSTOM_CHANGE_REALM_HEADER_NAME) != request.headers.end();
        if (authorization.isEmpty())
        {
            NX_VERBOSE(this, lm("Authenticating %1. Authorization header not found")
                .arg(request.requestLine));

            Qn::AuthResult authResult = Qn::Auth_WrongDigest;
            if (usedAuthMethod)
                *usedAuthMethod = nx_http::AuthMethod::httpDigest;
            QnUserResourcePtr userResource;
            if (!nxUserName.isEmpty())
            {
                userResource = findUserByName(nxUserName);
                if (userResource)
                {
                    NX_VERBOSE(this, lm("Authenticating %1. Found Nx user %2. Checking realm...")
                        .arg(request.requestLine).arg(nxUserName));

                    QString desiredRealm = nx::network::AppInfo::realm();
                    bool needRecalcPassword =
                        userResource->getRealm() != desiredRealm ||
                        (userResource->isLdap() && userResource->passwordExpired()) ||
                        (userResource->getDigest().isEmpty() && !userResource->isCloud());
                    if (canUpdateRealm && needRecalcPassword)
                    {
                        //requesting client to re-calculate digest after upgrade to 2.4 or fill ldap password
                        nx_http::insertOrReplaceHeader(
                            &response.headers,
                            nx_http::HttpHeader(Qn::REALM_HEADER_NAME, desiredRealm.toLatin1()));

                        addAuthHeader(
                            response,
                            userResource,
                            isProxy,
                            false); //requesting Basic authorization
                            return authResult;
                    }
                }
            }
            else {
                // use admin's realm by default for better compatibility with previous version
                // in case of default realm upgrade
                userResource = resourcePool()->getAdministrator();
            }

            addAuthHeader(
                response,
                userResource,
                isProxy);
            NX_DEBUG(this, lm("%1 requesting digest auth (%2)").args(authResult, request.requestLine));
            return authResult;
        }

        nx_http::header::Authorization authorizationHeader;
        if (!authorizationHeader.parse(authorization))
        {
            NX_VERBOSE(this, lm("Failed to authenticate %1 with HTTP authentication. "
                "Error parsing Authorization header").arg(request.requestLine.url));
            return Qn::Auth_Forbidden;
        }
        //TODO #ak better call m_userDataProvider->authorize here
        QnUserResourcePtr userResource = findUserByName(authorizationHeader.userid());

        // Extra step for LDAP authentication

        if (userResource && userResource->isLdap() && userResource->passwordExpired())
        {
            NX_VERBOSE(this, lm("Authenticating %1. Authentication LDAP user %2")
                .arg(request.requestLine).arg(userResource->getName()));

            // Check user password on LDAP server
            QString password;
            if (authorizationHeader.authScheme == nx_http::header::AuthScheme::basic)
            {
                password = authorizationHeader.basic->password;
            }
            else if (authorizationHeader.authScheme == nx_http::header::AuthScheme::digest)
            {
                password = userResource->decodeLDAPPassword();
                if (password.isEmpty())
                    return Qn::Auth_Forbidden; //< can't perform digest auth for LDAP user yet
            }

            auto authResult = m_ldap->authenticate(userResource->getName(), password);

            if ((authResult == Qn::Auth_WrongPassword ||
                authResult == Qn::Auth_WrongDigest ||
                authResult == Qn::Auth_WrongLogin) &&
                authorizationHeader.authScheme == nx_http::header::AuthScheme::digest)
            {
                if (doDigestAuth(request.requestLine,
                    authorizationHeader, response, isProxy, accessRights) == Qn::Auth_OK)
                {
                    // Cached value matched user digest by not LDAP server.
                    // Reset password in database to force user to relogin.
                    updateUserHashes(userResource, QString());
                }
            }

            if (authResult != Qn::Auth_OK)
                return authResult;
            updateUserHashes(userResource, password); //< update stored LDAP password/hash if need
            userResource->prolongatePassword();
        }

        // Standard authentication

        Qn::AuthResult authResult = Qn::Auth_Forbidden;
        if (authorizationHeader.authScheme == nx_http::header::AuthScheme::digest)
        {
            if (usedAuthMethod)
                *usedAuthMethod = nx_http::AuthMethod::httpDigest;

            authResult = doDigestAuth(
                request.requestLine, authorizationHeader, response, isProxy, accessRights);
            NX_DEBUG(this, lm("%1 with digest (%2)").args(authResult, request.requestLine));
        }
        else if (authorizationHeader.authScheme == nx_http::header::AuthScheme::basic)
        {
            if (usedAuthMethod)
                *usedAuthMethod = nx_http::AuthMethod::httpBasic;
            authResult = doBasicAuth(request.requestLine.method, authorizationHeader, response, accessRights);

            if (authResult == Qn::Auth_OK && userResource &&
                (userResource->getDigest().isEmpty() || userResource->getRealm() != nx::network::AppInfo::realm()))
            {
                updateUserHashes(userResource, authorizationHeader.basic->password);
            }
        }
        else
        {
            if (usedAuthMethod)
                *usedAuthMethod = nx_http::AuthMethod::httpBasic;
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
        return authResult;
    }

    NX_VERBOSE(this, lm("Failed to authenticate %1 with any method").arg(request.requestLine.url));

    return Qn::Auth_Forbidden;   //failed to authorise request with any method
}

nx_http::AuthMethodRestrictionList* QnAuthHelper::restrictionList()
{
    return &m_authMethodRestrictionList;
}

QPair<QString, QString> QnAuthHelper::createAuthenticationQueryItemForPath(
    const Qn::UserAccessData& accessRights,
    const QString& path,
    unsigned int periodMillis)
{
    QString authKey = QnUuid::createUuid().toString();
    if (authKey.isEmpty())
        return QPair<QString, QString>();   //bad guid, failure
    authKey.replace(lit("{"), QString());
    authKey.replace(lit("}"), QString());

    //disabling authentication
    QnMutexLocker lk(&m_mutex);

    //adding active period
    nx::utils::TimerManager::TimerGuard timerGuard(
        nx::utils::TimerManager::instance(),
        nx::utils::TimerManager::instance()->addTimer(
            std::bind(&QnAuthHelper::authenticationExpired, this, authKey, std::placeholders::_1),
            std::chrono::milliseconds(std::min(periodMillis, MAX_AUTHENTICATION_KEY_LIFE_TIME_MS))));

    TempAuthenticationKeyCtx ctx;
    ctx.timeGuard = std::move(timerGuard);
    ctx.path = path;
    ctx.accessRights = accessRights;
    m_authenticatedPaths.emplace(authKey, std::move(ctx));

    return QPair<QString, QString>(TEMP_AUTH_KEY_NAME, authKey);
}

void QnAuthHelper::authenticationExpired(const QString& authKey, quint64 /*timerID*/)
{
    QnMutexLocker lk(&m_mutex);
    m_authenticatedPaths.erase(authKey);
}

static bool verifyDigestUri(const QUrl& requestUrl, const QByteArray& uri)
{
    const QUrl digestUrl(QString::fromUtf8(uri));
    const auto requestPath = requestUrl.path();
    const auto digsetPath = digestUrl.path();
    if (requestUrl.path() != digestUrl.path())
        return false;

    const auto requestQuery = requestUrl.query();
    const auto digestQuery = digestUrl.query();
    if (kVerifyDigestUriWithParams && requestUrl.query() != digestUrl.query())
        return false;

    return true;
}

Qn::AuthResult QnAuthHelper::doDigestAuth(
    const nx_http::RequestLine& requestLine,
    const nx_http::header::Authorization& authorization,
    nx_http::Response& responseHeaders,
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

    QnUserResourcePtr userResource;
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

    if (userResource &&
        userResource->getRealm() != nx::network::AppInfo::realm())
    {
        //requesting client to re-calculate user's HA1 digest
        nx_http::insertOrReplaceHeader(
            &responseHeaders.headers,
            nx_http::HttpHeader(Qn::REALM_HEADER_NAME, nx::network::AppInfo::realm().toLatin1()));
    }
    addAuthHeader(
        responseHeaders,
        userResource,
        isProxy);

    if (errCode == Qn::Auth_WrongLogin && !QUuid(userName).isNull())
        errCode = Qn::Auth_WrongInternalLogin;

    return errCode;
}

Qn::AuthResult QnAuthHelper::doBasicAuth(
    const QByteArray& method,
    const nx_http::header::Authorization& authorization,
    nx_http::Response& response,
    Qn::UserAccessData* accessRights)
{
    NX_ASSERT(authorization.authScheme == nx_http::header::AuthScheme::basic);

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
        if (auto user = res.dynamicCast<QnUserResource>())
        {
            if (user->getDigest().isEmpty())
                emit emptyDigestDetected(user, authorization.basic->userid, authorization.basic->password);
        }
        return Qn::Auth_OK;
    }

    return errCode;
}

Qn::AuthResult QnAuthHelper::doCookieAuthorization(
    const QByteArray& method,
    const QByteArray& authData,
    nx_http::Response& responseHeaders,
    Qn::UserAccessData* accessRights)
{
    nx_http::Response tmpHeaders;

    QMap<nx_http::BufferType, nx_http::BufferType> params;
    nx::utils::parseNameValuePairs(authData, ';', &params);

    Qn::AuthResult authResult = Qn::Auth_Forbidden;
    if (params.contains(Qn::URL_QUERY_AUTH_KEY_NAME))
    {
        //authenticating
        authResult = authenticateByUrl(
            QUrl::fromPercentEncoding(params.value(Qn::URL_QUERY_AUTH_KEY_NAME)).toUtf8(),
            method,
            responseHeaders,
            accessRights);
    }
    else
    {
        NX_ASSERT(false); 
    }

    return authResult;
}

void QnAuthHelper::addAuthHeader(
    nx_http::Response& response,
    const QnUserResourcePtr& userResource,
    bool isProxy,
    bool isDigest)
{
    QString realm;
    if (userResource)
        realm = userResource->getRealm();
    else
        realm = nx::network::AppInfo::realm();

    const QString auth =
        isDigest
        ? lit("Digest realm=\"%1\", nonce=\"%2\", algorithm=MD5")
            .arg(realm)
            .arg(QLatin1String(m_nonceProvider->generateNonce()))
        : lit("Basic realm=\"%1\"").arg(realm);

    //QString auth(lit("Digest realm=\"%1\",nonce=\"%2\",algorithm=MD5,qop=\"auth\""));
    const QByteArray headerName = isProxy ? "Proxy-Authenticate" : "WWW-Authenticate";
    nx_http::insertOrReplaceHeader(&response.headers, nx_http::HttpHeader(
        headerName,
        auth.toLatin1()));
}

QByteArray QnAuthHelper::generateNonce(NonceProvider provider) const
{
    if (provider == NonceProvider::automatic)
        return m_nonceProvider->generateNonce();
    else
        return m_timeBasedNonceProvider->generateNonce();
}

Qn::AuthResult QnAuthHelper::authenticateByUrl(
    const QByteArray& authRecordBase64,
    const QByteArray& method,
    nx_http::Response& response,
    Qn::UserAccessData* accessRights) const
{
    auto authRecord = QByteArray::fromBase64(authRecordBase64);
    auto authFields = authRecord.split(':');
    if (authFields.size() != 3)
        return Qn::Auth_WrongDigest;

    nx_http::header::Authorization authorization(nx_http::header::AuthScheme::digest);
    authorization.digest->userid = authFields[0];
    authorization.digest->params["response"] = authFields[2];
    authorization.digest->params["nonce"] = authFields[1];
    authorization.digest->params["realm"] = nx::network::AppInfo::realm().toUtf8();
    //digestAuthParams.params["uri"];   uri is empty

    if (!m_nonceProvider->isNonceValid(authorization.digest->params["nonce"]))
        return Qn::Auth_WrongDigest;

    QnResourcePtr res;
    Qn::AuthResult errCode = Qn::Auth_WrongLogin;
    std::tie(errCode, res) = m_userDataProvider->authorize(
        method,
        authorization,
        &response.headers);
    if (!res)
        return Qn::Auth_WrongLogin;

    if (auto user = res.dynamicCast<QnUserResource>())
    {
        if (accessRights)
            *accessRights = Qn::UserAccessData(user->getId());
    }

    return errCode;
}

QnUserResourcePtr QnAuthHelper::findUserByName(const QByteArray& nxUserName) const
{
    auto res = m_userDataProvider->findResByName(nxUserName);
    if (auto user = res.dynamicCast<QnUserResource>())
        return user;
    return QnUserResourcePtr();
}

void QnAuthHelper::updateUserHashes(const QnUserResourcePtr& userResource, const QString& password)
{
    if (userResource->isLdap() && userResource->decodeLDAPPassword() == password)
        return; //< password is not changed

    userResource->setRealm(nx::network::AppInfo::realm());
    userResource->setPasswordAndGenerateHash(password);

    ec2::ApiUserData userData;
    fromResourceToApi(userResource, userData);
    commonModule()->ec2Connection()->getUserManager(Qn::kSystemAccess)->save(
        userData,
        QString(),
        ec2::DummyHandler::instance(),
        &ec2::DummyHandler::onRequestDone);
}

bool QnAuthHelper::checkUserPassword(const QnUserResourcePtr& user, const QString& password)
{
    if (!user->isCloud())
        return user->checkLocalUserPassword(password);

    // 3. Cloud users
    QByteArray auth = createHttpQueryAuthParam(
        user->getName(),
        password,
        user->getRealm(),
        "GET",
        qnAuthHelper->generateNonce());

    nx_http::Response response;
    return authenticateByUrl(auth, QByteArray("GET"), response) == Qn::Auth_OK;
}

QnLdapManager* QnAuthHelper::ldapManager() const
{
    return m_ldap.get();
}
