// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_time_watcher.h"

#include <QtCore/QTimeZone>

#include <api/model/time_reply.h>
#include <api/server_rest_connection.h>
#include <client_core/client_core_module.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/ini.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/resource/server.h>
#include <nx/vms/client/core/system_context.h>

using namespace std::chrono;

namespace {

// Update offset once in 2 minutes.
constexpr minutes kServerTimeUpdatePeriod = 2min;

} // namespace

namespace nx::vms::client::core {

ServerTimeWatcher::ServerTimeWatcher(SystemContext* systemContext, QObject* parent):
    QObject(parent),
    SystemContextAware(systemContext)
{
    const auto resourcePool = this->resourcePool();

    connect(resourcePool, &QnResourcePool::resourcesAdded, this,
        &ServerTimeWatcher::handleResourcesAdded);
    connect(resourcePool, &QnResourcePool::resourcesRemoved, this,
        &ServerTimeWatcher::handleResourcesRemoved);

    handleResourcesAdded(resourcePool->servers());

    const auto updateTimeData =
        [this, resourcePool]()
        {
            const auto onlineServers = resourcePool->getAllServers(
                nx::vms::api::ResourceStatus::online);
            for (const auto& server: onlineServers)
                ensureServerTimeZoneIsValid(server.dynamicCast<ServerResource>());
        };

    connect(&m_timer, &QTimer::timeout, this, updateTimeData);
    m_timer.setInterval(kServerTimeUpdatePeriod);
    m_timer.setSingleShot(false);
    m_timer.start();
}

ServerTimeWatcher::TimeMode ServerTimeWatcher::timeMode() const
{
    return m_mode;
}

void ServerTimeWatcher::setTimeMode(TimeMode mode)
{
    if (m_mode == mode)
        return;

    m_mode = mode;

    emit timeZoneChanged();
}

QTimeZone ServerTimeWatcher::timeZone(const QnMediaResourcePtr& resource)
{
    if (auto fileResource = resource.dynamicCast<QnAviResource>())
        return fileResource->timeZone();

    if (auto server = resource->toResource()->getParentResource().dynamicCast<ServerResource>())
        return server->timeZone();

    return QTimeZone{QTimeZone::LocalTime};
}

QDateTime ServerTimeWatcher::displayTime(qint64 msecsSinceEpoch) const
{
    return displayTime(currentServer().dynamicCast<ServerResource>(), msecsSinceEpoch);
}

QDateTime ServerTimeWatcher::displayTime(const ServerResourcePtr& server,
    qint64 msecsSinceEpoch) const
{
    const QTimeZone tz = (server && timeMode() == serverTimeMode)
        ? server->timeZone()
        : QTimeZone::LocalTime;

    return QDateTime::fromMSecsSinceEpoch(msecsSinceEpoch, tz);
}

void ServerTimeWatcher::ensureServerTimeZoneIsValid(const ServerResourcePtr& server)
{
    // Timezone is already set and valid.
    if (!NX_ASSERT(server) || !server->timeZoneInfo().timeZoneId.isEmpty())
        return;

    auto api = connectedServerApi();
    if (!api)
        return;

    auto callback = nx::utils::guarded(this,
        [this, id = server->getId()](
            bool success, rest::Handle /*handle*/, nx::network::rest::JsonResult reply)
        {
            if (!success)
                return;

            const auto server = resourcePool()->getResourceById<ServerResource>(id);
            if (!server)
                return;

            const auto result = reply.deserialized<QnTimeReply>();
            server->overrideTimeZoneInfo({
                .timeZoneOffsetMs = milliseconds(result.timeZoneOffset),
                .timeZoneId = result.timezoneId
            });
        });

    api->getServerLocalTime(server->getId(), callback, this->thread());
}

void ServerTimeWatcher::handleResourcesAdded(const QnResourceList& resources)
{
    bool atLeastOneServerAdded = false;
    for (const auto& resource: resources)
    {
        const auto server = resource.objectCast<ServerResource>();
        if (!server || server->hasFlags(Qn::fake))
            return;

        atLeastOneServerAdded = true;
        if (server->getStatus() == nx::vms::api::ResourceStatus::online)
            ensureServerTimeZoneIsValid(server);

        connect(server.get(), &QnResource::statusChanged, this,
            [this](const QnResourcePtr& resource)
            {
                ensureServerTimeZoneIsValid(resource.objectCast<ServerResource>());
            });
        connect(server.get(), &ServerResource::timeZoneChanged, this,
            &ServerTimeWatcher::timeZoneChanged);
    }

    if (atLeastOneServerAdded)
        emit timeZoneChanged();
}

void ServerTimeWatcher::handleResourcesRemoved(const QnResourceList& resources)
{
    for (const auto& resource: resources)
        resource->disconnect(this);
}

} // namespace nx::vms::client::core
