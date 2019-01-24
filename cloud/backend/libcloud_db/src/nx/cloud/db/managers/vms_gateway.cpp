#include "vms_gateway.h"

#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/log/log.h>

#include <nx/cloud/db/api/cloud_nonce.h>

#include <api/auth_util.h>

#include "../settings.h"

namespace nx::cloud::db {

VmsGateway::VmsGateway(
    const conf::Settings& settings,
    const AbstractAccountManager& accountManager)
    :
    m_settings(settings),
    m_accountManager(accountManager)
{
}

VmsGateway::~VmsGateway()
{
    m_asyncCall.pleaseStopSync();

    decltype(m_activeRequests) activeRequests;
    {
        QnMutexLocker lock(&m_mutex);
        m_activeRequests.swap(activeRequests);
    }

    for (auto& val: activeRequests)
        val.second.client->pleaseStopSync();
}

void VmsGateway::merge(
    const std::string& username,
    const std::string& targetSystemId,
    const std::string& systemIdToMergeTo,
    VmsRequestCompletionHandler completionHandler)
{
    using namespace std::placeholders;

    const auto urlBase = m_settings.vmsGateway().url;
    if (urlBase.empty())
        return reportInvalidConfiguration(targetSystemId, std::move(completionHandler));

    const auto baseRequestUrl =
        nx::network::http::rest::substituteParameters(urlBase, { targetSystemId });

    NX_VERBOSE(this, lm("Issuing merge request to %1 on behalf of %2. Url %3")
        .args(targetSystemId, username, baseRequestUrl));

    RequestContext requestContext;
    requestContext.client =
        std::make_unique<MediaServerClient>(nx::utils::Url(baseRequestUrl.c_str()));
    requestContext.client->setRequestTimeout(m_settings.vmsGateway().requestTimeout);
    requestContext.targetSystemId = targetSystemId;
    auto clientPtr = requestContext.client.get();

    auto account = m_accountManager.findAccountByUserName(username);
    if (!account)
    {
        NX_VERBOSE(this, lm("Could not find account %1").args(username));
        return reportInvalidConfiguration(targetSystemId, std::move(completionHandler));
    }
    const nx::network::http::Credentials userCredentials(
        account->email.c_str(),
        nx::network::http::Ha1AuthToken(account->passwordHa1.c_str()));

    clientPtr->setUserCredentials(userCredentials);

    requestContext.completionHandler = std::move(completionHandler);

    QnMutexLocker lock(&m_mutex);
    m_activeRequests.emplace(clientPtr, std::move(requestContext));
    MergeSystemData mergeRequest =
        prepareMergeRequestParameters(userCredentials, systemIdToMergeTo);
    clientPtr->mergeSystems(
        mergeRequest,
        std::bind(&VmsGateway::reportRequestResult, this, clientPtr, _1));
}

void VmsGateway::reportInvalidConfiguration(
    const std::string& targetSystemId,
    VmsRequestCompletionHandler completionHandler)
{
    NX_VERBOSE(this, lm("Cannot issue merge request to %1. No url").args(targetSystemId));

    m_asyncCall.post(
        [completionHandler = std::move(completionHandler)]()
        {
            VmsRequestResult vmsRequestResult;
            vmsRequestResult.resultCode = VmsResultCode::invalidData;
            completionHandler(std::move(vmsRequestResult));
        });
}

MergeSystemData VmsGateway::prepareMergeRequestParameters(
    const nx::network::http::Credentials& userCredentials,
    const std::string& systemIdToMergeTo)
{
    MergeSystemData mergeSystemData;
    mergeSystemData.mergeOneServer = false;
    mergeSystemData.takeRemoteSettings = true;
    mergeSystemData.ignoreIncompatible = false;

    AuthKey authKey;
    authKey.username = userCredentials.username.toUtf8();
    authKey.nonce = api::generateNonce(api::generateCloudNonceBase(systemIdToMergeTo)).c_str();

    authKey.calcResponse(userCredentials.authToken, nx::network::http::Method::get, "");
    mergeSystemData.getKey = authKey.toString();

    authKey.calcResponse(userCredentials.authToken, nx::network::http::Method::post, "");
    mergeSystemData.postKey = authKey.toString();

    mergeSystemData.url =
        nx::network::url::Builder().setScheme(nx::network::http::kSecureUrlSchemeName)
            .setHost(systemIdToMergeTo.c_str()).toString();

    return mergeSystemData;
}

void VmsGateway::reportRequestResult(
    MediaServerClient* clientPtr,
    QnJsonRestResult result)
{
    VmsRequestCompletionHandler completionHandler;
    std::string targetSystemId;

    {
        QnMutexLocker lock(&m_mutex);
        const auto it = m_activeRequests.find(clientPtr);
        if (it == m_activeRequests.end())
            return;
        completionHandler = std::move(it->second.completionHandler);
        targetSystemId = it->second.targetSystemId;
    }

    if (result.error == QnRestResult::Error::NoError)
    {
        NX_VERBOSE(this, lm("Merge request to %1 succeeded").args(targetSystemId));
    }
    else
    {
        NX_DEBUG(this, lm("Merge request to %1 failed with result %2 (%3, %4)")
            .args(targetSystemId, QnLexical::serialized(result.error),
                result.errorString, result.reply.toString()));
    }

    completionHandler(prepareVmsResult(clientPtr, result));

    {
        QnMutexLocker lock(&m_mutex);
        const auto it = m_activeRequests.find(clientPtr);
        if (it == m_activeRequests.end())
            return;
        m_activeRequests.erase(it);
    }
}

VmsRequestResult VmsGateway::prepareVmsResult(
    MediaServerClient* clientPtr,
    const QnJsonRestResult& vmsResult)
{
    VmsRequestResult result;
    result.resultCode = VmsResultCode::ok;
    result.vmsErrorDescription = vmsResult.errorString.toStdString();

    if (vmsResult.error != QnRestResult::Error::NoError)
        result = convertVmsResultToResultCode(clientPtr, vmsResult);

    return result;
}

VmsRequestResult VmsGateway::convertVmsResultToResultCode(
    MediaServerClient* clientPtr,
    const QnJsonRestResult& vmsResult)
{
    using namespace nx::network::http;

    VmsRequestResult result;
    result.resultCode = VmsResultCode::networkError;
    result.vmsErrorDescription = vmsResult.errorString.toStdString();

    if (clientPtr->prevRequestSysErrorCode() != SystemError::noError)
    {
        result.resultCode = VmsResultCode::networkError;
        result.vmsErrorDescription = 
            SystemError::toString(clientPtr->prevRequestSysErrorCode()).toStdString();
        return result;
    }

    if (vmsResult.error == QnRestResult::Error::CantProcessRequest)
    {
        switch (clientPtr->lastResponseHttpStatusLine().statusCode)
        {
            case StatusCode::serviceUnavailable:
            case StatusCode::notFound:
                result.resultCode = VmsResultCode::unreachable;
                break;

            case StatusCode::badRequest:
                result.resultCode = VmsResultCode::invalidData;
                break;

            case StatusCode::unauthorized:
            case StatusCode::forbidden:
                result.resultCode = VmsResultCode::forbidden;
                break;

            case StatusCode::badGateway:
            case StatusCode::gatewayTimeOut:
                result.resultCode = VmsResultCode::unreachable;
                break;

            default:
                result.resultCode = VmsResultCode::logicalError;
                break;
        }

        if (result.vmsErrorDescription.empty() && 
            !StatusCode::isSuccessCode(clientPtr->lastResponseHttpStatusLine().statusCode))
        {
            result.vmsErrorDescription =
                clientPtr->lastResponseHttpStatusLine().reasonPhrase.toStdString();
        }
    }

    return result;
}

} // namespace nx::cloud::db
