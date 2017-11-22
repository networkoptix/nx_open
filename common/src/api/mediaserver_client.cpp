#include "mediaserver_client.h"

#include <nx/network/url/url_builder.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/type_utils.h>

MediaServerClient::MediaServerClient(const nx::utils::Url& baseRequestUrl):
    m_baseRequestUrl(baseRequestUrl)
{
}

void MediaServerClient::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    for (auto& client: m_activeClients)
        client->bindToAioThread(aioThread);
}

void MediaServerClient::setUserCredentials(const nx_http::Credentials& userCredentials)
{
    m_userCredentials = userCredentials;
}

void MediaServerClient::setAuthenticationKey(const QString& key)
{
    m_authenticationKey = key;
}

void MediaServerClient::setRequestTimeout(std::chrono::milliseconds timeout)
{
    m_requestTimeout = timeout;
}

//-------------------------------------------------------------------------------------------------
// /api/ requests

void MediaServerClient::saveCloudSystemCredentials(
    const CloudCredentialsData& inputData,
    std::function<void(QnJsonRestResult)> completionHandler)
{
    performApiRequest("api/saveCloudSystemCredentials", inputData, std::move(completionHandler));
}

QnJsonRestResult MediaServerClient::saveCloudSystemCredentials(
    const CloudCredentialsData& request)
{
    using SaveCloudSystemCredentialsAsyncFuncPointer =
        void(MediaServerClient::*)(
            const CloudCredentialsData&, std::function<void(QnJsonRestResult)>);

    return syncCallWrapper(
        this,
        static_cast<SaveCloudSystemCredentialsAsyncFuncPointer>(
            &MediaServerClient::saveCloudSystemCredentials),
        request);
}

void MediaServerClient::getModuleInformation(
    std::function<void(QnJsonRestResult, QnModuleInformation)> completionHandler)
{
    performApiRequest("api/moduleInformation", std::move(completionHandler));
}

QnJsonRestResult MediaServerClient::getModuleInformation(QnModuleInformation* moduleInformation)
{
    using GetModuleInformationAsyncFuncPointer =
        void(MediaServerClient::*)(std::function<void(QnJsonRestResult, QnModuleInformation)>);

    return syncCallWrapper(
        this,
        static_cast<GetModuleInformationAsyncFuncPointer>(
            &MediaServerClient::getModuleInformation),
        moduleInformation);
}

void MediaServerClient::setupLocalSystem(
    const SetupLocalSystemData& request,
    std::function<void(QnJsonRestResult)> completionHandler)
{
    performApiRequest("api/setupLocalSystem", request, std::move(completionHandler));
}

QnJsonRestResult MediaServerClient::setupLocalSystem(const SetupLocalSystemData& request)
{
    using SetupLocalSystemAsyncFuncPointer =
        void(MediaServerClient::*)(
            const SetupLocalSystemData&, std::function<void(QnJsonRestResult)>);

    return syncCallWrapper(
        this,
        static_cast<SetupLocalSystemAsyncFuncPointer>(&MediaServerClient::setupLocalSystem),
        request);
}

void MediaServerClient::setupCloudSystem(
    const SetupCloudSystemData& request,
    std::function<void(QnJsonRestResult)> completionHandler)
{
    performApiRequest("api/setupCloudSystem", request, std::move(completionHandler));
}

QnJsonRestResult MediaServerClient::setupCloudSystem(
    const SetupCloudSystemData& request)
{
    using SetupCloudSystemAsyncFuncPointer =
        void(MediaServerClient::*)(
            const SetupCloudSystemData&, std::function<void(QnJsonRestResult)>);

    return syncCallWrapper(
        this,
        static_cast<SetupCloudSystemAsyncFuncPointer>(&MediaServerClient::setupCloudSystem),
        request);
}

void MediaServerClient::detachFromCloud(
    const DetachFromCloudData& request,
    std::function<void(QnJsonRestResult)> completionHandler)
{
    performApiRequest("api/detachFromCloud", request, std::move(completionHandler));
}

