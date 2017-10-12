#pragma once

#include <map>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/socket_common.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/mutex.h>

#include <network/module_information.h>
#include <nx_ec/data/api_resource_data.h>
#include <nx_ec/ec_api.h>
#include <rest/server/json_rest_result.h>

#include "model/cloud_credentials_data.h"
#include "model/detach_from_cloud_data.h"
#include "model/merge_system_data.h"
#include "model/setup_local_system_data.h"
#include "model/setup_cloud_system_data.h"
#include "model/system_settings_reply.h"

class MediaServerClient:
    public nx::network::aio::BasicPollable
{
    using base_type = nx::network::aio::BasicPollable;

public:
    MediaServerClient(const QUrl& baseRequestUrl);
    
    MediaServerClient(const MediaServerClient&) = delete;
    MediaServerClient& operator=(const MediaServerClient&) = delete;
    MediaServerClient(MediaServerClient&&) = default;
    MediaServerClient& operator=(MediaServerClient&&) = default;

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread);

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

    void mergeSystems(
        const MergeSystemData& request,
        std::function<void(QnJsonRestResult)> completionHandler);
    QnJsonRestResult mergeSystems(const MergeSystemData& request);

    //---------------------------------------------------------------------------------------------
    // /ec2/ requests

    void ec2GetUsers(
        std::function<void(ec2::ErrorCode, ec2::ApiUserDataList)> completionHandler);
    ec2::ErrorCode ec2GetUsers(ec2::ApiUserDataList* result);

    void ec2SaveUser(
        const ec2::ApiUserData& request,
        std::function<void(ec2::ErrorCode)> completionHandler);
    ec2::ErrorCode ec2SaveUser(const ec2::ApiUserData& request);

    void ec2GetSettings(
        std::function<void(ec2::ErrorCode, ec2::ApiResourceParamDataList)> completionHandler);
    ec2::ErrorCode ec2GetSettings(ec2::ApiResourceParamDataList* result);

    void ec2SetResourceParams(
        const ec2::ApiResourceParamWithRefDataList& request,
        std::function<void(ec2::ErrorCode)> completionHandler);
    ec2::ErrorCode ec2SetResourceParams(const ec2::ApiResourceParamWithRefDataList& request);

    void ec2GetResourceParams(
        const QnUuid& resourceId,
        std::function<void(ec2::ErrorCode, ec2::ApiResourceParamDataList)> completionHandler);
    ec2::ErrorCode ec2GetResourceParams(
        const QnUuid& resourceId,
        ec2::ApiResourceParamDataList* result);

    /**
     * NOTE: Can only be called within request completion handler. 
     *   Otherwise, result is not defined.
     */
    nx_http::StatusCode::Value prevResponseHttpStatusCode() const;

protected:
    virtual void stopWhileInAioThread() override;

private:
    QUrl m_baseRequestUrl;
    QString m_userName;
    QString m_password;
    // TODO: #ak Replace with std::set in c++17.
    std::map<
        nx::network::aio::BasicPollable*,
        std::unique_ptr<nx::network::aio::BasicPollable>> m_activeClients;
    nx_http::StatusCode::Value m_prevResponseHttpStatusCode = 
        nx_http::StatusCode::undefined;

    template<typename Input, typename ... Output>
    void performGetRequest(
        const std::string& requestPath,
        const Input& inputData,
        std::function<void(
            SystemError::ErrorCode,
            nx_http::StatusCode::Value statusCode,
            Output...)> completionHandler);

    template<typename ... Output>
    void performGetRequest(
        const std::string& requestPath,
        std::function<void(
            SystemError::ErrorCode,
            nx_http::StatusCode::Value statusCode,
            Output...)> completionHandler);

    template<typename CreateHttpClientFunc, typename ... Output>
    void performGetRequest(
        CreateHttpClientFunc createHttpClientFunc,
        const std::string& requestPath,
        std::function<void(
            SystemError::ErrorCode,
            nx_http::StatusCode::Value statusCode,
            Output...)> completionHandler);

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

    QnRestResult::Error toApiErrorCode(
        SystemError::ErrorCode sysErrorCode,
        nx_http::StatusCode::Value statusCode);

    template<typename Input, typename ... Output>
    void performAsyncEc2Call(
        const std::string& requestName,
        const Input& request,
        std::function<void(ec2::ErrorCode, Output...)> completionHandler);

    template<typename ... Output>
    void performAsyncEc2Call(
        const std::string& requestName,
        std::function<void(ec2::ErrorCode, Output...)> completionHandler);

    ec2::ErrorCode toEc2ErrorCode(
        SystemError::ErrorCode systemErrorCode,
        nx_http::StatusCode::Value statusCode);
};
