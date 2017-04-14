#include "workbench_server_time_watcher.h"

#include <api/media_server_connection.h>
#include <api/model/time_reply.h>

#include <client/client_settings.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

#include <plugins/resource/avi/avi_resource.h>

#include <utils/common/synctime.h>
#include <core/resource/fake_media_server.h>

namespace {
    const int serverTimeUpdatePeriodMs = 1000 * 60 * 2; /* 2 minutes. */
};


QnWorkbenchServerTimeWatcher::QnWorkbenchServerTimeWatcher(QObject *parent)
    : base_type(parent)
    , m_timer(new QTimer(this))
{
    connect(resourcePool(), &QnResourcePool::resourceAdded,   this,   &QnWorkbenchServerTimeWatcher::at_resourcePool_resourceAdded);
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,   &QnWorkbenchServerTimeWatcher::at_resourcePool_resourceRemoved);

    /* We need to process only servers. */
    for(const QnResourcePtr &resource: resourcePool()->getAllServers(Qn::AnyStatus))
        at_resourcePool_resourceAdded(resource);

    m_timer->setInterval(serverTimeUpdatePeriodMs);
    m_timer->setSingleShot(false);
    connect(m_timer, &QTimer::timeout, this, [this]
    {
        const auto onlineServers = resourcePool()->getAllServers(Qn::Online);
        for (const QnMediaServerResourcePtr &server: onlineServers)
            sendRequest(server);
    });
    m_timer->start();

    connect(qnSettings->notifier(QnClientSettings::TIME_MODE),  &QnPropertyNotifier::valueChanged,  this,   &QnWorkbenchServerTimeWatcher::displayOffsetsChanged);
    connect(qnSyncTime,                                         &QnSyncTime::timeChanged,           this,   &QnWorkbenchServerTimeWatcher::displayOffsetsChanged);
}

QnWorkbenchServerTimeWatcher::~QnWorkbenchServerTimeWatcher() {
}

qint64 QnWorkbenchServerTimeWatcher::utcOffset(const QnMediaServerResourcePtr &server, qint64 defaultValue) const {
    qint64 result = m_utcOffsetByResource.value(server, Qn::InvalidUtcOffset);
    return result == Qn::InvalidUtcOffset
        ? defaultValue
        : result;
}

qint64 QnWorkbenchServerTimeWatcher::utcOffset(const QnMediaResourcePtr &resource, qint64 defaultValue) const {
    if(QnAviResourcePtr fileResource = resource.dynamicCast<QnAviResource>()) {
        qint64 result = fileResource->timeZoneOffset();
        if (result != Qn::InvalidUtcOffset)
            NX_ASSERT(fileResource->hasFlags(Qn::utc), Q_FUNC_INFO, "Only utc resources should have offset.");
        return result == Qn::InvalidUtcOffset
            ? defaultValue
            : result;
    }

    if (QnMediaServerResourcePtr server = resourcePool()->getResourceById<QnMediaServerResource>(resource->toResource()->getParentId())) {
        NX_ASSERT(resource->toResourcePtr()->hasFlags(Qn::utc), Q_FUNC_INFO, "Only utc resources should have offset.");
        return utcOffset(server, defaultValue);
    }

    return defaultValue;
}

qint64 QnWorkbenchServerTimeWatcher::displayOffset(const QnMediaResourcePtr &resource) const {
    return qnSettings->timeMode() == Qn::ClientTimeMode
         ? 0
         : localOffset(resource, 0);
}


QDateTime QnWorkbenchServerTimeWatcher::serverTime(const QnMediaServerResourcePtr &server, qint64 msecsSinceEpoch) const {
    qint64 utcOffsetMs = utcOffset(server);

    QDateTime result;
    if (utcOffsetMs != Qn::InvalidUtcOffset) {
        result.setTimeSpec(Qt::OffsetFromUTC);
        result.setUtcOffset(utcOffsetMs / 1000);
        result.setMSecsSinceEpoch(msecsSinceEpoch);
    } else {
        result.setTimeSpec(Qt::UTC);
        result.setMSecsSinceEpoch(msecsSinceEpoch);
    }

    return result;
}


QDateTime QnWorkbenchServerTimeWatcher::displayTime(qint64 msecsSinceEpoch) const {
    if (qnSettings->timeMode() == Qn::ClientTimeMode)
        return QDateTime::fromMSecsSinceEpoch(msecsSinceEpoch);

    return serverTime(commonModule()->currentServer(), msecsSinceEpoch);
}


qint64 QnWorkbenchServerTimeWatcher::localOffset(const QnMediaResourcePtr &resource, qint64 defaultValue) const {
    qint64 utcOffsetMs = utcOffset(resource, Qn::InvalidUtcOffset);

    if(utcOffsetMs == Qn::InvalidUtcOffset)
        return defaultValue;

    QDateTime localDateTime = QDateTime::currentDateTime();
    QDateTime utcDateTime = localDateTime.toUTC();
    localDateTime.setTimeSpec(Qt::UTC);

    return utcOffsetMs - utcDateTime.msecsTo(localDateTime);
}

void QnWorkbenchServerTimeWatcher::sendRequest(const QnMediaServerResourcePtr &server) {
    if (server->getStatus() != Qn::Online || server.dynamicCast<QnFakeMediaServerResource>())
        return;

    int handle = server->apiConnection()->getTimeAsync(this, SLOT(at_replyReceived(int, const QnTimeReply &, int)));
    m_resourceByHandle[handle] = server;
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchServerTimeWatcher::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server || server.dynamicCast<QnFakeMediaServerResource>())
        return;

    auto updateServer = [this, server]{ sendRequest(server); };

    connect(server, &QnMediaServerResource::apiUrlChanged, this, updateServer);
    connect(server, &QnMediaServerResource::statusChanged, this, updateServer);
    m_utcOffsetByResource[server] = Qn::InvalidUtcOffset;

    updateServer();
}

void QnWorkbenchServerTimeWatcher::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if(!server)
        return;

    m_utcOffsetByResource.remove(server);
    disconnect(server, NULL, this, NULL);
}

void QnWorkbenchServerTimeWatcher::at_replyReceived(int status, const QnTimeReply &reply, int handle) {
    QnMediaServerResourcePtr server = m_resourceByHandle.take(handle);
    if (!m_utcOffsetByResource.contains(server))
        return;

    if(status == 0) {
        m_utcOffsetByResource[server] = reply.timeZoneOffset;
        emit displayOffsetsChanged();
    }
}