QnJsonRestResult MediaServerClient::detachFromCloud(const DetachFromCloudData& request)
{
    using DetachFromCloudAsyncFuncPointer =
        void(MediaServerClient::*)(
            const DetachFromCloudData&, std::function<void(QnJsonRestResult)>);

    return syncCallWrapper(
        this,
        static_cast<DetachFromCloudAsyncFuncPointer>(
            &MediaServerClient::detachFromCloud),
        request);
}

void MediaServerClient::mergeSystems(
    const MergeSystemData& request,
    std::function<void(QnJsonRestResult)> completionHandler)
{
    performApiRequest("api/mergeSystems", request, std::move(completionHandler));
}

QnJsonRestResult MediaServerClient::mergeSystems(const MergeSystemData& request)
{
    using AsyncFuncPointer =
        void(MediaServerClient::*)(
            const MergeSystemData&, std::function<void(QnJsonRestResult)>);

    return syncCallWrapper(
        this,
        static_cast<AsyncFuncPointer>(&MediaServerClient::mergeSystems),
        request);
}

//-------------------------------------------------------------------------------------------------
// /ec2/ requests

void MediaServerClient::ec2GetUsers(
    std::function<void(ec2::ErrorCode, ec2::ApiUserDataList)> completionHandler)
{
    performAsyncEc2Call("ec2/getUsers", std::move(completionHandler));
}

ec2::ErrorCode MediaServerClient::ec2GetUsers(ec2::ApiUserDataList* result)
{
    using Ec2GetUsersAsyncFuncPointer =
        void(MediaServerClient::*)(
            std::function<void(ec2::ErrorCode, ec2::ApiUserDataList)>);

    return syncCallWrapper(
        this,
        static_cast<Ec2GetUsersAsyncFuncPointer>(&MediaServerClient::ec2GetUsers),
        result);
}

void MediaServerClient::ec2SaveUser(
    const ec2::ApiUserData& request,
    std::function<void(ec2::ErrorCode)> completionHandler)
{
    performAsyncEc2Call("ec2/saveUser", request, std::move(completionHandler));
}

ec2::ErrorCode MediaServerClient::ec2SaveUser(const ec2::ApiUserData& request)
{
    using Ec2SaveUserAsyncFuncPointer =
        void(MediaServerClient::*)(
            const ec2::ApiUserData& request,
            std::function<void(ec2::ErrorCode)>);

    return syncCallWrapper(
        this,
        static_cast<Ec2SaveUserAsyncFuncPointer>(&MediaServerClient::ec2SaveUser),
        request);
}

void MediaServerClient::ec2GetSettings(
    std::function<void(ec2::ErrorCode, ec2::ApiResourceParamDataList)> completionHandler)
{
    performAsyncEc2Call("ec2/getSettings", std::move(completionHandler));
}

ec2::ErrorCode MediaServerClient::ec2GetSettings(ec2::ApiResourceParamDataList* result)
{
    using Ec2GetSettingsAsyncFuncPointer =
        void(MediaServerClient::*)(
            std::function<void(ec2::ErrorCode, ec2::ApiResourceParamDataList)>);

    return syncCallWrapper(
        this,
        static_cast<Ec2GetSettingsAsyncFuncPointer>(&MediaServerClient::ec2GetSettings),
        result);
}

void MediaServerClient::ec2SetResourceParams(
    const ec2::ApiResourceParamWithRefDataList& inputData,
    std::function<void(ec2::ErrorCode)> completionHandler)
{
    performAsyncEc2Call("ec2/setResourceParams", inputData, std::move(completionHandler));
}

