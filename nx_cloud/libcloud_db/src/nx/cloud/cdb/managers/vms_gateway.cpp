#include "vms_gateway.h"

#include <nx/fusion/model_functions.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/log/log.h>

#include "../settings.h"

namespace nx {
namespace cdb {

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::cdb, VmsResultCode,
    (nx::cdb::VmsResultCode::ok, "ok")
    (nx::cdb::VmsResultCode::invalidData, "invalidData")
    (nx::cdb::VmsResultCode::networkError, "networkError")
    (nx::cdb::VmsResultCode::forbidden, "forbidden")
    (nx::cdb::VmsResultCode::logicalError, "logicalError")
    (nx::cdb::VmsResultCode::unreachable, "unreachable")
)

//-------------------------------------------------------------------------------------------------

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
        nx_http::rest::substituteParameters(urlBase, { targetSystemId });

    NX_VERBOSE(this, lm("Issuing merge request to %1 on behalf of %2. Url %3")
        .args(targetSystemId, username, baseRequestUrl));

    RequestContext requestContext;
    requestContext.client = std::make_unique<MediaServerClient>(QUrl(baseRequestUrl.c_str()));
    requestContext.targetSystemId = targetSystemId;
    auto clientPtr = requestContext.client.get();

    if (!addAuthentication(username, clientPtr))
        return reportInvalidConfiguration(targetSystemId, std::move(completionHandler));

    requestContext.completionHandler = std::move(completionHandler);

    QnMutexLocker lock(&m_mutex);
    m_activeRequests.emplace(clientPtr, std::move(requestContext));
    MergeSystemData mergeRequest = prepareMergeRequestParameters(systemIdToMergeTo);
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

bool VmsGateway::addAuthentication(
    const std::string& username,
    MediaServerClient* clientPtr)
{
    auto account = m_accountManager.findAccountByUserName(username);
    if (!account)
    {
        NX_VERBOSE(this, lm("Could not find account %1").args(username));
        return false;
    }
    clientPtr->setUserCredentials(nx_http::Credentials(
        account->email.c_str(),
        nx_http::Ha1AuthToken(account->passwordHa1.c_str())));

    return true;
}

MergeSystemData VmsGateway::prepareMergeRequestParameters(
    const std::string& systemIdToMergeTo)
{
    MergeSystemData mergeSystemData;
    mergeSystemData.mergeOneServer = false;
    mergeSystemData.takeRemoteSettings = true;
    mergeSystemData.ignoreIncompatible = false;
    // TODO getKey
    // TODO postKey
    mergeSystemData.url =
        nx::network::url::Builder().setScheme(nx_http::kSecureUrlSchemeName)
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
        NX_DEBUG(this, lm("Merge request to %1 failed with result code %2")
            .args(targetSystemId, QnLexical::serialized(result.error)));
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
    VmsRequestResult result;
    result.resultCode = VmsResultCode::networkError;

    if (vmsResult.error == QnRestResult::Error::CantProcessRequest)
    {
        switch (clientPtr->prevResponseHttpStatusCode())
        {
            case nx_http::StatusCode::serviceUnavailable:
            case nx_http::StatusCode::notFound:
                result.resultCode = VmsResultCode::unreachable;
                break;

            case nx_http::StatusCode::badRequest:
                result.resultCode = VmsResultCode::invalidData;
                break;

            case nx_http::StatusCode::unauthorized:
            case nx_http::StatusCode::forbidden:
                result.resultCode = VmsResultCode::forbidden;
                break;

            default:
                result.resultCode = VmsResultCode::networkError;
                break;
        }
    }

    return result;
}

} // namespace cdb
} // namespace nx
