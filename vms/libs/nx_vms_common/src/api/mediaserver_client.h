// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <list>
#include <map>

#include <QtCore/QString>
#include <QtCore/QUrlQuery>

#include <analytics/db/analytics_db_types.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/fusion_data_http_client.h>
#include <nx/network/rest/result.h>
#include <nx/network/socket_common.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/sync_call.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/type_utils.h>
#include <nx/vms/api/data/camera_data.h>
#include <nx/vms/api/data/database_dump_data.h>
#include <nx/vms/api/data/media_server_data.h>
#include <nx/vms/api/data/merge_status_reply.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/api/data/resource_data.h>
#include <nx/vms/api/data/site_merge_data.h>
#include <nx/vms/api/data/system_merge_history_record.h>
#include <nx/vms/api/data/user_model.h>
#include <nx_ec/ec_api_fwd.h>
#include <utils/common/nxfusion_wrapper.h>

#include "model/merge_system_data.h"
#include "model/system_settings_reply.h"

class NX_VMS_COMMON_API MediaServerClient: public nx::network::aio::BasicPollable
{
    using base_type = nx::network::aio::BasicPollable;

public:
    MediaServerClient(
        const nx::utils::Url& baseRequestUrl, nx::network::ssl::AdapterFunc adapterFunc);
    ~MediaServerClient();

    MediaServerClient(const MediaServerClient&) = delete;
    MediaServerClient& operator=(const MediaServerClient&) = delete;

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    void setCredentials(const nx::network::http::Credentials& userCredentials);
    std::optional<nx::network::http::Credentials> getCredentials() const;

    /**
     * Exchange credentials for session id at the mediaserver synchronously.
     */
    bool login(const std::string& username, const std::string& password);

    using MergeSystemsHandler = std::function<void(
        SystemError::ErrorCode,
        nx::network::http::StatusCode::Value,
        nx::vms::api::MergeStatusReply)>;

    using UpdateCloudStorageHandler = std::function<void(SystemError::ErrorCode,
        nx::network::http::StatusCode::Value)>;

    void mergeSystems(
        const nx::vms::api::SiteMergeData& request, MergeSystemsHandler completionHandler);

    void updateCloudStorage(UpdateCloudStorageHandler completionHandler);

    /**
     * Authentication through query param.
     */
    void setAuthenticationKey(const QString& key);
    void setRequestTimeout(std::chrono::milliseconds timeout);

    //---------------------------------------------------------------------------------------------
    // /rest/ requests

    void getUsers(
        std::function<void(ec2::ErrorCode, std::vector<nx::vms::api::UserModel>)> completionHandler);
    ec2::ErrorCode getUsers(std::vector<nx::vms::api::UserModel>* result);

    void getUser(
        const QnUuid& id,
        std::function<void(ec2::ErrorCode, nx::vms::api::UserModel)> completionHandler);
    ec2::ErrorCode getUser(const QnUuid& id, nx::vms::api::UserModel* result);

    void saveUser(
        const nx::vms::api::UserModel& request,
        std::function<void(ec2::ErrorCode)> completionHandler);
    ec2::ErrorCode saveUser(const nx::vms::api::UserModel& request);

    void removeUser(const QnUuid& id, std::function<void(ec2::ErrorCode)> completionHandler);
    ec2::ErrorCode removeUser(const QnUuid& id);

    //---------------------------------------------------------------------------------------------
    // /api/ requests

    void getModuleInformation(
        std::function<void(nx::network::rest::JsonResult, nx::vms::api::ModuleInformation)>
            completionHandler);
    nx::network::rest::JsonResult getModuleInformation(nx::vms::api::ModuleInformation* result);

    // TODO: remove this when VMS gateway will use mergeSystems.
    void deprecatedMergeSystems(
        const MergeSystemData& request,
        std::function<void(nx::network::rest::JsonResult)> completionHandler);

    //---------------------------------------------------------------------------------------------
    // /ec2/ requests

