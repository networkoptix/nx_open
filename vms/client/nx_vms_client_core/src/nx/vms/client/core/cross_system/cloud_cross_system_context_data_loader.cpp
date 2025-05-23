// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_cross_system_context_data_loader.h"

#include <chrono>
#include <optional>

#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>

#include <api/server_rest_connection.h>
#include <nx/analytics/properties.h>
#include <nx/reflect/json.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/json/qt_containers_reflect.h>
#include <nx/utils/log/log.h>

using namespace std::chrono;
using namespace nx::vms::api;

namespace nx::vms::client::core {

namespace {

static constexpr auto kRequestDataRetryTimeout = 30s;
static constexpr auto kCamerasRefreshPeriod = 10min;
static const nx::utils::SoftwareVersion kUserGroupsApiSupportedVersion(6, 0);

} // namespace

struct CloudCrossSystemContextDataLoader::Private
{
    CloudCrossSystemContextDataLoader* const q;
    rest::ServerConnectionPtr const connection;
    const QString username;
    const nx::utils::SoftwareVersion version;
    bool doRequestUser = true;

    std::optional<rest::Handle> currentRequest;
    std::optional<UserModel> user;
    std::optional<UserGroupDataList> userGroups;
    std::optional<ServerInformationV1List> servers;
    std::optional<ServerModelV1List> serversTaxonomyDescriptors;
    std::optional<ServerFootageDataList> serverFootageData;
    std::optional<SystemSettings> systemSettings;
    std::optional<LicenseDataList> licenses;
    std::optional<CameraDataExList> cameras;
    QElapsedTimer camerasRefreshTimer;

    /**
    * IMPORTANT! Minimum supported cross-system version is 5.1. Do not use REST API calls with
    * version higher than v2 unless you are implementing versioning.
    */

    rest::Handle requestUser()
    {
        NX_VERBOSE(this, "Requesting user");
        auto callback = nx::utils::guarded(q,
            [this](
                bool success,
                ::rest::Handle requestId,
                QByteArray data,
                nx::network::http::HttpHeaders /*headers*/)
            {
                NX_ASSERT(currentRequest && *currentRequest == requestId);
                currentRequest = std::nullopt;

                if (!success)
                {
                     NX_WARNING(this, "User request failed");
                     return;
                }

                UserModel result;
                if (!QJson::deserialize(data, &result))
                {
                    NX_WARNING(this, "User reply cannot be deserialized");
                    return;
                }

                NX_VERBOSE(this, "User loaded successfully, username: %1", result.name);
                user = result;
                requestData();
            });

        return connection->getRawResult(
            QString("/rest/v3/users/") + QUrl::toPercentEncoding(username),
            {},
            std::move(callback),
            q->thread());
    }

    rest::Handle requestUserGroups()
    {
        NX_VERBOSE(this, "Requesting userGroups");
        auto callback = nx::utils::guarded(q,
            [this](
                bool success,
                ::rest::Handle requestId,
                QByteArray data,
                nx::network::http::HttpHeaders /*headers*/)
            {
                NX_ASSERT(currentRequest && *currentRequest == requestId);
                currentRequest = std::nullopt;

                if (!success)
                {
                    NX_WARNING(this, "UserGroups request failed");
                    return;
                }

                UserGroupDataList result;
                if (!QJson::deserialize(data, &result))
                {
                    NX_WARNING(this, "UserGroups reply cannot be deserialized");
                    return;
                }

                NX_VERBOSE(this, "UserGroups loaded successfully");
                userGroups = result;
                requestData();
            });

        return connection->getRawResult(
            QString("/rest/v3/userGroups/"),
            {},
            std::move(callback),
            q->thread());
    }

    rest::Handle requestServers()
    {
        NX_VERBOSE(this, "Updating servers");
        auto callback = nx::utils::guarded(q,
            [this](
                bool success,
                ::rest::Handle requestId,
                ::rest::ErrorOrData<ServerInformationV1List> result)
            {
                NX_ASSERT(currentRequest && *currentRequest == requestId);
                currentRequest = std::nullopt;

                if (!success)
                {
                     NX_WARNING(this, "Servers request failed");
                     return;
                }

                if (!result)
                {
                    NX_WARNING(this, "Servers request failed: %1", QJson::serialized(result.error()));
                    return;
                }

                NX_VERBOSE(this, "Received %1 servers", result->size());
                servers = *result;
                requestData();
            });

        return connection->getServersInfo(
            /*onlyFreshInfo*/ false,
            std::move(callback),
            q->thread());
    }

