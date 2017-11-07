#pragma once

#include <chrono>
#include <map>
#include <list>

#include <boost/optional.hpp>

#include <QtCore/QString>
#include <QtCore/QUrlQuery>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/fusion_data_http_client.h>
#include <nx/network/socket_common.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/sync_call.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/type_utils.h>

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
    MediaServerClient(const nx::utils::Url &baseRequestUrl);
    
    MediaServerClient(const MediaServerClient&) = delete;
    MediaServerClient& operator=(const MediaServerClient&) = delete;
    MediaServerClient(MediaServerClient&&) = default;
    MediaServerClient& operator=(MediaServerClient&&) = default;

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread);

    void setUserCredentials(const nx_http::Credentials& userCredentials);
    /**
     * Authentication through query param.
     */
    void setAuthenticationKey(const QString& key);
    void setRequestTimeout(std::chrono::milliseconds timeout);

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
    nx_http::StatusCode::Value lastResponseHttpStatusCode() const;

protected:
    virtual void stopWhileInAioThread() override;

    template<typename Input, typename ... Output>
    void performGetRequest(
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
    void performGetRequest(
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
    void performGetRequest(
        CreateHttpClientFunc createHttpClientFunc,
        const std::string& requestPath,
        std::function<void(
            SystemError::ErrorCode,
            nx_http::StatusCode::Value statusCode,
            Output...)> completionHandler)
    {
        nx::utils::Url requestUrl = nx::network::url::Builder(m_baseRequestUrl)
            .appendPath(QLatin1String("/"))
            .appendPath(QString::fromStdString(requestPath)).toUrl();
        if (!m_authenticationKey.isEmpty())
        {
            QUrlQuery query(requestUrl.query());
            query.addQueryItem(QLatin1String(Qn::URL_QUERY_AUTH_KEY_NAME), m_authenticationKey);
            requestUrl.setQuery(query);
        }
        nx_http::AuthInfo authInfo;
        if (m_userCredentials)
            authInfo.user = *m_userCredentials;

        auto fusionClient = createHttpClientFunc(requestUrl, std::move(authInfo));
        if (m_requestTimeout)
            fusionClient->setRequestTimeout(*m_requestTimeout);

        post(
            [this, fusionClient = std::move(fusionClient),
                completionHandler = std::move(completionHandler)]() mutable
            {
                executeRequest<decltype(fusionClient), decltype(completionHandler), Output...>(
                    std::move(fusionClient), std::move(completionHandler));
            });
    }

    template<typename HttpClient, typename CompletionHandler, typename ... Output>
    void executeRequest(
        HttpClient httpClient,
        CompletionHandler completionHandler)
    {
        httpClient->bindToAioThread(getAioThread());
        auto httpClientPtr = httpClient.get();
        m_activeClients.push_back(std::move(httpClient));
        httpClientPtr->execute(
            [this, fusionClientIter = --m_activeClients.end(),
                completionHandler = std::move(completionHandler)](
                    SystemError::ErrorCode errorCode,
                    const nx_http::Response* response,
                    Output... outData) mutable
            {
                auto client = std::move(*fusionClientIter);
                m_activeClients.erase(fusionClientIter);

                m_prevResponseHttpStatusCode = 
                    response
                    ? (nx_http::StatusCode::Value)response->statusLine.statusCode
                    : nx_http::StatusCode::undefined;

                return completionHandler(
                    errorCode,
                    m_prevResponseHttpStatusCode,
                    std::move(outData)...);
            });
    }

    template<typename ClientType, typename ResultCode, typename Output>
    ResultCode syncCallWrapper(
        ClientType* clientType,
        void(ClientType::*asyncFunc)(std::function<void(ResultCode, Output)>),
        Output* output)
    {
        ResultCode resultCode;
        std::tie(resultCode, *output) =
            makeSyncCall<ResultCode, Output>(std::bind(asyncFunc, clientType, std::placeholders::_1));
        return resultCode;
    }

    template<typename ClientType, typename ResultCode, typename Input>
    ResultCode syncCallWrapper(
        ClientType* clientType,
        void(ClientType::*asyncFunc)(const Input&, std::function<void(ResultCode)>),
        const Input& input)
    {
        using namespace std::placeholders;

        ResultCode resultCode;
        std::tie(resultCode) = makeSyncCall<ResultCode>(std::bind(asyncFunc, clientType, input, _1));
        return resultCode;
    }

    template<typename ClientType, typename ResultCode, typename Input, typename Output>
    ResultCode syncCallWrapper(
        ClientType* clientType,
        void(ClientType::*asyncFunc)(const Input&, std::function<void(ResultCode, Output)>),
        const Input& input,
        Output* output)
    {
        using namespace std::placeholders;

        ResultCode resultCode;
        std::tie(resultCode, *output) =
            makeSyncCall<ResultCode, Output>(std::bind(asyncFunc, clientType, input, _1));
        return resultCode;
    }

    template<typename Input>
    void performApiRequest(
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
                    if (sysErrorCode != SystemError::noError ||
                        (result.error == QnRestResult::Error::NoError &&
                            !nx_http::StatusCode::isSuccessCode(statusCode)))
                    {
                        result.error = toApiErrorCode(sysErrorCode, statusCode);
                    }
                    completionHandler(std::move(result));
                }));
    }

    template<typename Output>
    void performApiRequest(
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
                    if (sysErrorCode != SystemError::noError ||
                        (result.error == QnRestResult::Error::NoError &&
                            !nx_http::StatusCode::isSuccessCode(statusCode)))
                    {
                        result.error = toApiErrorCode(sysErrorCode, statusCode);
                    }

                    Output output;
                    if (result.error == QnRestResult::NoError)
                        output = result.deserialized<Output>();

                    completionHandler(std::move(result), std::move(output));
                }));
    }

    QnRestResult::Error toApiErrorCode(
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
    void performAsyncEc2Call(
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
    void performAsyncEc2Call(
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

    ec2::ErrorCode toEc2ErrorCode(
        SystemError::ErrorCode systemErrorCode,
        nx_http::StatusCode::Value statusCode);

private:
    boost::optional<std::chrono::milliseconds> m_requestTimeout;
    const nx::utils::Url m_baseRequestUrl;
    boost::optional<nx_http::Credentials> m_userCredentials;
    std::list<std::unique_ptr<nx::network::aio::BasicPollable>> m_activeClients;
    nx_http::StatusCode::Value m_prevResponseHttpStatusCode =
        nx_http::StatusCode::undefined;
    QString m_authenticationKey;
};
