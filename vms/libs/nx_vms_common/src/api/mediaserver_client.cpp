// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mediaserver_client.h"

#include <nx/network/http/http_client.h>
#include <nx/network/rest/params.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/type_utils.h>
#include <nx/vms/api/data/login.h>

#include <nx_ec/ec_api_common.h>

MediaServerClient::MediaServerClient(
    const nx::utils::Url& baseRequestUrl, nx::network::ssl::AdapterFunc adapterFunc):
    m_baseRequestUrl(baseRequestUrl), m_adapterFunc(std::move(adapterFunc))
{
}

MediaServerClient::~MediaServerClient()
{
    pleaseStopSync();
}

void MediaServerClient::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    for (auto& client: m_activeClients)
        client->bindToAioThread(aioThread);
}

void MediaServerClient::setCredentials(const nx::network::http::Credentials& userCredentials)
{
    m_credentials = userCredentials;
}

std::optional<nx::network::http::Credentials> MediaServerClient::getCredentials() const
{
    return m_credentials;
}

void MediaServerClient::login(const std::string& username, const std::string& password)
{
    m_credentials.reset();
    if (!NX_ASSERT(m_baseRequestUrl.scheme() == nx::network::http::kSecureUrlSchemeName))
        return;
    nx::network::http::HttpClient client(m_adapterFunc);
    if (m_requestTimeout)
        client.setTimeouts({*m_requestTimeout, *m_requestTimeout, *m_requestTimeout});
    auto baseUrl = m_baseRequestUrl;
    baseUrl.setPath("/rest/v1/login/sessions");
    const nx::vms::api::LoginSessionRequest data{
        QString::fromStdString(username), QString::fromStdString(password)};
    if (client.doPost(baseUrl, "application/json", QJson::serialized(data)))
    {
        auto body = client.fetchEntireMessageBody();
        if (body
            && client.response()
            && client.response()->statusLine.statusCode == nx::network::http::StatusCode::ok)
        {
            m_credentials = nx::network::http::BearerAuthToken(
                QJson::deserialized<nx::vms::api::LoginSession>(*body).token);
        }
    }
}

