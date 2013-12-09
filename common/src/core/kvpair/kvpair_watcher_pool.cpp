#include "kvpair_watcher_pool.h"

#include <core/kvpair/ptz_hotkey_kvpair_watcher.h>

QnKvPairWatcherPool::QnKvPairWatcherPool(QObject *parent) :
    QObject(parent)
{
    m_watchersByKey.insert(QLatin1String("ptz_hotkey"), instance<QnPtzHotkeyKvPairWatcher>());

}

QnKvPairWatcherPool::~QnKvPairWatcherPool() {

}

void QnKvPairWatcherPool::at_kvPair_changed(int resourceId, const QString &key, const QString &value) {
    if (!m_watchersByKey.contains(key))
        return;

    if (value.isEmpty())
        m_watchersByKey[key]->removeValue(resourceId);
    else
        m_watchersByKey[key]->updateValue(resourceId, value);
}