    void ec2GetMediaServersEx(
        std::function<void(ec2::ErrorCode, nx::vms::api::MediaServerDataExList)> completionHandler);
    ec2::ErrorCode ec2GetMediaServersEx(nx::vms::api::MediaServerDataExList* result);

    void ec2SaveMediaServer(
        const nx::vms::api::MediaServerData& request,
        std::function<void(ec2::ErrorCode)> completionHandler);
    ec2::ErrorCode ec2SaveMediaServer(const nx::vms::api::MediaServerData& request);

    void ec2GetStorages(
        const std::optional<QnUuid>& serverId,
        std::function<void(ec2::ErrorCode, nx::vms::api::StorageDataList)> completionHandler);

    ec2::ErrorCode ec2GetStorages(
        const std::optional<QnUuid>& serverId,
        nx::vms::api::StorageDataList* result);

    void ec2GetSettings(
        std::function<void(
            ec2::ErrorCode,
            nx::vms::api::ResourceParamDataList)> completionHandler);
    ec2::ErrorCode ec2GetSettings(nx::vms::api::ResourceParamDataList* result);

    void ec2SetResourceParams(
        const nx::vms::api::ResourceParamWithRefDataList& request,
        std::function<void(ec2::ErrorCode)> completionHandler);
    ec2::ErrorCode ec2SetResourceParams(const nx::vms::api::ResourceParamWithRefDataList& request);

    void ec2GetResourceParams(
        const QnUuid& resourceId,
        std::function<void(ec2::ErrorCode, nx::vms::api::ResourceParamDataList)> completionHandler);
    ec2::ErrorCode ec2GetResourceParams(
        const QnUuid& resourceId,
        nx::vms::api::ResourceParamDataList* result);

    void ec2GetSystemMergeHistory(std::function<
        void(ec2::ErrorCode, nx::vms::api::SystemMergeHistoryRecordList)> completionHandler);

    ec2::ErrorCode ec2GetSystemMergeHistory(nx::vms::api::SystemMergeHistoryRecordList* result);

    void ec2AnalyticsLookupObjectTracks(
        const nx::analytics::db::Filter& request,
        std::function<void(ec2::ErrorCode, nx::analytics::db::LookupResult)> completionHandler);
    ec2::ErrorCode ec2AnalyticsLookupObjectTracks(
        const nx::analytics::db::Filter& request,
        nx::analytics::db::LookupResult* result);

    void ec2DumpDatabase(
        std::function<void(ec2::ErrorCode, nx::vms::api::DatabaseDumpData)> completionHandler);
    ec2::ErrorCode ec2DumpDatabase(nx::vms::api::DatabaseDumpData* dump);

    void ec2RestoreDatabase(
        const nx::vms::api::DatabaseDumpData& dump,
        std::function<void(ec2::ErrorCode)> completionHandler);
    ec2::ErrorCode ec2RestoreDatabase(const nx::vms::api::DatabaseDumpData& dump);

    void ec2GetCameras(
        std::function<void(ec2::ErrorCode, nx::vms::api::CameraDataList)> completionHandler);
    ec2::ErrorCode ec2GetCameras(nx::vms::api::CameraDataList* cameras);

    SystemError::ErrorCode prevRequestSysErrorCode() const;

    /**
     * NOTE: Can only be called within request completion handler.
     *   Otherwise, result is not defined.
     */
    nx::network::http::StatusLine lastResponseHttpStatusLine() const;

protected:
    using SerializationImpl = nx::vms::common::detail::NxFusionWrapper;

    virtual void stopWhileInAioThread() override;

