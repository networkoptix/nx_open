#include "kvpair_watcher_pool.h"

#include <api/app_server_connection.h>

QnKvPairWatcherPool::QnKvPairWatcherPool(QObject *parent) :
    QObject(parent)
{
}

QnKvPairWatcherPool::~QnKvPairWatcherPool() {

}

void QnKvPairWatcherPool::registerWatcher(QnAbstractKvPairWatcher *watcher) {
    m_watchersByKey.insert(watcher->key(), watcher);
    connect(watcher, SIGNAL(valueModified(int,QString)), this, SLOT(at_watcher_valueModified(int,QString)));
}

void QnKvPairWatcherPool::at_watcher_valueModified(int resourceId, const QString &value) {
    QnAbstractKvPairWatcher* watcher = dynamic_cast<QnAbstractKvPairWatcher*>(sender());
    if (!watcher)
        return;

    int handle = QnAppServerConnectionFactory::createConnection()->saveAsync(
                resourceId,
                QnKvPairList() << QnKvPair(watcher->key(), value),
                this,
                SLOT(at_connection_replyReceived(int, const QnKvPairs &, int)));
}

void QnKvPairWatcherPool::at_kvPair_changed(int resourceId, const QString &key, const QString &value) {
    if (!m_watchersByKey.contains(key))
        return;

    if (value.isEmpty())
        m_watchersByKey[key]->removeValue(resourceId);
    else
        m_watchersByKey[key]->updateValue(resourceId, value);
}

void QnKvPairWatcherPool::at_connection_replyReceived(int status, const QnKvPairs &kvPairs, int handle) {
    if (status != 0)
        return;  // TODO: #GDM notify user about unsuccessful saving?

    foreach (int resourceId, kvPairs.keys()) {
        foreach(const QnKvPair &pair, kvPairs[resourceId]) {
            at_kvPair_changed(resourceId, pair.name(), pair.value());
        }
    }
}
