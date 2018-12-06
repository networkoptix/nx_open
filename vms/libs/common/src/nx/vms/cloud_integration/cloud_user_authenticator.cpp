#include "cloud_user_authenticator.h"

#include <chrono>

#include <nx/network/app_info.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/custom_headers.h>
#include <nx/utils/log/log.h>
#include <nx/utils/sync_call.h>

#include <api/app_server_connection.h>
#include <api/global_settings.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <nx_ec/dummy_handler.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <utils/common/app_info.h>

#include "cdb_nonce_fetcher.h"
#include "cloud_connection_manager.h"

namespace nx {
namespace vms {
namespace cloud_integration {

static const std::chrono::minutes kUnsuccessfulAuthorizationResultCachePeriod(1);

CloudUserAuthenticator::CloudUserAuthenticator(
    CloudConnectionManager* const cloudConnectionManager,
    std::unique_ptr<auth::AbstractUserDataProvider> defaultAuthenticator,
    const CdbNonceFetcher& cdbNonceFetcher,
    const CloudUserInfoPool& cloudUserInfoPool)
:
    m_cloudConnectionManager(cloudConnectionManager),
    m_defaultAuthenticator(std::move(defaultAuthenticator)),
    m_cdbNonceFetcher(cdbNonceFetcher),
    m_cloudUserInfoPool(cloudUserInfoPool)
{
    using namespace std::placeholders;

    m_monotonicClock.restart();
    Qn::directConnect(
        m_cloudConnectionManager, &CloudConnectionManager::cloudBindingStatusChanged,
        this, &CloudUserAuthenticator::cloudBindingStatusChanged);
}

CloudUserAuthenticator::~CloudUserAuthenticator()
{
    directDisconnectAll();
}

QnResourcePtr CloudUserAuthenticator::findResByName(const QByteArray& nxUserName) const
{
    // NOTE: This method returns NULL for cloud login.
    //  This is OK, since NULL from this method does not signal authentication failure.
    return m_defaultAuthenticator->findResByName(nxUserName);
}

Qn::AuthResult CloudUserAuthenticator::authorize(
    const QnResourcePtr& res,
    const nx::network::http::Method::ValueType& method,
    const nx::network::http::header::Authorization& authorizationHeader,
    nx::network::http::HttpHeaders* const responseHeaders)
{
    QnResourcePtr authResource;
    Qn::AuthResult authResult = Qn::Auth_OK;
    std::tie(authResult, authResource) = authorize(
        method,
        authorizationHeader,
        responseHeaders);
    if (authResult == Qn::Auth_OK)
    {
        return authResource == res
            ? Qn::Auth_OK
            : Qn::Auth_Forbidden;
    }

    return m_defaultAuthenticator->authorize(
        res,
        method,
        authorizationHeader,
        responseHeaders);
}

std::tuple<Qn::AuthResult, QnResourcePtr> CloudUserAuthenticator::authorize(
    const nx::network::http::Method::ValueType& method,
    const nx::network::http::header::Authorization& authorizationHeader,
    nx::network::http::HttpHeaders* const responseHeaders)
{
    const auto& commonModule = m_cloudConnectionManager->commonModule();

    const QByteArray userName = authorizationHeader.userid().toLower();

    auto cloudUsers = commonModule->resourcePool()->getResources<QnUserResource>().filtered(
        [userName](const QnUserResourcePtr& user)
        {
            return user->isCloud() &&
                user->isEnabled() &&
                user->getName().toUtf8().toLower() == userName;
        });
    const bool isCloudUser = !cloudUsers.isEmpty();

    if (authorizationHeader.authScheme != nx::network::http::header::AuthScheme::digest)
    {
        // Supporting only digest authentication for cloud-based authentication.

        if (isCloudUser)
        {
            NX_VERBOSE(this, lm("Refusing non-digest authentication of user %1")
                .arg(authorizationHeader.userid()));
            return std::tuple<Qn::AuthResult, QnResourcePtr>(Qn::Auth_Forbidden, cloudUsers[0]);
        }

        NX_VERBOSE(this, lm("Passing non-digest authentication of user %1 to the default authenticator")
            .arg(authorizationHeader.userid()));

        return m_defaultAuthenticator->authorize(
            method,
            authorizationHeader,
            responseHeaders);
    }

    const auto nonce = authorizationHeader.digest->params["nonce"];
    bool isCloudNonce = m_cdbNonceFetcher.isValidCloudNonce(nonce);

    if (isCloudUser && isCloudNonce)
    {
        auto userInfoPoolAuthResult =
            m_cloudUserInfoPool.authenticate(method, authorizationHeader);

        if (userInfoPoolAuthResult == Qn::Auth_OK)
            return std::tuple<Qn::AuthResult, QnResourcePtr>(Qn::Auth_OK, cloudUsers.first());

        if (userInfoPoolAuthResult == Qn::Auth_WrongPassword)
        {
            return std::tuple<Qn::AuthResult, QnResourcePtr>(
                Qn::Auth_WrongPassword,
                QnResourcePtr());
        }
    }

    // Server has provided to the client non-cloud nonce
    // for cloud user due to no cloud connection so far.
    if (isCloudUser && !isCloudNonce)
    {
        if (!cloudUsers.isEmpty())
        {
            NX_VERBOSE(this, lm("Refusing digest authentication of user %1 due to non-cloud nonce %2")
                .arg(authorizationHeader.userid()).arg(nonce));
            return std::make_tuple<Qn::AuthResult, QnResourcePtr>(
                Qn::Auth_CloudConnectError,
                cloudUsers.first());
        }
    }

    if (!isCloudUser || !isCloudNonce)    //< Nonce must be valid cloud nonce.
    {
        NX_VERBOSE(this, lm("Passing digest authentication of user %1 to the default authenticator. Nonce %2")
            .arg(authorizationHeader.userid()).arg(nonce));

        const std::tuple<Qn::AuthResult, QnResourcePtr> authResult =
            m_defaultAuthenticator->authorize(
                method,
                authorizationHeader,
                responseHeaders);

        if (std::get<0>(authResult) != Qn::Auth_WrongLogin)
        {
            NX_VERBOSE(this, lm("User %1 authentication result %2 by default authenticator. Nonce %3")
                .args(authorizationHeader.userid(), std::get<0>(authResult), nonce));
            return authResult;
        }
    }

    NX_VERBOSE(this, lm("Cloud authentication requested. username %1, nonce %2")
        .arg(authorizationHeader.userid()).arg(nonce));

    nx::String cloudNonce;
    nx::String nonceTrailer;
    if (!CdbNonceFetcher::parseCloudNonce(nonce, &cloudNonce, &nonceTrailer))
    {
        NX_VERBOSE(this, lm("Refusing authentication of user %1 due to invalid cloud nonce %2")
            .arg(authorizationHeader.userid()).arg(nonce));
        NX_VERBOSE(this, lm("Passing digest authentication of user %1 to the default authenticator. Nonce %2")
            .arg(authorizationHeader.userid()).arg(nonce));
        return m_defaultAuthenticator->authorize(
            method,
            authorizationHeader,
            responseHeaders);
    }

    // Checking authorization cache.
    const auto cacheKey = std::make_pair(authorizationHeader.userid(), cloudNonce);

    QnMutexLocker lk(&m_mutex);
    removeExpiredRecordsFromCache(&lk);
    auto cachedIter = m_authorizationCache.find(cacheKey);
    if (cachedIter == m_authorizationCache.end())
    {
        if (m_requestInProgress.find(cacheKey) != m_requestInProgress.end())
        {
            NX_VERBOSE(this, lm("Waiting for running cloud get_auth request. username %1, cloudNonce %2")
                .arg(authorizationHeader.userid()).arg(cloudNonce));

            // If request for required cacheKey is in progress, waiting for its completion.
            while (m_requestInProgress.find(cacheKey) != m_requestInProgress.end())
                m_cond.wait(lk.mutex());

            NX_VERBOSE(this, lm("Running get_auth request has provided some result. username %1, cloudNonce %2")
                .arg(authorizationHeader.userid()).arg(cloudNonce));
        }
        else
        {
            NX_VERBOSE(this, lm("Issuing cloud get_auth request. username %1, cloudNonce %2")
                .arg(authorizationHeader.userid()).arg(cloudNonce));
            fetchAuthorizationFromCloud(&lk, authorizationHeader.userid(), cloudNonce);
        }
    }

    cachedIter = m_authorizationCache.find(cacheKey);
    if (cachedIter == m_authorizationCache.end())
    {
        lk.unlock();
        NX_VERBOSE(this, lm("No valid cloud auth data. username %1, cloudNonce %2")
            .arg(authorizationHeader.userid()).arg(cloudNonce));

        if (!cloudUsers.isEmpty())
        {
            NX_DEBUG(this, lm("Refusing authentication of user %1 due to cloud connect error")
                .arg(authorizationHeader.userid()));
            return std::make_tuple<Qn::AuthResult, QnResourcePtr>(
                Qn::Auth_CloudConnectError,
                cloudUsers.first());
        }

        NX_VERBOSE(this, lm("Passing digest authentication of user %1 to the default authenticator. Nonce %2")
            .arg(authorizationHeader.userid()).arg(nonce));

        return m_defaultAuthenticator->authorize(
            method,
            authorizationHeader,
            responseHeaders);
    }

    return authorizeWithCacheItem(
        &lk,
        cachedIter->second,
        cloudNonce,
        nonceTrailer,
        responseHeaders,
        method,
        authorizationHeader);
}

void CloudUserAuthenticator::clear()
{
    QnMutexLocker lk(&m_mutex);
    m_authorizationCache.clear();
}

void CloudUserAuthenticator::removeExpiredRecordsFromCache(QnMutexLockerBase* const /*lk*/)
{
    const auto curClock = m_monotonicClock.elapsed();
    for (auto it = m_authorizationCache.begin();
         it != m_authorizationCache.end();
         )
    {
        if (it->second.expirationTime <= curClock)
            it = m_authorizationCache.erase(it);
        else
            ++it;
    }
}

QnUserResourcePtr CloudUserAuthenticator::getMappedLocalUserForCloudCredentials(
    const nx::network::http::StringType& userName) const
{
    const auto userNameQString = QString::fromUtf8(userName);
    const auto& commonModule = m_cloudConnectionManager->commonModule();
    // If there is user with same name in system, resolving to that user.
    const auto res = commonModule->resourcePool()->getResource(
        [&userNameQString](const QnResourcePtr& res) {
            return (res.dynamicCast<QnUserResource>() != nullptr) &&
                   (res->getName() == userNameQString);
        });

    if (res)
        return res.staticCast<QnUserResource>();

    // Cloud user is created by cloud portal during sharing process.
    return QnUserResourcePtr();
}

void CloudUserAuthenticator::fetchAuthorizationFromCloud(
    QnMutexLockerBase* const lk,
    const nx::network::http::StringType& userid,
    const nx::network::http::StringType& cloudNonce)
{
    NX_VERBOSE(this, lm("Auth data for username %1, cloudNonce %2 not found in cache. Quering cloud...")
        .arg(userid).arg(cloudNonce));

    nx::cloud::db::api::AuthRequest authRequest;
    authRequest.nonce = cloudNonce.toStdString();
    authRequest.realm = nx::network::AppInfo::realm().toStdString();
    authRequest.username = userid.toStdString();

    // Marking that request is in progress.
    const auto requestIter = m_requestInProgress.emplace(userid, cloudNonce).first;

    lk->unlock();

    nx::cloud::db::api::ResultCode resultCode;
    nx::cloud::db::api::AuthResponse authResponse;
    {
        const auto connection = m_cloudConnectionManager->getCloudConnection();
        if (connection)
        {
            std::tie(resultCode, authResponse) =
                makeSyncCall<nx::cloud::db::api::ResultCode, nx::cloud::db::api::AuthResponse>(
                    std::bind(
                        &nx::cloud::db::api::AuthProvider::getAuthenticationResponse,
                        connection->authProvider(),
                        std::move(authRequest),
                        std::placeholders::_1));
            if (resultCode != nx::cloud::db::api::ResultCode::ok)
                m_cloudConnectionManager->processCloudErrorCode(resultCode);
        }
        else
        {
            resultCode = nx::cloud::db::api::ResultCode::forbidden;
        }
    }

    lk->relock();

    m_requestInProgress.erase(requestIter);

    if (resultCode == nx::cloud::db::api::ResultCode::ok)
    {
        NX_VERBOSE(this, lm("Received successful authenticate response from cloud. username %1, cloudNonce %2")
            .arg(userid).arg(cloudNonce));

        CloudAuthenticationData authData;
        authData.authorized = true;
        authData.data = std::move(authResponse);
        authData.expirationTime =
            m_monotonicClock.elapsed() +
            std::chrono::duration_cast<std::chrono::milliseconds>(authResponse.validPeriod).count();
        m_authorizationCache.emplace(
            std::make_pair(userid, cloudNonce),
            std::move(authData));
    }
    else
    {
        NX_VERBOSE(this, lm("Failed to authenticate username %1, cloudNonce %2 in cloud: %3")
            .arg(userid).arg(cloudNonce)
            .arg(m_cloudConnectionManager->connectionFactory().toString(resultCode)));

        if (resultCode != nx::cloud::db::api::ResultCode::networkError &&
            resultCode != nx::cloud::db::api::ResultCode::unknownError)
        {
            // Caching invalid authorization result.
            CloudAuthenticationData authData;
            authData.authorized = false;
            authData.expirationTime =
                m_monotonicClock.elapsed() +
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    kUnsuccessfulAuthorizationResultCachePeriod).count();
            m_authorizationCache.emplace(
                std::make_pair(userid, cloudNonce),
                std::move(authData));
        }
    }