    // TODO: #akolesnikov Return std::variant<nx::network::rest::Result, Response> here.
    template<typename Input, typename ... Output>
    void performRestApiRequest(
        nx::network::http::Method httpMethod,
        const std::string& requestPath,
        const Input& inputData,
        std::function<void(
            SystemError::ErrorCode,
            nx::network::http::StatusCode::Value statusCode,
            Output...
        )> completionHandler)
    {
        using ActualOutputType = nx::utils::tuple_first_element_t<std::tuple<Output...>>;

        const nx::utils::Url requestUrl = nx::network::url::Builder(m_baseRequestUrl)
            .appendPath("/").appendPath(requestPath).toUrl();
        nx::network::http::Credentials credentials;
        if (NX_ASSERT(m_credentials))
            credentials = *m_credentials;

        auto fusionClient =
            std::make_unique<nx::network::http::FusionDataHttpClient<
                Input, ActualOutputType, SerializationImpl>>(
                    requestUrl, std::move(credentials), m_adapterFunc, inputData);
        if (m_requestTimeout)
            fusionClient->setRequestTimeout(*m_requestTimeout);

        post(
            [this, httpMethod, fusionClient = std::move(fusionClient),
                completionHandler = std::move(completionHandler)]() mutable
            {
                executeRequest<decltype(fusionClient), decltype(completionHandler), Output...>(
                    httpMethod,
                    std::move(fusionClient),
                    std::move(completionHandler));
            });
    }


    void performRestApiRequest(nx::network::http::Method httpMethod,
        const std::string& requestPath,
        std::function<void(SystemError::ErrorCode,
            nx::network::http::StatusCode::Value statusCode)> completionHandler)
    {
        const nx::utils::Url requestUrl = nx::network::url::Builder(m_baseRequestUrl)
                                              .appendPath("/")
                                              .appendPath(requestPath)
                                              .toUrl();
        nx::network::http::Credentials credentials;
        if (NX_ASSERT(m_credentials))
            credentials = *m_credentials;

        auto fusionClient = std::make_unique<
            nx::network::http::FusionDataHttpClient<void, void, SerializationImpl>>(
            requestUrl, std::move(credentials), m_adapterFunc);
        if (m_requestTimeout)
            fusionClient->setRequestTimeout(*m_requestTimeout);

        post(
            [this,
                httpMethod,
                fusionClient = std::move(fusionClient),
                completionHandler = std::move(completionHandler)]() mutable
            {
                executeRequest<decltype(fusionClient), decltype(completionHandler)>(
                    httpMethod, std::move(fusionClient), std::move(completionHandler));
            });
    }

    template<typename Input, typename ... Output>
    void performRequest(
        nx::network::http::Method httpMethod,
        const std::string& requestPath,
        const Input& inputData,
        std::function<void(
            SystemError::ErrorCode,
            nx::network::http::StatusCode::Value statusCode,
            Output...)> completionHandler)
    {
        using ActualOutputType = nx::utils::tuple_first_element_t<std::tuple<Output...>>;

        performRequest(
            httpMethod,
            [&inputData, this](const nx::utils::Url& url, nx::network::http::Credentials credentials)
            {
                return std::make_unique<nx::network::http::FusionDataHttpClient<
                    Input, ActualOutputType, SerializationImpl>>(
                        url, std::move(credentials), m_adapterFunc, inputData);
            },
            requestPath,
            std::move(completionHandler));
    }

    template<typename ... Output>
    void performRequest(
        nx::network::http::Method httpMethod,
        const std::string& requestPath,
        std::function<void(
            SystemError::ErrorCode,
            nx::network::http::StatusCode::Value statusCode,
            Output...)> completionHandler)
    {
        using ActualOutputType = nx::utils::tuple_first_element_t<std::tuple<Output...>>;

        performRequest(
            httpMethod,
            [this](const nx::utils::Url& url, nx::network::http::Credentials credentials)
            {
                return std::make_unique<nx::network::http::FusionDataHttpClient<
                    void, ActualOutputType, SerializationImpl>>(
                        url, std::move(credentials), m_adapterFunc);
            },
            requestPath,
            std::move(completionHandler));
    }

