#include "workbench_server_time_watcher.h"

#include <utils/common/checked_cast.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

#include <plugins/resource/avi/avi_resource.h>

#include <api/media_server_connection.h>

enum {
    ServerTimeUpdatePeriod = 1000 * 60 * 2, /* 2 minutes. */
};  


QnWorkbenchServerTimeWatcher::QnWorkbenchServerTimeWatcher(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    connect(resourcePool(), SIGNAL(resourceAdded(const QnResourcePtr &)),   this,   SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));
    connect(resourcePool(), SIGNAL(resourceRemoved(const QnResourcePtr &)), this,   SLOT(at_resourcePool_resourceRemoved(const QnResourcePtr &)));

    foreach(const QnResourcePtr &resource, resourcePool()->getResources())
        at_resourcePool_resourceAdded(resource);

    m_timer.start(ServerTimeUpdatePeriod, this);
}

QnWorkbenchServerTimeWatcher::~QnWorkbenchServerTimeWatcher() {
    foreach(const QnResourcePtr &resource, resourcePool()->getResources())
        at_resourcePool_resourceRemoved(resource);

    disconnect(resourcePool(), NULL, this, NULL);
}

qint64 QnWorkbenchServerTimeWatcher::utcOffset(const QnMediaServerResourcePtr &server, qint64 defaultValue) const {
    return m_utcOffsetByResource.value(server, defaultValue);
}

qint64 QnWorkbenchServerTimeWatcher::utcOffset(const QnMediaResourcePtr &resource, qint64 defaultValue) const {
    if(QnAviResourcePtr fileResource = resource.dynamicCast<QnAviResource>()) {
        qint64 result = fileResource->timeZoneOffset();
        return result == Qn::InvalidUtcOffset ? defaultValue : result;
    } else if(QnMediaServerResourcePtr server = resourcePool()->getResourceById(resource->toResource()->getParentId()).dynamicCast<QnMediaServerResource>()) {
        return utcOffset(server, defaultValue);
    } else {
        return defaultValue;
    }
}

qint64 QnWorkbenchServerTimeWatcher::localOffset(const QnMediaServerResourcePtr &server, qint64 defaultValue) const {
    // TODO: #Elric duplicate code.
    qint64 utcOffset = this->utcOffset(server, Qn::InvalidUtcOffset);
    if(utcOffset == Qn::InvalidUtcOffset)
        return defaultValue;

    QDateTime localDateTime = QDateTime::currentDateTime();
    QDateTime utcDateTime = localDateTime.toUTC();
    localDateTime.setTimeSpec(Qt::UTC);

    return utcOffset - utcDateTime.msecsTo(localDateTime);
}

qint64 QnWorkbenchServerTimeWatcher::localOffset(const QnMediaResourcePtr &resource, qint64 defaultValue) const {
    qint64 utcOffset = this->utcOffset(resource, Qn::InvalidUtcOffset);
    if(utcOffset == Qn::InvalidUtcOffset)
        return defaultValue;

    QDateTime localDateTime = QDateTime::currentDateTime();
    QDateTime utcDateTime = localDateTime.toUTC();
    localDateTime.setTimeSpec(Qt::UTC);

    return utcOffset - utcDateTime.msecsTo(localDateTime);
}

void QnWorkbenchServerTimeWatcher::sendRequest(const QnMediaServerResourcePtr &server) {
    if(server->getStatus() == Qn::Offline)
        return;

    int handle = server->apiConnection()->getTimeAsync(this, SLOT(at_replyReceived(int, const QnTimeReply &, int)));
    m_resourceByHandle[handle] = server;
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchServerTimeWatcher::timerEvent(QTimerEvent *event) {
    if(event->timerId() == m_timer.timerId()) {
        foreach(const QnMediaServerResourcePtr &server, resourcePool()->getResources().filtered<QnMediaServerResource>())
            sendRequest(server);
    } else {
        base_type::timerEvent(event);
    }
}

void QnWorkbenchServerTimeWatcher::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if(!server)
        return;

    connect(server.data(), SIGNAL(serverIfFound(const QnMediaServerResourcePtr &, const QString &, const QString &)), this, SLOT(at_server_serverIfFound(const QnMediaServerResourcePtr &)));
    connect(server.data(), SIGNAL(statusChanged(const QnResourcePtr &)), this, SLOT(at_resource_statusChanged(const QnResourcePtr &)));
    sendRequest(server);
}

void QnWorkbenchServerTimeWatcher::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if(!server)
        return;

    m_utcOffsetByResource.remove(server);
    disconnect(server.data(), NULL, this, NULL);
}

void QnWorkbenchServerTimeWatcher::at_server_serverIfFound(const QnMediaServerResourcePtr &resource) {
    sendRequest(resource);
}

void QnWorkbenchServerTimeWatcher::at_resource_statusChanged(const QnResourcePtr &resource) {
    if(QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>())
        sendRequest(server);
}

void QnWorkbenchServerTimeWatcher::at_replyReceived(int status, const QnTimeReply &reply, int handle) {
    Q_UNUSED(status);

    QnMediaServerResourcePtr server = m_resourceByHandle.value(handle);
    m_resourceByHandle.remove(handle);

    if(status == 0) {
        m_utcOffsetByResource[server] = reply.timeZoneOffset;
        emit offsetsChanged();
    }
}

