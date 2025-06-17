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
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/core/system_finder/system_description.h>

using namespace std::chrono;
using namespace nx::vms::api;
using HttpHeaders = nx::network::http::HttpHeaders;

namespace nx::vms::client::core {

namespace {

static constexpr auto kRequestDataRetryTimeout = 30s;
static constexpr auto kCamerasRefreshPeriod = 10min;
static const nx::utils::SoftwareVersion kUserGroupsApiSupportedVersion(6, 0);

} // namespace

struct CloudCrossSystemContextDataLoader::Private
{
    CloudCrossSystemContextDataLoader* const q;
    CloudCrossSystemRequestScheduler* scheduler;
    rest::ServerConnectionPtr const connection;
    core::SystemDescriptionPtr description;
    const QString username;
    bool doRequestUser = true;
    bool isFirstTime = true;

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

    void requestUser(GroupedTaskQueue::PromisePtr promise)
    {
        NX_VERBOSE(this, "Requesting user");
        auto callback = nx::utils::guarded(q,
            [this, promise](bool success, ::rest::Handle, QByteArray data, HttpHeaders)
            {
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
                updateStatus();
            });

        connection->getRawResult(
            QString("/rest/v3/users/") + QUrl::toPercentEncoding(username),
            {},
            std::move(callback),
            q->thread());
    }

    void requestUserGroups(GroupedTaskQueue::PromisePtr promise)
    {
        NX_VERBOSE(this, "Requesting userGroups");
        auto callback = nx::utils::guarded(q,
            [this, promise](bool success, ::rest::Handle, QByteArray data, HttpHeaders)
            {
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
                updateStatus();
            });

        connection->getRawResult(
            QString("/rest/v3/userGroups/"),
            {},
            std::move(callback),
            q->thread());
    }

    void requestServers(GroupedTaskQueue::PromisePtr promise)
    {
        NX_VERBOSE(this, "Updating servers");
        auto callback = nx::utils::guarded(q,
            [this, promise](
                bool success,
                ::rest::Handle,
                ::rest::ErrorOrData<ServerInformationV1List> result)
            {
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
                updateStatus();
            });

        connection->getServersInfo(
            /*onlyFreshInfo*/ false,
            std::move(callback),
            q->thread());
    }

    void requestTaxonomyDescriptors(GroupedTaskQueue::PromisePtr promise)
    {
        NX_VERBOSE(this, "Updating servers taxonomy descriptors data");
        auto callback = nx::utils::guarded(q,
            [this, promise](bool success, ::rest::Handle, QByteArray data, HttpHeaders)
            {
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
                updateStatus();
            }
        );

        connection->getRawResult(
            QString("/rest/v2/servers?_with=id,parameters.%1")
                .arg(nx::analytics::kDescriptorsProperty),
            {},
            std::move(callback),
            q);
    }

    void requestServerFootageData(GroupedTaskQueue::PromisePtr promise)
    {
        NX_VERBOSE(this, "Updating server footage data");
        auto callback = nx::utils::guarded(q,
            [this, promise](bool success, ::rest::Handle, QByteArray data, HttpHeaders)
            {
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
                updateStatus();
            });

        connection->getRawResult(
            "/ec2/getCameraHistoryItems",
            {},
            std::move(callback),
            q->thread());
    }

    void requestSystemSettings(GroupedTaskQueue::PromisePtr promise)
    {
        NX_VERBOSE(this, "Updating system settings");
        auto callback = nx::utils::guarded(q,
            [this, promise](bool success, ::rest::Handle, QByteArray data, HttpHeaders)
            {
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
                updateStatus();
            }
        );

        connection->getRawResult(
            "/rest/v2/system/settings",
            {},
            std::move(callback),
            q->thread());
    }

    void requestLicenses(GroupedTaskQueue::PromisePtr promise)
    {
        NX_VERBOSE(this, "Updating licenses");
        auto callback = nx::utils::guarded(q,
            [this, promise](bool success, ::rest::Handle, QByteArray data, HttpHeaders)
            {
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
                updateStatus();
            }
        );

        connection->getRawResult(
            "/rest/v2/licenses",
            {},
            std::move(callback),
            q->thread());
    }

    void requestCameras(GroupedTaskQueue::PromisePtr promise)
    {
        NX_VERBOSE(this, "Updating cameras");
        auto callback = nx::utils::guarded(q,
            [this, promise](bool success, ::rest::Handle, QByteArray data, HttpHeaders)
            {
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
                cameras = result;
                camerasRefreshTimer.restart();

                if (!isFirstTime)
                    emit q->camerasUpdated();

                updateStatus();
            });

        connection->getRawResult(
            "/ec2/getCamerasEx",
            {},
            std::move(callback),
            q->thread());
    }

    void requestData()
    {
        if (scheduler->hasTasks(description->cloudId()))
            return;

        auto addTask =
            [this](auto function, const QString& id)
            {
                scheduler->add(q,
                    description->cloudId(),
                    function,
                    nx::format("%1_%2", description->name(), id));
            };

        if (!user && doRequestUser)
            addTask([this](auto promise) { requestUser(promise); }, "user");

        if (!userGroups && description->version() >= kUserGroupsApiSupportedVersion)
            addTask([this](auto promise) { requestUserGroups(promise); }, "groups");

        if (!servers)
            addTask([this](auto promise) { requestServers(promise); }, "servers");

        if (!serversTaxonomyDescriptors)
            addTask([this](auto promise) { requestTaxonomyDescriptors(promise); }, "taxonomy");

        if (!serverFootageData)
            addTask([this](auto promise) { requestServerFootageData(promise); }, "footage");

        if (!systemSettings)
            addTask([this](auto promise) { requestSystemSettings(promise); }, "settings");

        const bool updateLicenses = !licenses && user
            && user->permissions.testFlag(nx::vms::api::GlobalPermission::powerUser);

        if (updateLicenses)
            addTask([this](auto promise) { requestLicenses(promise); }, "licenses");

        const bool updateCameras = !cameras
            || camerasRefreshTimer.hasExpired(milliseconds(kCamerasRefreshPeriod).count());

        if (updateCameras)
            addTask([this](auto promise) { requestCameras(promise); }, "cameras");
    }

    bool isDataNeeded()
    {
        return (!user && doRequestUser)
            || !servers
            || !serversTaxonomyDescriptors
            || !serverFootageData
            || !systemSettings
            || (!licenses
                && user && user->permissions.testFlag(nx::vms::api::GlobalPermission::powerUser))
            || (!userGroups && description->version() >= kUserGroupsApiSupportedVersion)
            || !cameras
            || camerasRefreshTimer.hasExpired(milliseconds(kCamerasRefreshPeriod).count());
    }

    void updateStatus()
    {
        if (!isDataNeeded() && isFirstTime)
        {
            emit q->ready();
            isFirstTime = false;
        }
    }
};

CloudCrossSystemContextDataLoader::CloudCrossSystemContextDataLoader(
    rest::ServerConnectionPtr connection,
    core::SystemDescriptionPtr description,
    const QString& username,
    QObject* parent)
    :
    QObject(parent),
    d(new Private{
        .q = this,
        .scheduler = appContext()->cloudCrossSystemManager()->scheduler(),
        .connection = connection,
        .description = description,
        .username = username,
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