    template<typename CreateHttpClientFunc, typename ... Output>
    void performRequest(
        nx::network::http::Method httpMethod,
        CreateHttpClientFunc createHttpClientFunc,
        std::string requestPath,
        std::function<void(
            SystemError::ErrorCode,
            nx::network::http::StatusCode::Value statusCode,
            Output...)> completionHandler)
    {
        const auto queryPos = requestPath.find('?');
        std::string query;
        if (queryPos != std::string::npos)
        {
            query = requestPath.substr(queryPos+1);
            requestPath.erase(queryPos, std::string::npos);
        }

        nx::utils::Url requestUrl = nx::network::url::Builder(m_baseRequestUrl)
            .appendPath("/").appendPath(requestPath).setQuery(query).toUrl();
        if (!m_authenticationKey.isEmpty())
        {
            QUrlQuery authQuery(requestUrl.query());
            authQuery.addQueryItem(Qn::URL_QUERY_AUTH_KEY_NAME, m_authenticationKey);
            requestUrl.setQuery(authQuery);
        }
        nx::network::http::Credentials credentials;
        if (m_credentials)
            credentials = *m_credentials;

        auto fusionClient = createHttpClientFunc(requestUrl, std::move(credentials));
        if (m_requestTimeout)
            fusionClient->setRequestTimeout(*m_requestTimeout);

        post(
            [this, httpMethod, fusionClient = std::move(fusionClient),
                completionHandler = std::move(completionHandler)]() mutable
            {
                executeRequest<decltype(fusionClient), decltype(completionHandler), Output...>(
                    httpMethod,
                    std::move(fusionClient),
                    std::move(completionHandler));
            });
    }

    template<typename HttpClient, typename CompletionHandler, typename ... Output>
    void executeRequest(
        nx::network::http::Method httpMethod,
        HttpClient httpClient,
        CompletionHandler completionHandler)
    {
        httpClient->bindToAioThread(getAioThread());
        auto httpClientPtr = httpClient.get();
        m_activeClients.push_back(std::move(httpClient));
        httpClientPtr->execute(
            httpMethod,
            [this, fusionClientIter = --m_activeClients.end(),
                completionHandler = std::move(completionHandler)](
                    SystemError::ErrorCode errorCode,
                    const nx::network::http::Response* response,
                    Output... outData) mutable
            {
                m_prevRequestSysErrorCode = errorCode;
                m_prevResponseHttpStatusLine =
                    response ? response->statusLine : nx::network::http::StatusLine();

                m_activeClients.erase(fusionClientIter);

                return completionHandler(
                    errorCode,
                    static_cast<nx::network::http::StatusCode::Value>(
                        m_prevResponseHttpStatusLine.statusCode),
                    std::move(outData)...);
            });
    }

    template<typename ClientType, typename ResultCode, typename... Output>
    ResultCode syncCallWrapper(
        ClientType* clientType,
        void(ClientType::*asyncFunc)(std::function<void(ResultCode, Output...)>),
        Output*... output)
    {
        ResultCode resultCode;
        std::tie(resultCode, *output...) =
            makeSyncCall<ResultCode, Output...>(std::bind(asyncFunc, clientType, std::placeholders::_1));
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
        nx::network::http::Method httpMethod,
        const std::string& requestName,
        const Input& input,
        std::function<void(nx::network::rest::JsonResult)> completionHandler)
    {
        performRequest<Input, nx::network::rest::JsonResult>(
            httpMethod,
            requestName,
            input,
            std::function<void(
                SystemError::ErrorCode,
                nx::network::http::StatusCode::Value,
                nx::network::rest::JsonResult)>(
                [this, completionHandler = std::move(completionHandler)](
                    SystemError::ErrorCode sysErrorCode,
                    nx::network::http::StatusCode::Value statusCode,
                    nx::network::rest::JsonResult result)
                {
                    if (sysErrorCode != SystemError::noError ||
                        (result.error == nx::network::rest::Result::Error::NoError &&
                            !nx::network::http::StatusCode::isSuccessCode(statusCode)))
                    {
                        result.error = toApiErrorCode(sysErrorCode, statusCode);
                    }
                    completionHandler(std::move(result));
                }));
    }