    rest::Handle requestServersTaxonomyDescriptors()
    {
        NX_VERBOSE(this, "Updating servers taxonomy descriptors data");
        auto callback =
            [this](
                bool success,
                ::rest::Handle requestId,
                QByteArray data,
                nx::network::http::HttpHeaders /*headers*/)
            {
                NX_ASSERT(currentRequest && *currentRequest == requestId);
                currentRequest = std::nullopt;

                if (!success)
                {
                     NX_WARNING(this, "Servers taxonomy descriptors request failed");
                     return;
                }

                auto [result, deserializationResult] =
                    reflect::json::deserialize<ServerModelV1List>(nx::toBufferView(data));
                if (!deserializationResult.success)
                {
                    NX_WARNING(
                        this,
                        "Servers taxonomy descriptors cannot be deserialized, error: %1",
                        deserializationResult.errorDescription);
                    return;
                }

                NX_VERBOSE(this, "Servers taxonomy descriptors received");

                serversTaxonomyDescriptors = result;
                requestData();
            };

        return connection->getRawResult(
            QString("/rest/v2/servers?_with=id,parameters.%1")
                .arg(nx::analytics::kDescriptorsProperty),
            {},
            callback,
            q);
    }

    rest::Handle requestServerFootageData()
    {
        NX_VERBOSE(this, "Updating server footage data");
        auto callback = nx::utils::guarded(q,
            [this](
                bool success,
                ::rest::Handle requestId,
                QByteArray data,
                nx::network::http::HttpHeaders /*headers*/)
            {
                NX_ASSERT(currentRequest && *currentRequest == requestId);
                currentRequest = std::nullopt;

                if (!success)
                {
                     NX_WARNING(this, "Server footage request failed");
                     return;
                }

                ServerFootageDataList result;
                if (!QJson::deserialize(data, &result))
                {
                    NX_WARNING(this, "Server footage list cannot be deserialized");
                    return;
                }

                NX_VERBOSE(this, "Received %1 server footage entries", result.size());
                serverFootageData = result;
                requestData();
            });

        return connection->getRawResult(
            "/ec2/getCameraHistoryItems",
            {},
            callback,
            q->thread());
    }

    rest::Handle requestSystemSettings()
    {
        NX_VERBOSE(this, "Updating system settings");
        auto callback = nx::utils::guarded(q,
            [this](
                bool success,
                ::rest::Handle requestId,
                QByteArray data,
                nx::network::http::HttpHeaders /*headers*/)
            {
                NX_ASSERT(currentRequest && *currentRequest == requestId);
                currentRequest = std::nullopt;

                if (!success)
                {
                     NX_WARNING(this, "System settings request failed");
                     return;
                }

                auto [result, deserializationResult] =
                    reflect::json::deserialize<SystemSettings>(data.data());
                if (!deserializationResult.success)
                {
                    NX_WARNING(
                        this,
                        "System settings cannot be deserialized, error: %1",
                        deserializationResult.errorDescription);
                    return;
                }

                NX_VERBOSE(this, "System settings received");

                systemSettings = result;
                requestData();
            }
        );

        return connection->getRawResult(
            "/rest/v2/system/settings",
            {},
            callback,
            q->thread());
    }

    rest::Handle requestLicenses()
    {
        NX_VERBOSE(this, "Updating licenses");
        auto callback = nx::utils::guarded(q,
            [this](
                bool success,
                ::rest::Handle requestId,
                QByteArray data,
                nx::network::http::HttpHeaders /*headers*/)
            {
                NX_ASSERT(currentRequest && *currentRequest == requestId);
                currentRequest = std::nullopt;

                if (!success)
                {
                     NX_WARNING(this, "Licenses request failed");
                     return;
                }

                auto [result, deserializationResult] =
                    reflect::json::deserialize<LicenseDataList>(data.data());
                if (!deserializationResult.success)
                {
                    NX_WARNING(
                        this,
                        "Licenses cannot be deserialized, error: %1",
                        deserializationResult.errorDescription);
                    return;
                }

                NX_VERBOSE(this, "Licenses received");

                licenses = result;
                requestData();
            }
        );

        return connection->getRawResult(
            "/rest/v2/licenses",
            {},
            callback,
            q->thread());
    }

