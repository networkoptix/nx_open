#ifndef INCOMPATIBLE_SERVER_WATCHER_H
#define INCOMPATIBLE_SERVER_WATCHER_H

#include <QtCore/QObject>
#include <core/resource/resource_fwd.h>
#include <utils/common/uuid.h>

struct QnGlobalModuleInformation;

class QnIncompatibleServerWatcher : public QObject {
    Q_OBJECT
public:
    explicit QnIncompatibleServerWatcher(QObject *parent = 0);
    ~QnIncompatibleServerWatcher();

    void start();
    void stop();

private slots:
    void at_peerChanged(const QnGlobalModuleInformation &moduleInformation);
    void at_peerLost(const QnGlobalModuleInformation &moduleInformation);
    void at_resourcePool_resourceChanged(const QnResourcePtr &resource);

private:
    void removeResource(const QnUuid &id);
    QnUuid getFakeId(const QnUuid &realId) const;

private:
    mutable QMutex m_mutex;
    QHash<QnUuid, QnUuid> m_fakeUuidByServerUuid;
    QHash<QnUuid, QnUuid> m_serverUuidByFakeUuid;
};

#endif // INCOMPATIBLE_SERVER_WATCHER_H