    m_cond.wakeAll();
}

std::tuple<Qn::AuthResult, QnResourcePtr> CloudUserAuthenticator::authorizeWithCacheItem(
    QnMutexLockerBase* const /*lock*/,
    const CloudAuthenticationData& cacheItem,
    const nx::network::http::StringType& cloudNonce,
    const nx::network::http::StringType& nonceTrailer,
    nx::network::http::HttpHeaders* const responseHeaders,
    const nx::network::http::Method::ValueType& method,
    const nx::network::http::header::Authorization& authorizationHeader) const
{
    NX_VERBOSE(this, lm("Authenticating cloud user %1, cloudNonce %2").
        arg(authorizationHeader.userid()).arg(cloudNonce));

    const auto ha2 = nx::network::http::calcHa2(method, authorizationHeader.digest->params["uri"]);

    if (!cacheItem.authorized)
    {
        NX_VERBOSE(this, lm("Refusing authentication of user %1 due to cached \"not authorized\" status")
            .arg(authorizationHeader.userid()));
        return std::make_tuple(Qn::Auth_WrongLogin, QnResourcePtr());
    }

    // Translating cloud account to local user.
    auto localUser = getMappedLocalUserForCloudCredentials(
        cacheItem.data.authenticatedAccountData.accountEmail.c_str());
    if (!localUser)
    {
        NX_INFO(this, lm("Refusing authentication of user %1: could not find user record in local database")
            .arg(authorizationHeader.userid()));
        return std::make_tuple(Qn::Auth_WrongLogin, QnResourcePtr());
    }

    const auto calculatedResponse = nx::network::http::calcResponseFromIntermediate(
        cacheItem.data.intermediateResponse.c_str(),
        cloudNonce.size(),
        nonceTrailer,
        ha2);
    if (authorizationHeader.digest->params["response"] == calculatedResponse)
    {
        // Returning resolved user to HTTP client.
        responseHeaders->emplace(
            Qn::EFFECTIVE_USER_NAME_HEADER_NAME,
            localUser->getName().toUtf8());

        NX_VERBOSE(this, lm("Successful cloud authentication. username %1, cloudNonce %2")
            .arg(authorizationHeader.userid()).arg(cloudNonce));
        return std::make_tuple(Qn::Auth_OK, std::move(localUser));
    }
    else
    {
        NX_VERBOSE(this, lm("Refusing authentication of user %1: response digest validation failed")
            .arg(authorizationHeader.userid()));
        return std::make_tuple(Qn::Auth_WrongPassword, std::move(localUser));
    }
}

void CloudUserAuthenticator::cloudBindingStatusChanged(bool boundToCloud)
{
    if (!boundToCloud)
        clear();
}

} // namespace cloud_integration
} // namespace vms
} // namespace nx
