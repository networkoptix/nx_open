#include "mediaserver_client.h"

#include <nx/network/http/fusion_data_http_client.h>
#include <nx/utils/type_utils.h>

#include <utils/common/sync_call.h>

MediaServerClient::MediaServerClient(const SocketAddress& mediaServerEndpoint):
    m_mediaServerEndpoint(mediaServerEndpoint)
{
}

void MediaServerClient::setUserName(const QString& userName)
{
    m_userName = userName;
}

void MediaServerClient::setPassword(const QString& password)
{
    m_password = password;
}

//-------------------------------------------------------------------------------------------------
// /api/ requests

void MediaServerClient::saveCloudSystemCredentials(
    const CloudCredentialsData& inputData,
    std::function<void(QnJsonRestResult)> completionHandler)
{
    performApiRequest("api/saveCloudSystemCredentials", inputData, std::move(completionHandler));
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
        static_cast<SetupLocalSystemAsyncFuncPointer>(
            &MediaServerClient::setupLocalSystem),
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

//-------------------------------------------------------------------------------------------------
// /ec2/ requests

void MediaServerClient::ec2SetResourceParams(
    const ec2::ApiResourceParamWithRefDataList& inputData,
    std::function<void(ec2::ErrorCode)> completionHandler)
{
    performAsyncEc2Call("ec2/setResourceParams", inputData, std::move(completionHandler));
}

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

//-------------------------------------------------------------------------------------------------
// Utilities

template<typename Input, typename ... Output>
void MediaServerClient::performGetRequest(
    const QString& requestPath,
    const Input& inputData,
    std::function<void(SystemError::ErrorCode, Output...)> completionHandler)
{
    using ActualOutputType =
        typename nx::utils::tuple_first_element<void, std::tuple<Output...>>::type;

    //TODO #ak save client to container and remove in destructor

    QUrl url(lm("http://%1/%2").arg(m_mediaServerEndpoint.toString()).arg(requestPath));
    url.setUserName(m_userName);
    url.setPassword(m_password);
    nx_http::AuthInfo authInfo;
    authInfo.username = m_userName;
    authInfo.password = m_password;
    auto fusionClient = std::make_shared<nx_http::FusionDataHttpClient<Input, ActualOutputType>>(
        url, std::move(authInfo), inputData);
    auto fusionClientPtr = fusionClient.get();
    fusionClientPtr->execute(
        [fusionClient = std::move(fusionClient),
            completionHandler = std::move(completionHandler)](
                SystemError::ErrorCode errorCode,
                const nx_http::Response* /*response*/,
                Output... outData)
        {
            return completionHandler(errorCode, std::move(outData)...);
        });
}

template<typename ... Output>
void MediaServerClient::performGetRequest(
    const QString& requestPath,
    std::function<void(SystemError::ErrorCode, Output...)> completionHandler)
{
    // TODO: #ak think about a way to combine this method with the previous one

    using ActualOutputType =
        typename nx::utils::tuple_first_element<void, std::tuple<Output...>>::type;

    QUrl url(lm("http://%1/%2").arg(m_mediaServerEndpoint.toString()).arg(requestPath));
    url.setUserName(m_userName);
    url.setPassword(m_password);
    nx_http::AuthInfo authInfo;
    authInfo.username = m_userName;
    authInfo.password = m_password;
    auto fusionClient = std::make_shared<nx_http::FusionDataHttpClient<void, ActualOutputType>>(
        url, std::move(authInfo));
    auto fusionClientPtr = fusionClient.get();
    fusionClientPtr->execute(
        [fusionClient = std::move(fusionClient),
            completionHandler = std::move(completionHandler)](
                SystemError::ErrorCode errorCode,
                const nx_http::Response* /*response*/,
                Output... outData)
        {
            return completionHandler(errorCode, std::move(outData)...);
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

template<typename Input>
void MediaServerClient::performApiRequest(
    const char* requestName,
    const Input& input,
    std::function<void(QnJsonRestResult)> completionHandler)
{
    performGetRequest<Input, QnJsonRestResult>(
        requestName,
        input,
        std::function<void(SystemError::ErrorCode, QnJsonRestResult)>(
            [completionHandler = std::move(completionHandler)](
                SystemError::ErrorCode sysErrorCode, QnJsonRestResult result)
            {
                if (sysErrorCode != SystemError::noError)
                    result.error = QnRestResult::CantProcessRequest;
                completionHandler(std::move(result));
            }));
}

template<typename Output>
void MediaServerClient::performApiRequest(
    const char* requestName,
    std::function<void(QnJsonRestResult, Output)> completionHandler)
{
    performGetRequest<QnJsonRestResult>(
        requestName,
        std::function<void(SystemError::ErrorCode, QnJsonRestResult)>(
            [completionHandler = std::move(completionHandler)](
                SystemError::ErrorCode sysErrorCode, QnJsonRestResult result)
            {
                Output output;
                if (sysErrorCode == SystemError::noError)
                    output = result.deserialized<Output>();
                else
                    result.error = QnRestResult::CantProcessRequest;
                completionHandler(std::move(result), std::move(output));
            }));
}

template<typename Input, typename ... Output>
void MediaServerClient::performAsyncEc2Call(
    const char* requestName,
    const Input& request,
    std::function<void(ec2::ErrorCode, Output...)> completionHandler)
{
    performGetRequest<Output...>(
        requestName,
        request,
        std::function<void(SystemError::ErrorCode)>(
            [this, completionHandler = std::move(completionHandler)](
                SystemError::ErrorCode sysErrorCode,
                Output... output)
            {
                completionHandler(
                    systemErrorCodeToEc2Error(sysErrorCode),
                    std::move(output)...);
            }));
}

template<typename ... Output>
void MediaServerClient::performAsyncEc2Call(
    const char* requestName,
    std::function<void(ec2::ErrorCode, Output...)> completionHandler)
{
    performGetRequest<Output...>(
        requestName,
        std::function<void(SystemError::ErrorCode, Output...)>(
            [this, completionHandler = std::move(completionHandler)](
                SystemError::ErrorCode sysErrorCode, Output... result)
            {
                completionHandler(
                    systemErrorCodeToEc2Error(sysErrorCode),
                    std::move(result)...);
            }));
}

ec2::ErrorCode MediaServerClient::systemErrorCodeToEc2Error(
    SystemError::ErrorCode systemErrorCode)
{
    return systemErrorCode == SystemError::noError
        ? ec2::ErrorCode::ok
        : ec2::ErrorCode::ioError;
}
