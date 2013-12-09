#ifndef KVPAIR_WATCHER_POOL_H
#define KVPAIR_WATCHER_POOL_H

#include <QtCore/QObject>

#include <core/kvpair/abstract_kvpair_watcher.h>

#include <utils/common/instance_storage.h>

class QnKvPairWatcherPool : public QObject, public QnInstanceStorage
{
    Q_OBJECT
public:
    explicit QnKvPairWatcherPool(QObject *parent = 0);
    virtual ~QnKvPairWatcherPool();

private slots:
    void at_kvPair_changed(int resourceId, const QString &key, const QString &value);

private:
    QHash<QString, QnAbstractKvPairWatcher*> m_watchersByKey;
};

#endif // KVPAIR_WATCHER_POOL_H