void MediaServerClient::mergeSystems(
    const nx::vms::api::SystemMergeData& request,
    std::function<void(SystemError::ErrorCode, nx::network::http::StatusCode::Value, QByteArray)>
        completionHandler)
{
    performPostRequest("rest/v1/system/merge", request, std::move(completionHandler));
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

void MediaServerClient::getModuleInformation(
    std::function<void(nx::network::rest::JsonResult, nx::vms::api::ModuleInformation)>
        completionHandler)
{
    performApiRequest("api/moduleInformation", std::move(completionHandler));
}

nx::network::rest::JsonResult MediaServerClient::getModuleInformation(
    nx::vms::api::ModuleInformation* moduleInformation)
{
    using GetModuleInformationAsyncFuncPointer = void (MediaServerClient::*)(
        std::function<void(nx::network::rest::JsonResult, nx::vms::api::ModuleInformation)>);

    return syncCallWrapper(
        this,
        static_cast<GetModuleInformationAsyncFuncPointer>(
            &MediaServerClient::getModuleInformation),
        moduleInformation);
}

void MediaServerClient::deprecatedMergeSystems(
    const MergeSystemData& request,
    std::function<void(nx::network::rest::JsonResult)> completionHandler)
{
    performApiRequest("api/mergeSystems", request, std::move(completionHandler));
}

//-------------------------------------------------------------------------------------------------
// /ec2/ requests

void MediaServerClient::ec2GetUsers(
    std::function<void(ec2::ErrorCode, nx::vms::api::UserDataList)> completionHandler)
{
    performAsyncEc2Call("ec2/getUsers", std::move(completionHandler));
}

ec2::ErrorCode MediaServerClient::ec2GetUsers(nx::vms::api::UserDataList* result)
{
    using Ec2GetUsersAsyncFuncPointer =
        void(MediaServerClient::*)(
            std::function<void(ec2::ErrorCode, nx::vms::api::UserDataList)>);

    return syncCallWrapper(
        this,
        static_cast<Ec2GetUsersAsyncFuncPointer>(&MediaServerClient::ec2GetUsers),
        result);
}

void MediaServerClient::ec2SaveUser(
    const nx::vms::api::UserData& request,
    std::function<void(ec2::ErrorCode)> completionHandler)
{
    performAsyncEc2Call("ec2/saveUser", request, std::move(completionHandler));
}

ec2::ErrorCode MediaServerClient::ec2SaveUser(const nx::vms::api::UserData& request)
{
    using Ec2SaveUserAsyncFuncPointer =
        void(MediaServerClient::*)(
            const nx::vms::api::UserData& request,
            std::function<void(ec2::ErrorCode)>);

    return syncCallWrapper(
        this,
        static_cast<Ec2SaveUserAsyncFuncPointer>(&MediaServerClient::ec2SaveUser),
        request);
}

void MediaServerClient::ec2GetMediaServersEx(
    std::function<void(ec2::ErrorCode, nx::vms::api::MediaServerDataExList)> completionHandler)
{
    performAsyncEc2Call("ec2/getMediaServersEx", std::move(completionHandler));
}

ec2::ErrorCode MediaServerClient::ec2GetMediaServersEx(
    nx::vms::api::MediaServerDataExList* result)
{
    using Ec2GetMediaServersExAsyncFuncPointer =
        void(MediaServerClient::*)(
            std::function<void(ec2::ErrorCode, nx::vms::api::MediaServerDataExList)>);

    return syncCallWrapper(
        this,
        static_cast<Ec2GetMediaServersExAsyncFuncPointer>(
            &MediaServerClient::ec2GetMediaServersEx),
        result);
}

void MediaServerClient::ec2SaveMediaServer(
    const nx::vms::api::MediaServerData& request,
    std::function<void(ec2::ErrorCode)> completionHandler)
{
    performAsyncEc2Call("ec2/saveMediaServer", request, std::move(completionHandler));
}

ec2::ErrorCode MediaServerClient::ec2SaveMediaServer(
    const nx::vms::api::MediaServerData& request)
{
    using Ec2SaveMediaServerAsyncFuncPointer =
        void(MediaServerClient::*)(
            const nx::vms::api::MediaServerData& request,
            std::function<void(ec2::ErrorCode)>);

    return syncCallWrapper(
        this,
        static_cast<Ec2SaveMediaServerAsyncFuncPointer>(
            &MediaServerClient::ec2SaveMediaServer),
        request);
}

void MediaServerClient::ec2GetStorages(
    const std::optional<QnUuid>& serverId,
    std::function<void(ec2::ErrorCode, nx::vms::api::StorageDataList)> completionHandler)
{
    std::string requestPath;
    if (serverId)
    {
        requestPath = nx::format("ec2/getStorages?id=%1")
            .arg(serverId->toSimpleStdString()).toStdString();
    }
    else
    {
        requestPath = "ec2/getStorages";
    }

    performAsyncEc2Call(requestPath, std::move(completionHandler));
}

ec2::ErrorCode MediaServerClient::ec2GetStorages(
    const std::optional<QnUuid>& serverId,
    nx::vms::api::StorageDataList* result)
{
    using Ec2GetStoragesAsyncFuncPointer =
        void(MediaServerClient::*)(
            const std::optional<QnUuid>& serverId,
            std::function<void(ec2::ErrorCode, nx::vms::api::StorageDataList)>);

    return syncCallWrapper(
        this,
        static_cast<Ec2GetStoragesAsyncFuncPointer>(
            &MediaServerClient::ec2GetStorages),
        serverId,
        result);
}

void MediaServerClient::ec2GetSettings(
    std::function<void(ec2::ErrorCode, nx::vms::api::ResourceParamDataList)> completionHandler)
{
    performAsyncEc2Call("ec2/getSettings", std::move(completionHandler));
}

ec2::ErrorCode MediaServerClient::ec2GetSettings(nx::vms::api::ResourceParamDataList* result)
{
    using Ec2GetSettingsAsyncFuncPointer =
        void(MediaServerClient::*)(
            std::function<void(ec2::ErrorCode, nx::vms::api::ResourceParamDataList)>);

    return syncCallWrapper(
        this,
        static_cast<Ec2GetSettingsAsyncFuncPointer>(&MediaServerClient::ec2GetSettings),
        result);
}

void MediaServerClient::ec2SetResourceParams(
    const nx::vms::api::ResourceParamWithRefDataList& inputData,
    std::function<void(ec2::ErrorCode)> completionHandler)
{
    performAsyncEc2Call("ec2/setResourceParams", inputData, std::move(completionHandler));
}

ec2::ErrorCode MediaServerClient::ec2SetResourceParams(
    const nx::vms::api::ResourceParamWithRefDataList& request)
{
    using Ec2SetResourceParamsAsyncFuncPointer =
        void(MediaServerClient::*)(
            const nx::vms::api::ResourceParamWithRefDataList&,
            std::function<void(ec2::ErrorCode)>);

    return syncCallWrapper(
        this,
        static_cast<Ec2SetResourceParamsAsyncFuncPointer>(
            &MediaServerClient::ec2SetResourceParams),
        request);
}

void MediaServerClient::ec2GetResourceParams(
    const QnUuid& resourceId,
    std::function<void(ec2::ErrorCode, nx::vms::api::ResourceParamDataList)> completionHandler)
{
    performAsyncEc2Call(
        nx::format("ec2/getResourceParams?id=%1")
            .arg(resourceId.toSimpleString().toStdString()).toStdString(),
        std::move(completionHandler));
}

ec2::ErrorCode MediaServerClient::ec2GetResourceParams(
    const QnUuid& resourceId,
    nx::vms::api::ResourceParamDataList* result)
{
    using Ec2GetResourceParamsAsyncFuncPointer =
        void(MediaServerClient::*)(
            const QnUuid&,
            std::function<void(ec2::ErrorCode, nx::vms::api::ResourceParamDataList)>);

    return syncCallWrapper(
        this,
        static_cast<Ec2GetResourceParamsAsyncFuncPointer>(
            &MediaServerClient::ec2GetResourceParams),
        resourceId,
        result);
}

void MediaServerClient::ec2GetSystemMergeHistory(std::function<
    void(ec2::ErrorCode, nx::vms::api::SystemMergeHistoryRecordList)> completionHandler)
{
    performAsyncEc2Call("ec2/getSystemMergeHistory", std::move(completionHandler));
}

ec2::ErrorCode MediaServerClient::ec2GetSystemMergeHistory(
    nx::vms::api::SystemMergeHistoryRecordList* result)
{
    using Ec2GetSystemMergeHistoryAsyncFuncPointer =
        void(MediaServerClient::*)(
            std::function<void(ec2::ErrorCode, nx::vms::api::SystemMergeHistoryRecordList)>);

    return syncCallWrapper(
        this,
        static_cast<Ec2GetSystemMergeHistoryAsyncFuncPointer>(
            &MediaServerClient::ec2GetSystemMergeHistory),
        result);
}

void MediaServerClient::ec2AnalyticsLookupObjectTracks(
    const nx::analytics::db::Filter& request,
    std::function<void(ec2::ErrorCode, nx::analytics::db::LookupResult)> completionHandler)
{
    QString requestPath(lit("ec2/analyticsLookupObjectTracks"));

    nx::network::rest::Params queryParams;
    nx::analytics::db::serializeToParams(request, &queryParams);
    if (!queryParams.isEmpty())
    {
        // We create request path here. Not an URL. So, no need to encode it.
        requestPath += lit("?") + queryParams.toUrlQuery().toString(QUrl::FullyDecoded);
    }

    performAsyncEc2Call(
        requestPath.toStdString(),
        std::move(completionHandler));
}

ec2::ErrorCode MediaServerClient::ec2AnalyticsLookupObjectTracks(
    const nx::analytics::db::Filter& request,
    nx::analytics::db::LookupResult* result)
{
    using AsyncFuncPointer =
        void(MediaServerClient::*)(
            const nx::analytics::db::Filter&,
            std::function<void(ec2::ErrorCode, nx::analytics::db::LookupResult)>);

    return syncCallWrapper(
        this,
        static_cast<AsyncFuncPointer>(&MediaServerClient::ec2AnalyticsLookupObjectTracks),
        request,
        result);
}

void MediaServerClient::ec2DumpDatabase(
    std::function<void(ec2::ErrorCode, nx::vms::api::DatabaseDumpData)> completionHandler)
{
    performAsyncEc2Call(
        "ec2/dumpDatabase",
        std::move(completionHandler));
}

ec2::ErrorCode MediaServerClient::ec2DumpDatabase(nx::vms::api::DatabaseDumpData* dump)
{
    using Ec2DumpDatabaseAsyncFuncPointer =
        void(MediaServerClient::*)(
            std::function<void(ec2::ErrorCode, nx::vms::api::DatabaseDumpData)>);

    return syncCallWrapper(
        this,
        static_cast<Ec2DumpDatabaseAsyncFuncPointer>(&MediaServerClient::ec2DumpDatabase),
        dump);
}

void MediaServerClient::ec2RestoreDatabase(
    const nx::vms::api::DatabaseDumpData& dump,
    std::function<void(ec2::ErrorCode)> completionHandler)
{
    performAsyncEc2Call("ec2/restoreDatabase", std::move(dump), std::move(completionHandler));
}

ec2::ErrorCode MediaServerClient::ec2RestoreDatabase(
    const nx::vms::api::DatabaseDumpData& dump)
{
    using Ec2RestoreDatabaseAsyncFuncPointer =
        void(MediaServerClient::*)(
            const nx::vms::api::DatabaseDumpData&,
            std::function<void(ec2::ErrorCode)>);

    return syncCallWrapper(
        this,
        static_cast<Ec2RestoreDatabaseAsyncFuncPointer>(&MediaServerClient::ec2RestoreDatabase),
        dump);
}

SystemError::ErrorCode MediaServerClient::prevRequestSysErrorCode() const
{
    return m_prevRequestSysErrorCode;
}

nx::network::http::StatusLine MediaServerClient::lastResponseHttpStatusLine() const
{
    return m_prevResponseHttpStatusLine;
}

void MediaServerClient::stopWhileInAioThread()
{
    m_activeClients.clear();
}

//-------------------------------------------------------------------------------------------------
// Utilities

ec2::ErrorCode MediaServerClient::toEc2ErrorCode(
    SystemError::ErrorCode systemErrorCode,
    nx::network::http::StatusCode::Value statusCode)
{
    if (systemErrorCode != SystemError::noError)
        return ec2::ErrorCode::ioError;
    if (!nx::network::http::StatusCode::isSuccessCode(statusCode))
        return ec2::ErrorCode::forbidden;

    return ec2::ErrorCode::ok;
}