    template<typename... Output>
    void performApiRequest(
        nx::network::http::Method httpMethod,
        const std::string& requestName,
        std::function<void(nx::network::rest::JsonResult, Output...)> completionHandler)
    {
        performRequest<nx::network::rest::JsonResult>(
            httpMethod,
            requestName,
            std::function<void(
                SystemError::ErrorCode,
                nx::network::http::StatusCode::Value,
                nx::network::rest::JsonResult)>(
                [this, completionHandler = std::move(completionHandler)](
                    SystemError::ErrorCode sysErrorCode,
                    nx::network::http::StatusCode::Value statusCode,
                    nx::network::rest::JsonResult result)
                {
                    if (sysErrorCode != SystemError::noError ||
                        (result.error == nx::network::rest::Result::Error::NoError &&
                            !nx::network::http::StatusCode::isSuccessCode(statusCode)))
                    {
                        result.error = toApiErrorCode(sysErrorCode, statusCode);
                    }

                    reportApiRequestResult(std::move(result), std::move(completionHandler));
                }));
    }

    template<typename Output>
    void reportApiRequestResult(
        nx::network::rest::JsonResult result,
        std::function<void(nx::network::rest::JsonResult, Output)> completionHandler)
    {
        Output output;
        if (result.error == nx::network::rest::Result::NoError)
            output = result.deserialized<Output>();

        completionHandler(std::move(result), std::move(output));
    }

    void reportApiRequestResult(
        nx::network::rest::JsonResult result,
        std::function<void(nx::network::rest::JsonResult)> completionHandler)
    {
        completionHandler(std::move(result));
    }

    nx::network::rest::Result::Error toApiErrorCode(
        SystemError::ErrorCode sysErrorCode,
        nx::network::http::StatusCode::Value statusCode)
    {
        if (sysErrorCode != SystemError::noError)
            return nx::network::rest::Result::CantProcessRequest;
        if (!nx::network::http::StatusCode::isSuccessCode(statusCode))
            return nx::network::rest::Result::CantProcessRequest;
        return nx::network::rest::Result::NoError;
    }

    template<typename Input, typename ... Output>
    void performAsyncCall(
        nx::network::http::Method httpMethod,
        const std::string& requestName,
        const Input& request,
        std::function<void(ec2::ErrorCode, Output...)> completionHandler)
    {
        performRequest<Output...>(
            httpMethod,
            requestName,
            request,
            std::function<void(SystemError::ErrorCode, nx::network::http::StatusCode::Value)>(
                [this, completionHandler = std::move(completionHandler)](
                    SystemError::ErrorCode sysErrorCode,
                    nx::network::http::StatusCode::Value statusCode,
                    Output... output)
                {
                    completionHandler(
                        toEc2ErrorCode(sysErrorCode, statusCode),
                        std::move(output)...);
                }));
    }

    template<typename ... Output>
    void performAsyncCall(
        nx::network::http::Method httpMethod,
        const std::string& requestName,
        std::function<void(ec2::ErrorCode, Output...)> completionHandler)
    {
        performRequest<Output...>(
            httpMethod,
            requestName,
            std::function<void(SystemError::ErrorCode, nx::network::http::StatusCode::Value, Output...)>(
                [this, completionHandler = std::move(completionHandler)](
                    SystemError::ErrorCode sysErrorCode,
                    nx::network::http::StatusCode::Value statusCode,
                    Output... result)
                {
                    completionHandler(
                        toEc2ErrorCode(sysErrorCode, statusCode),
                        std::move(result)...);
                }));
    }

    ec2::ErrorCode toEc2ErrorCode(
        SystemError::ErrorCode systemErrorCode,
        nx::network::http::StatusCode::Value statusCode);

private:
    std::optional<std::chrono::milliseconds> m_requestTimeout;
    const nx::utils::Url m_baseRequestUrl;
    nx::network::ssl::AdapterFunc m_adapterFunc;
    std::optional<nx::network::http::Credentials> m_credentials;
    std::list<std::unique_ptr<nx::network::aio::BasicPollable>> m_activeClients;
    SystemError::ErrorCode m_prevRequestSysErrorCode = SystemError::noError;
    nx::network::http::StatusLine m_prevResponseHttpStatusLine;
    QString m_authenticationKey;
};
