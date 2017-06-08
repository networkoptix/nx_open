#pragma once

#include <nx/network/socket_common.h>
#include <nx/utils/move_only_func.h>

#include <api/model/cloud_credentials_data.h>
#include <api/model/detach_from_cloud_data.h>
#include <api/model/setup_local_system_data.h>
#include <api/model/setup_cloud_system_data.h>
#include <api/model/system_settings_reply.h>
#include <network/module_information.h>
#include <nx_ec/data/api_resource_data.h>
#include <nx_ec/ec_api.h>
#include <rest/server/json_rest_result.h>

class MediaServerClient
{
public:
    MediaServerClient(const SocketAddress& mediaServerEndpoint);
    
    MediaServerClient(const MediaServerClient&) = delete;
    MediaServerClient& operator=(const MediaServerClient&) = delete;
    MediaServerClient(MediaServerClient&&) = default;
    MediaServerClient& operator=(MediaServerClient&&) = default;

    void setUserName(const QString& userName);
    void setPassword(const QString& password);

    //---------------------------------------------------------------------------------------------
    // /api/ requests

    void saveCloudSystemCredentials(
        const CloudCredentialsData& request,
        std::function<void(QnJsonRestResult)> completionHandler);
    QnJsonRestResult saveCloudSystemCredentials(const CloudCredentialsData& request);

    void getModuleInformation(
        std::function<void(QnJsonRestResult, QnModuleInformation)> completionHandler);
    QnJsonRestResult getModuleInformation(QnModuleInformation* result);

    void setupLocalSystem(
        const SetupLocalSystemData& request,
        std::function<void(QnJsonRestResult)> completionHandler);
    QnJsonRestResult setupLocalSystem(const SetupLocalSystemData& request);

    void setupCloudSystem(
        const SetupCloudSystemData& request,
        std::function<void(QnJsonRestResult)> completionHandler);
    QnJsonRestResult setupCloudSystem(const SetupCloudSystemData& request);

    void detachFromCloud(
        const DetachFromCloudData& request,
        std::function<void(QnJsonRestResult)> completionHandler);
    QnJsonRestResult detachFromCloud(const DetachFromCloudData& request);

    //---------------------------------------------------------------------------------------------
    // /ec2/ requests

    void ec2SetResourceParams(
        const ec2::ApiResourceParamWithRefDataList& request,
        std::function<void(ec2::ErrorCode)> completionHandler);

    void ec2GetUsers(
        std::function<void(ec2::ErrorCode, ec2::ApiUserDataList)> completionHandler);
    ec2::ErrorCode ec2GetUsers(ec2::ApiUserDataList* result);

    void ec2GetSettings(
        std::function<void(ec2::ErrorCode, ec2::ApiResourceParamDataList)> completionHandler);
    ec2::ErrorCode ec2GetSettings(ec2::ApiResourceParamDataList* result);

    void ec2GetResourceParams(
        const QnUuid& resourceId,
        std::function<void(ec2::ErrorCode, ec2::ApiResourceParamDataList)> completionHandler);
    ec2::ErrorCode ec2GetResourceParams(
        const QnUuid& resourceId,
        ec2::ApiResourceParamDataList* result);

private:
    SocketAddress m_mediaServerEndpoint;
    QString m_userName;
    QString m_password;

    template<typename Input, typename ... Output>
    void performGetRequest(
        const std::string& requestPath,
        const Input& inputData,
        std::function<void(SystemError::ErrorCode, Output...)> completionHandler);

    template<typename ... Output>
    void performGetRequest(
        const std::string& requestPath,
        std::function<void(SystemError::ErrorCode, Output...)> completionHandler);

    template<typename ResultCode, typename Output>
    ResultCode syncCallWrapper(
        void(MediaServerClient::*asyncFunc)(std::function<void(ResultCode, Output)>),
        Output* output);

    template<typename ResultCode, typename Input>
    ResultCode syncCallWrapper(
        void(MediaServerClient::*asyncFunc)(const Input&, std::function<void(ResultCode)>),
        const Input& input);

    template<typename ResultCode, typename Input, typename Output>
    ResultCode syncCallWrapper(
        void(MediaServerClient::*asyncFunc)(const Input&, std::function<void(ResultCode, Output)>),
        const Input& input,
        Output* output);

    template<typename Input>
    void performApiRequest(
        const std::string& requestName,
        const Input& input,
        std::function<void(QnJsonRestResult)> completionHandler);

    template<typename Output>
    void performApiRequest(
        const std::string& requestName,
        std::function<void(QnJsonRestResult, Output)> completionHandler);

    template<typename Input, typename ... Output>
    void performAsyncEc2Call(
        const std::string& requestName,
        const Input& request,
        std::function<void(ec2::ErrorCode, Output...)> completionHandler);

    template<typename ... Output>
    void performAsyncEc2Call(
        const std::string& requestName,
        std::function<void(ec2::ErrorCode, Output...)> completionHandler);

    ec2::ErrorCode systemErrorCodeToEc2Error(SystemError::ErrorCode systemErrorCode);
};
