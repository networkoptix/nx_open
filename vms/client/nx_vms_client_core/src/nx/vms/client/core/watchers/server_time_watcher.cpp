// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_time_watcher.h"

#include <api/model/time_reply.h>
#include <api/server_rest_connection.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/fake_media_server.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <utils/common/synctime.h>

namespace {

// Update offset once in 2 minutes.
constexpr int kServerTimeUpdatePeriodMs = 1000 * 60 * 2;

} // namespace

namespace nx::vms::client::core {

ServerTimeWatcher::ServerTimeWatcher(QObject* parent):
    base_type(parent)
{
    const auto resourcePool = qnClientCoreModule->commonModule()->resourcePool();

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

    if (auto virtualCamera = resource.dynamicCast<QnVirtualCameraResource>();
        virtualCamera && virtualCamera->virtualCameraIgnoreTimeZone())
    {
        return 0;
    }

    if (auto server = resource->toResource()->getParentResource()
        .dynamicCast<QnMediaServerResource>())
    {
        NX_ASSERT(resource->toResourcePtr()->hasFlags(Qn::utc),
            "Only utc resources should have offset.");
        return server->utcOffset(defaultValue);
    }

    return defaultValue;
}

qint64 ServerTimeWatcher::displayOffset(const QnMediaResourcePtr& resource) const
{
    return timeMode() == clientTimeMode ? 0 : localOffset(resource, 0);
}

QDateTime ServerTimeWatcher::serverTime(
    const QnMediaServerResourcePtr& server,
    qint64 msecsSinceEpoch)
{
    const auto utcOffsetMs = server
        ? server->utcOffset()
        : Qn::InvalidUtcOffset;

    QDateTime result;
    if (utcOffsetMs != Qn::InvalidUtcOffset)
    {
        result.setTimeSpec(Qt::OffsetFromUTC);
        result.setUtcOffset(utcOffsetMs / 1000);
        result.setMSecsSinceEpoch(msecsSinceEpoch);
    }
    else
    {
        result.setTimeSpec(Qt::UTC);
        result.setMSecsSinceEpoch(msecsSinceEpoch);
    }

    return result;
}

QDateTime ServerTimeWatcher::displayTime(qint64 msecsSinceEpoch) const
{
    return displayTime(qnClientCoreModule->commonModule()->currentServer(), msecsSinceEpoch);
}

QDateTime ServerTimeWatcher::displayTime(
    const QnMediaServerResourcePtr& server, qint64 msecsSinceEpoch) const
{
    if (timeMode() == clientTimeMode)
        return QDateTime::fromMSecsSinceEpoch(msecsSinceEpoch);

    return serverTime(server, msecsSinceEpoch);
}

qint64 ServerTimeWatcher::localOffset(
    const QnMediaResourcePtr& resource,
    qint64 defaultValue) const
{
    const qint64 utcOffsetMs = utcOffset(resource, Qn::InvalidUtcOffset);

    if(utcOffsetMs == Qn::InvalidUtcOffset)
        return defaultValue;

    auto localDateTime = QDateTime::currentDateTime();
    const auto utcDateTime = localDateTime.toUTC();
    localDateTime.setTimeSpec(Qt::UTC);

    return utcOffsetMs - utcDateTime.msecsTo(localDateTime);
}

void ServerTimeWatcher::sendRequest(const QnMediaServerResourcePtr& server)
{
    if (server->getStatus() != nx::vms::api::ResourceStatus::online
        || server.dynamicCast<QnFakeMediaServerResource>())
    {
        return;
    }

    if (!connection())
        return;

    auto callback = nx::utils::guarded(this,
        [this, id = server->getId()](
            bool success, rest::Handle /*handle*/, nx::network::rest::JsonResult reply)
        {
            if (!success)
                return;

            const auto server = qnClientCoreModule->commonModule()
                ->resourcePool()->getResourceById<QnMediaServerResource>(id);
            if (!server)
                return;

            const auto result = reply.deserialized<QnTimeReply>();
            const auto offset = result.timeZoneOffset;
            if (server->utcOffset() == offset)
                return;

            server->setUtcOffset(offset);
            emit displayOffsetsChanged();
        });

    connectedServerApi()->getServerLocalTime(server->getId(), callback, this->thread());
}

void ServerTimeWatcher::handleResourceAdded(const QnResourcePtr& resource)
{
    const auto server = resource.dynamicCast<QnMediaServerResource>();
    if (!server || server.dynamicCast<QnFakeMediaServerResource>())
        return;

    const auto updateServer = [this, server]{ sendRequest(server); };

    connect(server.get(), &QnMediaServerResource::apiUrlChanged, this, updateServer);
    connect(server.get(), &QnMediaServerResource::statusChanged, this, updateServer);
    updateServer();
}

void ServerTimeWatcher::handleResourceRemoved(const QnResourcePtr& resource)
{
    resource->disconnect(this);
}

} // namespace nx::vms::client::core
