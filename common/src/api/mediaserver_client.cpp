#include "mediaserver_client.h"

#include <nx/network/http/fusion_data_http_client.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/sync_call.h>
#include <nx/utils/type_utils.h>

MediaServerClient::MediaServerClient(const nx::utils::Url& baseRequestUrl):
    m_baseRequestUrl(baseRequestUrl)
{
}

void MediaServerClient::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    for (auto& val: m_activeClients)
        val.first->bindToAioThread(aioThread);
}

void MediaServerClient::setUserCredentials(const nx_http::Credentials& userCredentials)
{
    m_userCredentials = userCredentials;
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
        static_cast<Ec2GetResourceParamsAsyncFuncPointer>(
            &MediaServerClient::ec2GetResourceParams),
        resourceId,
        result);
}

nx_http::StatusCode::Value MediaServerClient::prevResponseHttpStatusCode() const
{
    return m_prevResponseHttpStatusCode;
}

void MediaServerClient::stopWhileInAioThread()
{
    m_activeClients.clear();
}

//-------------------------------------------------------------------------------------------------
// Utilities

template<typename Input, typename ... Output>
void MediaServerClient::performGetRequest(
    const std::string& requestPath,
    const Input& inputData,
    std::function<void(
        SystemError::ErrorCode,
        nx_http::StatusCode::Value statusCode,
        Output...)> completionHandler)
{
    using ActualOutputType =
        typename nx::utils::tuple_first_element<void, std::tuple<Output...>>::type;

    performGetRequest(
        [&inputData](const nx::utils::Url& url, nx_http::AuthInfo authInfo)
        {
            return std::make_unique<nx_http::FusionDataHttpClient<Input, ActualOutputType>>(
                url, std::move(authInfo), inputData);
        },
        requestPath,
        std::move(completionHandler));
}

template<typename ... Output>
void MediaServerClient::performGetRequest(
    const std::string& requestPath,
    std::function<void(
        SystemError::ErrorCode,
        nx_http::StatusCode::Value statusCode,
        Output...)> completionHandler)
{
    using ActualOutputType =
        typename nx::utils::tuple_first_element<void, std::tuple<Output...>>::type;

    performGetRequest(
        [](const nx::utils::Url& url, nx_http::AuthInfo authInfo)
        {
            return std::make_unique<nx_http::FusionDataHttpClient<void, ActualOutputType>>(
                url, std::move(authInfo));
        },
        requestPath,
        std::move(completionHandler));
}

template<typename CreateHttpClientFunc, typename ... Output>
void MediaServerClient::performGetRequest(
    CreateHttpClientFunc createHttpClientFunc,
    const std::string& requestPath,
    std::function<void(
        SystemError::ErrorCode,
        nx_http::StatusCode::Value statusCode,
        Output...)> completionHandler)
{
    using ActualOutputType =
        typename nx::utils::tuple_first_element<void, std::tuple<Output...>>::type;

    nx::utils::Url requestUrl = nx::network::url::Builder(m_baseRequestUrl)
        .appendPath(QLatin1String("/"))
        .appendPath(QString::fromStdString(requestPath)).toUrl();
    nx_http::AuthInfo authInfo;
    authInfo.user = m_userCredentials;

    auto fusionClient = createHttpClientFunc(requestUrl, std::move(authInfo));

    post(
        [this,
            fusionClient = std::move(fusionClient),
            completionHandler = std::move(completionHandler)]() mutable
        {
            fusionClient->bindToAioThread(getAioThread());
            auto fusionClientPtr = fusionClient.get();
            m_activeClients.emplace(fusionClientPtr, std::move(fusionClient));
            fusionClientPtr->execute(
                [this, fusionClientPtr, completionHandler = std::move(completionHandler)](
                    SystemError::ErrorCode errorCode,
                    const nx_http::Response* response,
                    Output... outData)
                {
                    auto clientIter = m_activeClients.find(fusionClientPtr);
                    NX_ASSERT(clientIter != m_activeClients.end());
                    auto client = std::move(clientIter->second);
                    m_activeClients.erase(clientIter);

                    m_prevResponseHttpStatusCode = 
                        response
                        ? (nx_http::StatusCode::Value)response->statusLine.statusCode
                        : nx_http::StatusCode::undefined;

                    return completionHandler(
                        errorCode,
                        m_prevResponseHttpStatusCode,
                        std::move(outData)...);
                });
        });
}