ec2::ErrorCode MediaServerClient::ec2SetResourceParams(
    const ec2::ApiResourceParamWithRefDataList& request)
{
    using Ec2SetResourceParamsAsyncFuncPointer =
        void(MediaServerClient::*)(
            const ec2::ApiResourceParamWithRefDataList&,
            std::function<void(ec2::ErrorCode)>);

    return syncCallWrapper(
        this,
        static_cast<Ec2SetResourceParamsAsyncFuncPointer>(
            &MediaServerClient::ec2SetResourceParams),
        request);
}

void MediaServerClient::ec2GetResourceParams(
    const QnUuid& resourceId,
    std::function<void(ec2::ErrorCode, ec2::ApiResourceParamDataList)> completionHandler)
{
    performAsyncEc2Call(
        lm("ec2/getResourceParams?id=%1")
            .arg(resourceId.toSimpleString().toStdString()).toStdString(),
        std::move(completionHandler));
}

ec2::ErrorCode MediaServerClient::ec2GetResourceParams(
    const QnUuid& resourceId,
    ec2::ApiResourceParamDataList* result)
{
    using Ec2GetResourceParamsAsyncFuncPointer =
        void(MediaServerClient::*)(
            const QnUuid&,
            std::function<void(ec2::ErrorCode, ec2::ApiResourceParamDataList)>);

    return syncCallWrapper(
        this,
        static_cast<Ec2GetResourceParamsAsyncFuncPointer>(
            &MediaServerClient::ec2GetResourceParams),
        resourceId,
        result);
}

void MediaServerClient::ec2GetSystemMergeHistory(
    std::function<void(ec2::ErrorCode, ec2::ApiSystemMergeHistoryRecordList)> completionHandler)
{
    performAsyncEc2Call("ec2/getSystemMergeHistory", std::move(completionHandler));
}

ec2::ErrorCode MediaServerClient::ec2GetSystemMergeHistory(
    ec2::ApiSystemMergeHistoryRecordList* result)
{
    using Ec2GetSystemMergeHistoryAsyncFuncPointer =
        void(MediaServerClient::*)(
            std::function<void(ec2::ErrorCode, ec2::ApiSystemMergeHistoryRecordList)>);

    return syncCallWrapper(
        this,
        static_cast<Ec2GetSystemMergeHistoryAsyncFuncPointer>(
            &MediaServerClient::ec2GetSystemMergeHistory),
        result);
}

void MediaServerClient::ec2AnalyticsLookupDetectedObjects(
    const nx::analytics::storage::Filter& request,
    std::function<void(ec2::ErrorCode, nx::analytics::storage::LookupResult)> completionHandler)
{
    performAsyncEc2Call(
        "ec2/analyticsLookupDetectedObjects",
        std::move(completionHandler));
}

ec2::ErrorCode MediaServerClient::ec2AnalyticsLookupDetectedObjects(
    const nx::analytics::storage::Filter& request,
    nx::analytics::storage::LookupResult* result)
{
    using AsyncFuncPointer =
        void(MediaServerClient::*)(
            const nx::analytics::storage::Filter&,
            std::function<void(ec2::ErrorCode, nx::analytics::storage::LookupResult)>);

    return syncCallWrapper(
        this,
        static_cast<AsyncFuncPointer>(&MediaServerClient::ec2AnalyticsLookupDetectedObjects),
        request,
        result);
}

nx_http::StatusCode::Value MediaServerClient::lastResponseHttpStatusCode() const
{
    return m_prevResponseHttpStatusCode;
}

void MediaServerClient::stopWhileInAioThread()
{
    m_activeClients.clear();
}

//-------------------------------------------------------------------------------------------------
// Utilities

ec2::ErrorCode MediaServerClient::toEc2ErrorCode(
    SystemError::ErrorCode systemErrorCode,
    nx_http::StatusCode::Value statusCode)
{
    if (systemErrorCode != SystemError::noError)
        return ec2::ErrorCode::ioError;
    if (!nx_http::StatusCode::isSuccessCode(statusCode))
        return ec2::ErrorCode::forbidden;

    return ec2::ErrorCode::ok;
}
