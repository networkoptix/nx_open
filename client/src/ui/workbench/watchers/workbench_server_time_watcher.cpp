#include "workbench_server_time_watcher.h"

#include <utils/common/checked_cast.h>

#include <core/resource_managment/resource_pool.h>
#include <core/resource/media_server_resource.h>

#include <api/media_server_connection.h>


QnWorkbenchServerTimeWatcher::QnWorkbenchServerTimeWatcher(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    connect(resourcePool(), SIGNAL(resourceAdded(const QnResourcePtr &)),   this,   SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));
    connect(resourcePool(), SIGNAL(resourceRemoved(const QnResourcePtr &)), this,   SLOT(at_resourcePool_resourceRemoved(const QnResourcePtr &)));

    foreach(const QnResourcePtr &resource, resourcePool()->getResources())
        at_resourcePool_resourceAdded(resource);
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

void QnWorkbenchServerTimeWatcher::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if(!server)
        return;

    connect(server.data(), SIGNAL(serverIFFound(const QString &)), this, SLOT(at_server_serverIFFound()));
    at_server_serverIFFound(server);
}

void QnWorkbenchServerTimeWatcher::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if(!server)
        return;

    m_utcOffsetByResource.remove(server);
    disconnect(server.data(), NULL, this, NULL);
}

void QnWorkbenchServerTimeWatcher::at_server_serverIFFound(const QnMediaServerResourcePtr &server) {
    if(server->getPrimaryIF().isEmpty())
        return;

    int handle = server->apiConnection()->asyncGetTime(this, SLOT(at_replyReceived(int, const QDateTime &, int, int)));
    m_resourceByHandle[handle] = server;
}

void QnWorkbenchServerTimeWatcher::at_server_serverIFFound() {
    at_server_serverIFFound(toSharedPointer(checked_cast<QnMediaServerResource *>(sender())));
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