template<typename ResultCode, typename Output>
ResultCode MediaServerClient::syncCallWrapper(
    void(MediaServerClient::*asyncFunc)(std::function<void(ResultCode, Output)>),
    Output* output)
{
    ResultCode resultCode;
    std::tie(resultCode, *output) =
        makeSyncCall<ResultCode, Output>(std::bind(asyncFunc, this, std::placeholders::_1));
    return resultCode;
}

template<typename ResultCode, typename Input>
ResultCode MediaServerClient::syncCallWrapper(
    void(MediaServerClient::*asyncFunc)(const Input&, std::function<void(ResultCode)>),
    const Input& input)
{
    using namespace std::placeholders;

    ResultCode resultCode;
    std::tie(resultCode) = makeSyncCall<ResultCode>(std::bind(asyncFunc, this, input, _1));
    return resultCode;
}

template<typename ResultCode, typename Input, typename Output>
ResultCode MediaServerClient::syncCallWrapper(
    void(MediaServerClient::*asyncFunc)(const Input&, std::function<void(ResultCode, Output)>),
    const Input& input,
    Output* output)
{
    using namespace std::placeholders;

    ResultCode resultCode;
    std::tie(resultCode, *output) =
        makeSyncCall<ResultCode, Output>(std::bind(asyncFunc, this, input, _1));
    return resultCode;
}

template<typename Input>
void MediaServerClient::performApiRequest(
    const std::string& requestName,
    const Input& input,
    std::function<void(QnJsonRestResult)> completionHandler)
{
    performGetRequest<Input, QnJsonRestResult>(
        requestName,
        input,
        std::function<void(SystemError::ErrorCode, nx_http::StatusCode::Value, QnJsonRestResult)>(
            [this, completionHandler = std::move(completionHandler)](
                SystemError::ErrorCode sysErrorCode,
                nx_http::StatusCode::Value statusCode,
                QnJsonRestResult result)
            {
                result.error = toApiErrorCode(sysErrorCode, statusCode);
                completionHandler(std::move(result));
            }));
}

template<typename Output>
void MediaServerClient::performApiRequest(
    const std::string& requestName,
    std::function<void(QnJsonRestResult, Output)> completionHandler)
{
    performGetRequest<QnJsonRestResult>(
        requestName,
        std::function<void(SystemError::ErrorCode, nx_http::StatusCode::Value, QnJsonRestResult)>(
            [this, completionHandler = std::move(completionHandler)](
                SystemError::ErrorCode sysErrorCode,
                nx_http::StatusCode::Value statusCode,
                QnJsonRestResult result)
            {
                Output output;
                result.error = toApiErrorCode(sysErrorCode, statusCode);
                if (result.error == QnRestResult::NoError)
                    output = result.deserialized<Output>();
                completionHandler(std::move(result), std::move(output));
            }));
}

QnRestResult::Error MediaServerClient::toApiErrorCode(
    SystemError::ErrorCode sysErrorCode,
    nx_http::StatusCode::Value statusCode)
{
    if (sysErrorCode != SystemError::noError)
        return QnRestResult::CantProcessRequest;
    if (!nx_http::StatusCode::isSuccessCode(statusCode))
        return QnRestResult::CantProcessRequest;
    return QnRestResult::NoError;
}

template<typename Input, typename ... Output>
void MediaServerClient::performAsyncEc2Call(
    const std::string& requestName,
    const Input& request,
    std::function<void(ec2::ErrorCode, Output...)> completionHandler)
{
    performGetRequest<Output...>(
        requestName,
        request,
        std::function<void(SystemError::ErrorCode, nx_http::StatusCode::Value)>(
            [this, completionHandler = std::move(completionHandler)](
                SystemError::ErrorCode sysErrorCode,
                nx_http::StatusCode::Value statusCode,
                Output... output)
            {
                completionHandler(
                    toEc2ErrorCode(sysErrorCode, statusCode),
                    std::move(output)...);
            }));
}

template<typename ... Output>
void MediaServerClient::performAsyncEc2Call(
    const std::string& requestName,
    std::function<void(ec2::ErrorCode, Output...)> completionHandler)
{
    performGetRequest<Output...>(
        requestName,
        std::function<void(SystemError::ErrorCode, nx_http::StatusCode::Value, Output...)>(
            [this, completionHandler = std::move(completionHandler)](
                SystemError::ErrorCode sysErrorCode,
                nx_http::StatusCode::Value statusCode,
                Output... result)
            {
                completionHandler(
                    toEc2ErrorCode(sysErrorCode, statusCode),
                    std::move(result)...);
            }));
}

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
