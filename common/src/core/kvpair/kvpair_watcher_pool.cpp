#include "kvpair_watcher_pool.h"

QnKvPairWatcherPool::QnKvPairWatcherPool(QObject *parent) :
    QObject(parent)
{
}

QnKvPairWatcherPool::~QnKvPairWatcherPool() {

}

void QnKvPairWatcherPool::registerWatcher(const QString &key, QnAbstractKvPairWatcher *watcher) {
    m_watchersByKey.insert(key, watcher);
}

void QnKvPairWatcherPool::at_kvPair_changed(int resourceId, const QString &key, const QString &value) {
    if (!m_watchersByKey.contains(key))
        return;

    if (value.isEmpty())
        m_watchersByKey[key]->removeValue(resourceId);
    else
        m_watchersByKey[key]->updateValue(resourceId, value);
}

