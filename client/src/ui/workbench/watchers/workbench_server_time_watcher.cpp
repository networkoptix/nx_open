#include "workbench_server_time_watcher.h"

#include <utils/common/checked_cast.h>

#include <core/resource_managment/resource_pool.h>
#include <core/resource/media_server_resource.h>

#include <api/media_server_connection.h>

enum {
    ServerTimeRequestPeriod = 1000 * 60 * 2, /* 2 minutes. */
};  


QnWorkbenchServerTimeWatcher::QnWorkbenchServerTimeWatcher(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    connect(resourcePool(), SIGNAL(resourceAdded(const QnResourcePtr &)),   this,   SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));
    connect(resourcePool(), SIGNAL(resourceRemoved(const QnResourcePtr &)), this,   SLOT(at_resourcePool_resourceRemoved(const QnResourcePtr &)));

    foreach(const QnResourcePtr &resource, resourcePool()->getResources())
        at_resourcePool_resourceAdded(resource);

    m_timer.start(ServerTimeRequestPeriod, this);
}

QnWorkbenchServerTimeWatcher::~QnWorkbenchServerTimeWatcher() {
    foreach(const QnResourcePtr &resource, resourcePool()->getResources())
        at_resourcePool_resourceRemoved(resource);

    disconnect(resourcePool(), NULL, this, NULL);
}

qint64 QnWorkbenchServerTimeWatcher::utcOffset(const QnMediaServerResourcePtr &server, qint64 defaultValue) const {
    return m_utcOffsetByResource.value(server, defaultValue);
}

qint64 QnWorkbenchServerTimeWatcher::localOffset(const QnMediaServerResourcePtr &server, qint64 defaultValue) const {
    qint64 utcOffset = this->utcOffset(server, Qn::InvalidUtcOffset);
    if(utcOffset == Qn::InvalidUtcOffset)
        return defaultValue;

    QDateTime localDateTime = QDateTime::currentDateTime();
    QDateTime utcDateTime = localDateTime.toUTC();
    localDateTime.setTimeSpec(Qt::UTC);

    return utcOffset - utcDateTime.msecsTo(localDateTime);
}

void QnWorkbenchServerTimeWatcher::updateServerTime(const QnMediaServerResourcePtr &server) {
    if(server->getPrimaryIF().isEmpty())
        return;

    int handle = server->apiConnection()->asyncGetTime(this, SLOT(at_replyReceived(int, const QDateTime &, int, int)));
    m_resourceByHandle[handle] = server;
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchServerTimeWatcher::timerEvent(QTimerEvent *event) {
    if(event->timerId() == m_timer.timerId()) {
        foreach(const QnMediaServerResourcePtr &server, resourcePool()->getResources().filtered<QnMediaServerResource>())
            updateServerTime(server);
    } else {
        base_type::timerEvent(event);
    }
}

void QnWorkbenchServerTimeWatcher::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if(!server)
        return;

    connect(server.data(), SIGNAL(serverIFFound(const QString &)), this, SLOT(at_server_serverIFFound()));
    updateServerTime(server);
}

void QnWorkbenchServerTimeWatcher::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if(!server)
        return;

    m_utcOffsetByResource.remove(server);
    disconnect(server.data(), NULL, this, NULL);
}

void QnWorkbenchServerTimeWatcher::at_server_serverIFFound() {
    updateServerTime(toSharedPointer(checked_cast<QnMediaServerResource *>(sender())));
}

void QnWorkbenchServerTimeWatcher::at_replyReceived(int status, const QDateTime &dateTime, int utcOffset, int handle) {
    Q_UNUSED(status);
    Q_UNUSED(dateTime);

    QnMediaServerResourcePtr server = m_resourceByHandle.value(handle);
    m_resourceByHandle.remove(handle);

    if(dateTime.isValid()) {
        m_utcOffsetByResource[server] = utcOffset * 1000ll; /* Convert seconds to milliseconds. */
        emit offsetsChanged();
    }
}

