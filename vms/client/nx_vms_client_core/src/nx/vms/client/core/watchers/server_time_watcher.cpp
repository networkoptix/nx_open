// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_time_watcher.h"

#include <QtCore/QTimeZone>

#include <api/model/time_reply.h>
#include <api/server_rest_connection.h>
#include <client_core/client_core_module.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/ini.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/resource/server.h>
#include <nx/vms/client/core/system_context.h>
#include <utils/common/synctime.h>

using namespace std::chrono;

namespace {

// Update offset once in 2 minutes.
constexpr int kServerTimeUpdatePeriodMs = 1000 * 60 * 2;

} // namespace

namespace nx::vms::client::core {

ServerTimeWatcher::ServerTimeWatcher(SystemContext* systemContext, QObject* parent):
    QObject(parent),
    SystemContextAware(systemContext)
{
    const auto resourcePool = this->resourcePool();

    connect(resourcePool, &QnResourcePool::resourceAdded, this,
        &ServerTimeWatcher::handleResourceAdded);
    connect(resourcePool, &QnResourcePool::resourceRemoved, this,
        &ServerTimeWatcher::handleResourceRemoved);

    /* We need to process only servers. */
    for (const auto& resource: resourcePool->servers())
        handleResourceAdded(resource);

    const auto updateTimeData =
        [this, resourcePool]()
        {
            const auto onlineServers = resourcePool->getAllServers(nx::vms::api::ResourceStatus::online);
            for (const auto& server: onlineServers)
                sendRequest(server);
        };

    connect(&m_timer, &QTimer::timeout, this, updateTimeData);
    m_timer.setInterval(kServerTimeUpdatePeriodMs);
    m_timer.setSingleShot(false);
    m_timer.start();

    connect(this, &ServerTimeWatcher::timeModeChanged,
        this, &ServerTimeWatcher::displayOffsetsChanged);
    connect(qnSyncTime, &QnSyncTime::timeChanged, this,
        &ServerTimeWatcher::displayOffsetsChanged);
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

    emit timeModeChanged();
}

qint64 ServerTimeWatcher::utcOffset(
    const QnMediaResourcePtr& resource,
    qint64 defaultValue)
{
    if (auto fileResource = resource.dynamicCast<QnAviResource>())
    {
        qint64 result = fileResource->timeZoneOffset();
        if (result != Qn::InvalidUtcOffset)
            NX_ASSERT(fileResource->hasFlags(Qn::utc), "Only utc resources should have offset.");

        return result == Qn::InvalidUtcOffset
            ? defaultValue
            : result;
    }

    if (auto server = resource->toResource()->getParentResource().dynamicCast<ServerResource>())
    {
        NX_ASSERT(resource->toResourcePtr()->hasFlags(Qn::utc),
            "Only utc resources should have offset.");
        return server->timeZoneInfo().utcOffset.count();
    }

    return defaultValue;
}

qint64 ServerTimeWatcher::displayOffset(const QnMediaResourcePtr& resource) const
{
    return timeMode() == clientTimeMode
        ? 0
        : localOffset(resource, 0);
}

QDateTime ServerTimeWatcher::serverTime(
    const ServerResourcePtr& server,
    qint64 msecsSinceEpoch)
{
    // Try to use actual server timezone when possible.
    if (NX_ASSERT(server) && ini().useNativeServerTimeZone)
    {
        QByteArray timezoneId = server->timeZoneInfo().timezoneId.toUtf8();
        const bool isTzAvailable = QTimeZone::isTimeZoneIdAvailable(timezoneId);
        QTimeZone tz(timezoneId);
        if (!timezoneId.isEmpty() && isTzAvailable && tz.isValid())
        {
            QDateTime result;
            result.setTimeZone(tz);
            result.setMSecsSinceEpoch(msecsSinceEpoch);
            NX_TRACE(NX_SCOPE_TAG, "Calculated time based on server timezone %1 is %2",
                server->timeZoneInfo().timezoneId, result);
            return result;
        }
        else
        {
            NX_VERBOSE(NX_SCOPE_TAG, "Timezone is not available, using default calculation.");
        }
    }

    // Intentionally fall-through to default calculation.
    {
        const auto utcOffset = duration_cast<seconds>(server->timeZoneInfo().utcOffset);

        QDateTime result;
        result.setTimeSpec(Qt::OffsetFromUTC);
        result.setOffsetFromUtc(utcOffset.count());
        result.setMSecsSinceEpoch(msecsSinceEpoch);
        NX_TRACE(NX_SCOPE_TAG, "Calculated time based on server utc offset %1 is %2",
            utcOffset, result);
        return result;
    }
}

QDateTime ServerTimeWatcher::displayTime(qint64 msecsSinceEpoch) const
{
    return displayTime(currentServer().dynamicCast<ServerResource>(), msecsSinceEpoch);
}

QDateTime ServerTimeWatcher::displayTime(const ServerResourcePtr& server,
    qint64 msecsSinceEpoch) const
{
    if (!server || timeMode() == clientTimeMode)
        return QDateTime::fromMSecsSinceEpoch(msecsSinceEpoch);

    return serverTime(server, msecsSinceEpoch);
}

qint64 ServerTimeWatcher::localOffset(
    const QnMediaResourcePtr& resource,
    qint64 defaultValue) const
{
    const qint64 utcOffsetMs = utcOffset(resource, Qn::InvalidUtcOffset);

    if (utcOffsetMs == Qn::InvalidUtcOffset)
        return defaultValue;

    auto localDateTime = QDateTime::currentDateTime();
    const auto utcDateTime = localDateTime.toUTC();
    localDateTime.setTimeSpec(Qt::UTC);

    return utcOffsetMs - utcDateTime.msecsTo(localDateTime);
}

void ServerTimeWatcher::sendRequest(const QnMediaServerResourcePtr& server)
{
    if (server->getStatus() != nx::vms::api::ResourceStatus::online)
        return;

    auto connection = this->connection();
    if (!connection)
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
            server->setTimeZoneInfo({result.timezoneId, milliseconds(result.timeZoneOffset)});
        });

    connectedServerApi()->getServerLocalTime(
        server->getId(),
        callback,
        this->thread());
}

void ServerTimeWatcher::handleResourceAdded(const QnResourcePtr& resource)
{
    const auto server = resource.dynamicCast<ServerResource>();
    if (!server)
        return;

    const auto updateServer = [this, server]{ sendRequest(server); };

    connect(server.get(), &QnMediaServerResource::apiUrlChanged, this, updateServer);
    connect(server.get(), &QnMediaServerResource::statusChanged, this, updateServer);
    updateServer();

    connect(server.get(), &ServerResource::timeZoneInfoChanged, this,
        &ServerTimeWatcher::displayOffsetsChanged);
}

void ServerTimeWatcher::handleResourceRemoved(const QnResourcePtr& resource)
{
    resource->disconnect(this);
}

} // namespace nx::vms::client::core
