#ifndef KVPAIR_WATCHER_POOL_H
#define KVPAIR_WATCHER_POOL_H

#include <QtCore/QObject>

#include <api/model/kvpair.h>

#include <core/kvpair/abstract_kvpair_watcher.h>

#include <utils/common/instance_storage.h>

class QnKvPairWatcherPool : public QObject, public QnInstanceStorage
{
    Q_OBJECT
public:
    explicit QnKvPairWatcherPool(QObject *parent = 0);
    virtual ~QnKvPairWatcherPool();

    void registerWatcher(QnAbstractKvPairWatcher *watcher);

private slots:
    void at_watcher_valueModified(int resourceId, const QString &value);
    void at_kvPair_changed(int resourceId, const QString &key, const QString &value);

    void at_connection_replyReceived(int status, const QnKvPairs &kvPairs, int handle);

    void at_kvPairsChanged(const QnKvPairs &kvPairs);
    void at_kvPairsDeleted(const QnKvPairs &kvPairs);
private:
    QHash<QString, QnAbstractKvPairWatcher*> m_watchersByKey;
};

#endif // KVPAIR_WATCHER_POOL_H