    rest::Handle requestCameras()
    {
        NX_VERBOSE(this, "Updating cameras");
        auto callback = nx::utils::guarded(q,
            [this](
                bool success,
                ::rest::Handle requestId,
                QByteArray data,
                nx::network::http::HttpHeaders /*headers*/)
            {
                NX_ASSERT(currentRequest && *currentRequest == requestId);
                currentRequest = std::nullopt;

                if (!success)
                {
                     NX_WARNING(this, "Cameras request failed");
                     return;
                }

                CameraDataExList result;
                if (!QJson::deserialize(data, &result))
                {
                    NX_WARNING(this, "Cameras list cannot be deserialized");
                    return;
                }

                NX_VERBOSE(this, "Received %1 cameras", result.size());
                bool firstTime = !cameras;
                cameras = result;
                camerasRefreshTimer.restart();
                if (firstTime)
                    emit q->ready();
                else
                    emit q->camerasUpdated();
            });

        return connection->getRawResult(
            "/ec2/getCamerasEx",
            {},
            callback,
            q->thread());
    }

    void requestData()
    {
        if (currentRequest)
        {
            NX_VERBOSE(this, "Request %1 is in progress", *currentRequest);
        }
        else if (!user && doRequestUser)
        {
            currentRequest = requestUser();
        }
        else if (!userGroups && version >= kUserGroupsApiSupportedVersion)
        {
            currentRequest = requestUserGroups();
        }
        else if (!servers)
        {
            currentRequest = requestServers();
        }
        else if (!serversTaxonomyDescriptors)
        {
            currentRequest = requestServersTaxonomyDescriptors();
        }
        else if (!serverFootageData)
        {
            currentRequest = requestServerFootageData();
        }
        else if (!systemSettings)
        {
            currentRequest = requestSystemSettings();
        }
        else if (!licenses && user
            && user->permissions.testFlag(nx::vms::api::GlobalPermission::powerUser))
        {
            currentRequest = requestLicenses();
        }
        else if (!cameras
            || camerasRefreshTimer.hasExpired(milliseconds(kCamerasRefreshPeriod).count()))
        {
            currentRequest = requestCameras();
        }
    }
};

CloudCrossSystemContextDataLoader::CloudCrossSystemContextDataLoader(
    rest::ServerConnectionPtr connection,
    const QString& username,
    nx::utils::SoftwareVersion version,
    QObject* parent)
    :
    QObject(parent),
    d(new Private{
        .q = this,
        .connection = connection,
        .username = username,
        .version = version
        })
{
}

CloudCrossSystemContextDataLoader::~CloudCrossSystemContextDataLoader()
{
}

void CloudCrossSystemContextDataLoader::start(bool requestUser)
{
    d->doRequestUser = requestUser;

    auto requestDataTimer = new QTimer(this);
    requestDataTimer->setInterval(kRequestDataRetryTimeout);
    requestDataTimer->callOnTimeout([this]() { requestData(); });
    requestDataTimer->start();
    requestData();
}

void CloudCrossSystemContextDataLoader::requestData()
{
    d->requestData();
}

UserModel CloudCrossSystemContextDataLoader::user() const
{
    return d->user.value_or(UserModel());
}

std::optional<nx::vms::api::UserGroupDataList>
    CloudCrossSystemContextDataLoader::userGroups() const
{
    return d->userGroups;
}

ServerInformationV1List CloudCrossSystemContextDataLoader::servers() const
{
    return d->servers.value_or(ServerInformationV1List());
}

ServerModelV1List CloudCrossSystemContextDataLoader::serverTaxonomyDescriptions() const
{
    return d->serversTaxonomyDescriptors.value_or(ServerModelV1List());
}

ServerFootageDataList CloudCrossSystemContextDataLoader::serverFootageData() const
{
    return d->serverFootageData.value_or(ServerFootageDataList());
}

CameraDataExList CloudCrossSystemContextDataLoader::cameras() const
{
    return d->cameras.value_or(CameraDataExList());
}

SystemSettings CloudCrossSystemContextDataLoader::systemSettings() const
{
    return d->systemSettings.value_or(SystemSettings());
}

LicenseDataList CloudCrossSystemContextDataLoader::licenses() const
{
    return d->licenses.value_or(LicenseDataList());
}

} // namespace nx::vms::client::core
