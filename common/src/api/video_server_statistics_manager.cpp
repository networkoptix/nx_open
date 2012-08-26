#include "video_server_statistics_manager.h"

#include <QtCore/QTimer>

#include <api/video_server_statistics_data.h>
#include <api/video_server_statistics_storage.h>

#include <core/resource/video_server_resource.h>

/** Data update period. For the best result should be equal to server's */
#define REQUEST_TIME 2000

QnVideoServerStatisticsManager::QnVideoServerStatisticsManager(QObject *parent):
    QObject(parent)
{
    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(at_timer_timeout()));
    timer->start(REQUEST_TIME);
}

void QnVideoServerStatisticsManager::registerServerWidget(QnVideoServerResourcePtr resource, QObject *target, const char *slot){
    QString id = resource->getUniqueId();
    if (!m_statistics.contains(id))
        m_statistics[id] = new QnStatisticsStorage(this, resource->apiConnection());
    m_statistics[id]->registerServerWidget(target, slot);
}

void QnVideoServerStatisticsManager::unregisterServerWidget(QnVideoServerResourcePtr resource, QObject *target){
    QString id = resource->getUniqueId();
    if (!m_statistics.contains(id))
        return;
    m_statistics[id]->unregisterServerWidget(target);
}

qint64 QnVideoServerStatisticsManager::getHistory(QnVideoServerResourcePtr resource, qint64 lastId, QnStatisticsHistory *history){
    QString id = resource->getUniqueId();
    if (!m_statistics.contains(id))
        return -1;
    return m_statistics[id]->getHistory(lastId, history);
}

void QnVideoServerStatisticsManager::at_timer_timeout(){
    // TODO: #GDM
    // There is a cleaner and shorter way to do this.
    // 
    // foreach(QnStatisticsStorage *storage, m_statistics)
    //   storage->update();
    //   
    // BTW, it's recommended not to use Java-style iterators because their API
    // differs from C++/STL one, they are not widely used in our code (in contrast 
    // to STL-style iterators), and other container libraries normally 
    // don't offer such an API.

    QHashIterator<QString, QnStatisticsStorage *> iter(m_statistics);
    while (iter.hasNext()){
        iter.next();
        iter.value()->update();
    }
}
