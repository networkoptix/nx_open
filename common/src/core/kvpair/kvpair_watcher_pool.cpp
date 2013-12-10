#include "kvpair_watcher_pool.h"

#include <api/app_server_connection.h>

#include <api/common_message_processor.h>

QnKvPairWatcherPool::QnKvPairWatcherPool(QObject *parent) :
    QObject(parent)
{
    connect(QnCommonMessageProcessor::instance(), SIGNAL(kvPairsChanged(QnKvPairs)), this, SLOT(at_kvPairsChanged(QnKvPairs)));
    connect(QnCommonMessageProcessor::instance(), SIGNAL(kvPairsDeleted(QnKvPairs)), this, SLOT(at_kvPairsDeleted(QnKvPairs)));
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
    at_kvPairsChanged(kvPairs);
}

void QnKvPairWatcherPool::at_kvPairsChanged(const QnKvPairs &kvPairs) {
    foreach (int resourceId, kvPairs.keys())
        foreach (const QnKvPair &kvPair, kvPairs[resourceId])
            at_kvPair_changed(resourceId, kvPair.name(), kvPair.value());
}

void QnKvPairWatcherPool::at_kvPairsDeleted(const QnKvPairs &kvPairs) {
    foreach (int resourceId, kvPairs.keys())
        foreach (const QnKvPair &kvPair, kvPairs[resourceId])
            at_kvPair_changed(resourceId, kvPair.name(), QString());
}
